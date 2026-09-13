/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "blockphysics.h"
#include "world.h"
#include "../blockstates.h"
#include "../lighting.h"
#include "../metadatainternal.h"
#include "../networkhandler.h"
#include "../packet.h"
#include "../scripthooks.h"
#include "stb_ds.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct PhysicsUpdate {
    Chunk *chunk;
    int index;
    double due;
} PhysicsUpdate;
static BlockPhysicsDefinition definitions[256];
static PhysicsUpdate *queue;
static Chunk **changedChunks;
static double simulationTime;
static bool batching;
static const Vector3 neighbors[6] = {{0, -1, 0}, {0, 1, 0},  {-1, 0, 0},
                                     {1, 0, 0},  {0, 0, -1}, {0, 0, 1}};

static Vector3 Offset(Vector3 a, Vector3 b) {
    return (Vector3){a.x + b.x, a.y + b.y, a.z + b.z};
}
static Vector3 Position(Chunk *chunk, int index) {
    return Offset(chunk->blockPosition, ServerChunk_IndexToPos(index));
}
Chunk *ServerBlockPhysics_Find(Vector3 p, int *index) {
    if (!isfinite(p.x) || !isfinite(p.y) || !isfinite(p.z) || fabsf(p.x) > 33554430 ||
        fabsf(p.y) > 33554430 || fabsf(p.z) > 33554430)
        return NULL;
    Chunk *chunk =
        ServerWorld_GetChunkAt((Vector3){floorf(p.x / 16), floorf(p.y / 16), floorf(p.z / 16)});
    if (!chunk || chunk->loadFailed)
        return NULL;
    *index = ServerChunk_PosToIndex((Vector3){floorf(p.x) - chunk->blockPosition.x,
                                              floorf(p.y) - chunk->blockPosition.y,
                                              floorf(p.z) - chunk->blockPosition.z});
    return chunk;
}

// Heap slots are indexed by cell, so repeated neighbor notifications are O(1).
static void Put(int slot, PhysicsUpdate update) {
    queue[slot] = update;
    update.chunk->physicsSlots[update.index] = slot + 1;
}
static void Up(int slot) {
    PhysicsUpdate update = queue[slot];
    while (slot > 0) {
        int parent = (slot - 1) / 2;
        if (queue[parent].due <= update.due)
            break;
        Put(slot, queue[parent]);
        slot = parent;
    }
    Put(slot, update);
}
static void Remove(int slot) {
    PhysicsUpdate removed = queue[slot], last = arrpop(queue);
    removed.chunk->physicsSlots[removed.index] = 0;
    if (slot >= arrlen(queue))
        return;
    Put(slot, last);
    if (slot > 0 && last.due < queue[(slot - 1) / 2].due) {
        Up(slot);
        return;
    }
    while (slot * 2 + 1 < arrlen(queue)) {
        int child = slot * 2 + 1;
        if (child + 1 < arrlen(queue) && queue[child + 1].due < queue[child].due)
            child++;
        if (queue[child].due >= last.due)
            break;
        Put(slot, queue[child]);
        slot = child;
    }
    Put(slot, last);
}
static bool Schedule(Chunk *chunk, int index, float delay) {
    if (chunk->data[index] >= 256 || !definitions[chunk->data[index]].type)
        return false;
    if (!chunk->physicsSlots) {
        chunk->physicsSlots = calloc(CHUNK_SIZE, sizeof(*chunk->physicsSlots));
        if (!chunk->physicsSlots)
            return false;
    }
    // Always advance at least one simulation interval; callbacks cannot recurse.
    double due = simulationTime + fmaxf(delay, 0.05f);
    int slot = chunk->physicsSlots[index] - 1;
    if (slot >= 0) {
        if (due < queue[slot].due) {
            queue[slot].due = due;
            Up(slot);
        }
        return true;
    }
    PhysicsUpdate update = {chunk, index, due};
    arrput(queue, update);
    slot = arrlen(queue) - 1;
    Put(slot, update);
    Up(slot);
    return true;
}
bool ServerBlockPhysics_Schedule(Vector3 p, float delay) {
    int index;
    Chunk *chunk = ServerBlockPhysics_Find(p, &index);
    return chunk && isfinite(delay) && delay >= 0 && Schedule(chunk, index, delay);
}
void ServerBlockPhysics_Cancel(Chunk *chunk, int index) {
    if (chunk->physicsSlots && chunk->physicsSlots[index])
        Remove(chunk->physicsSlots[index] - 1);
}
static void Wake(Vector3 p) {
    int index;
    Chunk *chunk = ServerBlockPhysics_Find(p, &index);
    if (chunk && chunk->data[index] < 256)
        Schedule(chunk, index, definitions[chunk->data[index]].interval);
}
void ServerBlockPhysics_Changed(Vector3 p) {
    Wake(p);
    for (int side = 0; side < 6; side++)
        Wake(Offset(p, neighbors[side]));
}
static int NeighborBlock(Chunk *chunk, int index, int side) {
    Vector3 local = Offset(ServerChunk_IndexToPos(index), neighbors[side]);
    if (ServerChunk_IsValidPos(local))
        return chunk->data[ServerChunk_PosToIndex(local)];
    int next;
    Chunk *neighbor = ServerBlockPhysics_Find(Offset(chunk->blockPosition, local), &next);
    return neighbor ? neighbor->data[next] : -1;
}
static void Activate(Chunk *chunk, int index) {
    int id = chunk->data[index];
    if (id >= 256 || !definitions[id].type)
        return;
    BlockPhysicsDefinition *definition = &definitions[id];
    // Terrain starts mostly settled. Only put the nescessary cells into the heap.
    if (!definition->callback && definition->type == BLOCK_PHYSICS_FALLING) {
        int below = NeighborBlock(chunk, index, 0);
        if (below < 0 || below >= 256 || below == id || !definition->replaceable[below])
            return;
    } else if (!definition->callback && definition->type == BLOCK_PHYSICS_FLUID) {
        Metadata *metadata = ChunkMetadata_Get(chunk, index);
        bool source = !metadata || !metadata->size || (metadata->data[0] & 1);
        if (source) {
            bool exposed = false;
            for (int side = 0; side < 6; side++) {
                if (side == 1)
                    continue; // Fluids never spread upward.
                int next = NeighborBlock(chunk, index, side);
                if (next >= 0 && next < 256 && next != id && definition->replaceable[next]) {
                    exposed = true;
                    break;
                }
            }
            if (!exposed)
                return;
        }
    }
    Schedule(chunk, index, definition->interval);
}
void ServerBlockPhysics_Loaded(Chunk *chunk) {
    // One scan on activation reconstructs work after save/load; no idle-world scans.
    for (int i = 0; i < CHUNK_SIZE; i++)
        Activate(chunk, i);
    for (int side = 0; side < 6; side++) {
        Vector3 direction = neighbors[side];
        for (int a = 0; a < 16; a++)
            for (int b = 0; b < 16; b++) {
                Vector3 local;
                if (direction.x)
                    local = (Vector3){direction.x < 0 ? -1 : 16, a, b};
                else if (direction.y)
                    local = (Vector3){a, direction.y < 0 ? -1 : 16, b};
                else
                    local = (Vector3){a, b, direction.z < 0 ? -1 : 16};
                int index;
                Chunk *neighbor =
                    ServerBlockPhysics_Find(Offset(chunk->blockPosition, local), &index);
                if (neighbor)
                    Activate(neighbor, index);
            }
    }
}
void ServerBlockPhysics_Forget(Chunk *chunk) {
    if (!chunk->physicsSlots)
        return; // Worker-created chunks were never scheduled.
    for (int i = 0; i < CHUNK_SIZE; i++)
        if (chunk->physicsSlots[i])
            Remove(chunk->physicsSlots[i] - 1);
    free(chunk->physicsSlots);
    chunk->physicsSlots = NULL;
}
void ServerBlockPhysics_Define(int id, const BlockPhysicsDefinition *definition) {
    if (id > 0 && id < 256)
        definitions[id] = *definition;
}
void ServerBlockPhysics_Reset(void) {
    while (arrlen(queue))
        Remove(arrlen(queue) - 1);
    arrfree(queue);
    arrfree(changedChunks);
    memset(definitions, 0, sizeof(definitions));
    simulationTime = 0;
    batching = false;
}
int ServerBlockPhysics_Pending(void) {
    return arrlen(queue);
}

bool ServerBlockPhysics_Defer(Chunk *chunk, int index) {
    if (!batching)
        return false;
    if (!chunk->physicsChanged) {
        chunk->physicsChanged = true;
        arrput(changedChunks, chunk);
    }
    chunk->physicsDirty[index / 8] |= 1u << (index % 8);
    return true;
}
static void Flush(void) {
    ServerBlockUpdate updates[CHUNK_SIZE];
    for (int c = 0; c < arrlen(changedChunks); c++) {
        Chunk *chunk = changedChunks[c];
        int count = 0;
        for (int byte = 0; byte < CHUNK_SIZE / 8; byte++) {
            if (!chunk->physicsDirty[byte])
                continue;
            for (int bit = 0; bit < 8; bit++)
                if (chunk->physicsDirty[byte] & (1u << bit)) {
                    int index = byte * 8 + bit;
                    updates[count++] =
                        (ServerBlockUpdate){Position(chunk, index), chunk->data[index]};
                }
        }
        for (int p = 0; p < arrlen(chunk->players); p++) {
            Player *player = chunk->players[p];
            if (!player->disconnected)
                ServerNetwork_Send(player, ServerPacket_CreateBlockBatch(updates, count));
        }
        memset(chunk->physicsDirty, 0, sizeof(chunk->physicsDirty));
        chunk->physicsChanged = false;
    }
    arrsetlen(changedChunks, 0);
}

bool ServerBlockPhysics_Move(Vector3 from, Vector3 to) {
    int first, second;
    Chunk *a = ServerBlockPhysics_Find(from, &first), *b = ServerBlockPhysics_Find(to, &second);
    if (!a || !b || a->savePending || b->savePending || (a == b && first == second))
        return false;
    int id = a->data[first], previous = b->data[second];
    if (!id || id >= 256 || previous >= 256 || id == previous || !definitions[id].type ||
        !definitions[id].replaceable[previous])
        return false;
    // Reserve/copy the payload before changing IDs. Neither callback sees half a move.
    Metadata *metadata = ChunkMetadata_Get(a, first), copy = {0};
    if (metadata && !Metadata_Copy(&copy, metadata))
        return false;
    if (copy.size && !ChunkMetadata_Reserve(b)) {
        Metadata_Free(&copy);
        return false;
    }
    bool outer = batching;
    batching = true;
    ServerWorld_SetBlock(from, 0, true, false, false);
    ServerWorld_SetBlock(to, id, true, false, false);
    ChunkMetadata_Assign(b, second, &copy);
    b->states[second] = ServerBlockStates_Resolve(b, second);
    ScriptHooks_BlockUpdate(from, 0, id);
    ScriptHooks_BlockUpdate(to, id, previous);
    if (!outer) {
        Flush();
        batching = false;
    }
    return true;
}

bool ServerBlockPhysics_Place(Vector3 p, int id, Metadata *metadata) {
    int index;
    Chunk *chunk = ServerBlockPhysics_Find(p, &index);
    if (!chunk || chunk->savePending || !ServerWorld_IsBlockDefined(id))
        return false;
    if (metadata->size && !ChunkMetadata_Reserve(chunk))
        return false;
    int previous = chunk->data[index];
    int previousState = ServerBlockStates_Resolve(chunk, index);
    ServerWorld_SetBlock(p, id, false, false, false);
    ChunkMetadata_Assign(chunk, index, metadata);
    chunk->states[index] = ServerBlockStates_Resolve(chunk, index);
    if (previousState != chunk->states[index])
        ServerLighting_Changed(chunk);
    if (!ServerBlockPhysics_Defer(chunk, index))
        ServerWorld_Broadcast(ServerPacket_CreateSetBlock(id, p, false));
    ServerBlockPhysics_Changed(p);
    if (previous != id)
        ScriptHooks_BlockUpdate(p, id, previous);
    return true;
}

// Fluid fields occupy the first six metadata bits; custom fields follow them.
bool ServerBlockPhysics_GetFluid(Vector3 p, FluidState *state) {
    int index;
    Chunk *chunk = ServerBlockPhysics_Find(p, &index);
    if (!chunk || chunk->data[index] >= 256 ||
        definitions[chunk->data[index]].type != BLOCK_PHYSICS_FLUID)
        return false;
    int schemaId = serverBlockSchemas[chunk->data[index]];
    Metadata *value = ChunkMetadata_Get(chunk, index);
    MetadataSchema *schema = serverMetadataSchemas[schemaId];
    if (value && value->size && value->version != schema->version)
        return false;
    unsigned char bits = value && value->size ? value->data[0] : schema->defaults.data[0];
    *state = (FluidState){
        .source = (bits & 1) != 0, .falling = (bits & 32) != 0, .level = (bits >> 1) & 15};
    return true;
}
bool ServerBlockPhysics_SetFluid(Vector3 p, FluidState state) {
    int index;
    Chunk *chunk = ServerBlockPhysics_Find(p, &index);
    if (!chunk || chunk->savePending)
        return false;
    int id = chunk->data[index];
    if (id >= 256 || definitions[id].type != BLOCK_PHYSICS_FLUID || state.level < 0 ||
        state.level > definitions[id].maxLevel)
        return false;
    MetadataSchema *schema = serverMetadataSchemas[serverBlockSchemas[id]];
    Metadata *value = ChunkMetadata_Get(chunk, index);
    if (value && value->size && value->version != schema->version)
        return false;
    unsigned char bits = state.source | (state.level << 1) | (state.falling << 5);
    unsigned char previous = value && value->size ? value->data[0] : schema->defaults.data[0];
    if ((previous & 63) == bits)
        return true;
    if (!value || !value->size) {
        if (!ChunkMetadata_Set(chunk, index, &schema->defaults))
            return false;
        value = ChunkMetadata_Get(chunk, index);
    }
    chunk->states[index] = ServerBlockStates_Resolve(chunk, index);
    value->data[0] = (value->data[0] & 192) | bits;
    if (value->size == schema->defaults.size &&
        !memcmp(value->data, schema->defaults.data, value->size))
        ChunkMetadata_Clear(chunk, index);
    ServerBlockStates_Changed(chunk, index);
    ServerBlockPhysics_Changed(p);
    return true;
}
bool ServerBlockPhysics_FluidModels(int id, BlockDefinition *base) {
    StateSet *set = calloc(1, sizeof(*set));
    if (!set)
        return false;
    int levels = definitions[id].maxLevel;
    set->fieldCount = 3;
    strcpy(set->fields[0].name, "source");
    set->fields[0].boolean = true;
    strcpy(set->fields[1].name, "level");
    set->fields[1].bits = 4;
    strcpy(set->fields[2].name, "falling");
    set->fields[2].boolean = true;
    set->count = levels + 2;
    for (int i = 0; i < set->count; i++) {
        BlockDefinition d = *base;
        memset(d.min, 0, 3);
        memset(d.max, 16, 3);
        if (i > 0 && i <= levels)
            d.max[1] = 16 * (levels + 1 - i) / (levels + 1);
        // A single bounds box uses the existing fast model path.
        d.geometry = (BlockGeometry){0};
        set->states[i] = d;
    }
    set->rules[set->ruleCount++] =
        (StateRule){.mask = 1, .values = {1, 0, 0}, .state = 0, .rotationField = -1};
    set->rules[set->ruleCount++] =
        (StateRule){.mask = 4, .values = {0, 0, 1}, .state = levels + 1, .rotationField = -1};
    for (int i = 1; i <= levels; i++)
        set->rules[set->ruleCount++] =
            (StateRule){.mask = 2, .values = {0, i, 0}, .state = i, .rotationField = -1};
    bool ok = ServerBlockStates_Define(id, set, base);
    free(set);
    return ok;
}

static int Read(Vector3 p) {
    int index;
    Chunk *chunk = ServerBlockPhysics_Find(p, &index);
    return chunk && !chunk->savePending ? chunk->data[index] : -1;
}
static bool Replaceable(int id, int target) {
    return target >= 0 && target < 256 && target != id && definitions[id].replaceable[target];
}
static bool CanDrop(int id, Vector3 p) {
    Vector3 position = Offset(p, neighbors[0]);
    int below = Read(position);
    FluidState state;
    if (below == id && ServerBlockPhysics_GetFluid(position, &state))
        return !state.source;
    return Replaceable(id, below);
}
static void Fill(Vector3 p, int id, FluidState state) {
    int previous = Read(p);
    if (!Replaceable(id, previous))
        return;
    Metadata value = {0};
    MetadataSchema *schema = serverMetadataSchemas[serverBlockSchemas[id]];
    if (!Metadata_Copy(&value, &schema->defaults))
        return;
    value.data[0] =
        (value.data[0] & 192) | state.source | (state.level << 1) | (state.falling << 5);
    ServerBlockPhysics_Place(p, id, &value);
    Metadata_Free(&value);
}
static void Fluid(Vector3 p, int id) {
    FluidState state;
    if (!ServerBlockPhysics_GetFluid(p, &state))
        return;
    BlockPhysicsDefinition *definition = &definitions[id];
    int below = Read(Offset(p, neighbors[0]));
    if (!state.source) {
        int above = Read(Offset(p, neighbors[1]));
        // Preserve uncertain supply at an unloaded border until that chunk activates.
        if (above < 0 || below < 0)
            return;
        FluidState next = {.level = definition->maxLevel + 1};
        int sources = 0;
        if (above == id)
            next = (FluidState){.falling = true};
        else
            for (int side = 2; side < 6; side++) {
                Vector3 neighbor = Offset(p, neighbors[side]);
                int other = Read(neighbor);
                if (other < 0)
                    return;
                FluidState supply;
                if (other != id || !ServerBlockPhysics_GetFluid(neighbor, &supply))
                    continue;
                if (supply.source)
                    sources++;
                if (!supply.source && CanDrop(id, neighbor))
                    continue;
                int level = (supply.source || supply.falling ? 0 : supply.level) + 1;
                if (level < next.level)
                    next.level = level;
            }
        if (definition->renewableSources && sources >= 2 && below != id && !Replaceable(id, below))
            next = (FluidState){.source = true};
        if (next.level > definition->maxLevel) {
            ServerWorld_SetBlock(p, 0, true, false, true);
            return;
        }
        if (!ServerBlockPhysics_SetFluid(p, next))
            return;
        state = next;
    }
    if (below < 0)
        return;
    if (Replaceable(id, below)) {
        Fill(Offset(p, neighbors[0]), id, (FluidState){.falling = true});
        return;
    }
    if (below == id && CanDrop(id, p))
        return;
    int level = (state.source || state.falling ? 0 : state.level) + 1;
    if (level > definition->maxLevel)
        return;
    for (int side = 2; side < 6; side++)
        Fill(Offset(p, neighbors[side]), id, (FluidState){.level = level});
}
int ServerBlockPhysics_Update(float dt) {
    if (!isfinite(dt) || dt <= 0)
        return 0;
    simulationTime += dt;
    batching = true;
    int processed = 0;
    double deadline = GetTime() + BLOCK_PHYSICS_SECONDS;
    // Due work carries over without changing its deadline when the budget is exhausted.
    while (processed < BLOCK_PHYSICS_BUDGET && arrlen(queue) && queue[0].due <= simulationTime) {
        if (processed && processed % 64 == 0 && GetTime() >= deadline)
            break;
        PhysicsUpdate update = queue[0];
        Remove(0);
        processed++;
        Chunk *chunk = update.chunk;
        int id = chunk->data[update.index];
        if (id >= 256)
            continue;
        BlockPhysicsDefinition *definition = &definitions[id];
        if (!definition->type)
            continue;
        if (chunk->savePending) {
            Schedule(chunk, update.index, definition->interval);
            continue;
        }
        Vector3 p = Position(chunk, update.index);
        bool saving = false;
        for (int side = 0; side < 6; side++) {
            int index;
            Chunk *neighbor = ServerBlockPhysics_Find(Offset(p, neighbors[side]), &index);
            if (neighbor && neighbor->savePending)
                saving = true;
        }
        if (saving) {
            Schedule(chunk, update.index, definition->interval);
            continue;
        }
        if (definition->callback)
            ScriptHooks_BlockPhysics(p, id);
        else if (definition->type == BLOCK_PHYSICS_FALLING)
            ServerBlockPhysics_Move(p, Offset(p, neighbors[0]));
        else if (definition->type == BLOCK_PHYSICS_FLUID)
            Fluid(p, id);
    }
    Flush();
    batching = false;
    return processed;
}
