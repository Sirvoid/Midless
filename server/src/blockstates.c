/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "lighting.h"
#include "blockstates.h"
#include "metadata.h"
#include "world/world.h"
#include "world/chunk/chunk.h"
#include "packet.h"
#include "networkhandler.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
static StateSet *sets[256];
void ServerBlockStates_UpdateBounds(BlockDefinition *d) {
    BlockGeometry *g = &d->geometry;
    if (!g->enabled || !g->boxCount)
        return;
    BlockBox bounds = BlockBox_Rotate(g->boxes[0].bounds, g->rotation);
    for (int i = 1; i < g->boxCount; i++) {
        BlockBox b = BlockBox_Rotate(g->boxes[i].bounds, g->rotation);
        for (int a = 0; a < 3; a++) {
            if (b.min[a] < bounds.min[a])
                bounds.min[a] = b.min[a];
            if (b.max[a] > bounds.max[a])
                bounds.max[a] = b.max[a];
        }
    }
    memcpy(d->min, bounds.min, 3);
    memcpy(d->max, bounds.max, 3);
}
static int Resolve(StateSet *set, int id, const Metadata *metadata) {
    int64_t values[BLOCK_STATE_MAX_FIELDS] = {0};
    for (int i = 0; i < set->fieldCount; i++)
        ServerMetadata_StateValue(id, metadata, set->fields[i].name, &values[i]);
    for (int i = 0; i < set->ruleCount; i++) {
        StateRule *r = &set->rules[i];
        bool matches = true;
        for (int f = 0; f < set->fieldCount; f++)
            if ((r->mask & (1 << f)) && r->values[f] != values[f])
                matches = false;
        if (matches) {
            int64_t rotation = r->rotationField < 0 ? 0 : values[r->rotationField];
            return r->state + (rotation >= 0 && rotation < 4 ? rotation : 0);
        }
    }
    return 0;
}
void ServerBlockStates_Remove(int id) {
    if (id > 0 && id < 256) {
        free(sets[id]);
        sets[id] = NULL;
    }
}
void ServerBlockStates_Reset(void) {
    for (int i = 1; i < 256; i++)
        ServerBlockStates_Remove(i);
}
int ServerBlockStates_Count(int id) {
    return id > 0 && id < 256 && sets[id] ? sets[id]->count : 1;
}
const BlockDefinition *ServerBlockStates_Definition(int id, int state) {
    if (id < 1 || id > 255)
        return NULL;
    if (sets[id] && state >= 0 && state < sets[id]->count)
        return &sets[id]->states[state];
    return serverWorld.hasBlockDefinition[id] ? &serverWorld.blockDefinitions[id] : NULL;
}
int ServerBlockStates_Resolve(Chunk *chunk, int index) {
    int id = chunk->data[index];
    return id > 0 && id < 256 && sets[id] ? Resolve(sets[id], id, ChunkMetadata_Get(chunk, index))
                                          : 0;
}
int ServerBlockStates_WireId(int id, Vector3 p) {
    if (id < 1 || id > 255 || !sets[id])
        return id;
    Chunk *c =
        ServerWorld_GetChunkAt((Vector3){floorf(p.x / 16), floorf(p.y / 16), floorf(p.z / 16)});
    if (!c)
        return id;
    Vector3 local = {floorf(p.x) - c->blockPosition.x, floorf(p.y) - c->blockPosition.y,
                     floorf(p.z) - c->blockPosition.z};
    int index = ServerChunk_PosToIndex(local);
    return id | ((c->data[index] == id ? ServerBlockStates_Resolve(c, index)
                                       : Resolve(sets[id], id, NULL))
                 << 8);
}
void ServerBlockStates_Changed(Chunk *c, int index) {
    int state = ServerBlockStates_Resolve(c, index);
    if (c->states[index] == state)
        return;
    c->states[index] = state;
    ServerLighting_Changed(c);
    Vector3 local = ServerChunk_IndexToPos(index),
            p = {c->blockPosition.x + local.x, c->blockPosition.y + local.y,
                 c->blockPosition.z + local.z};
    if (!serverWorld.players)
        return;
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *player = serverWorld.players[i];
        if (player && !player->disconnected && ServerChunk_PlayerInChunk(c, player))
            ServerNetwork_Send(player, ServerPacket_CreateSetBlock(c->data[index], p, false));
    }
}
BlockShape ServerBlockStates_Shape(int id, Vector3 p) {
    int wire = ServerBlockStates_WireId(id, p);
    return BlockShape_Get(id, ServerBlockStates_Definition(id, wire >> 8), p);
}

bool ServerBlockStates_Define(int id, const StateSet *definition, BlockDefinition *base) {
    if (id < 1 || id > 255)
        return false;
    StateSet *copy = malloc(sizeof(*copy));
    if (!copy)
        return false;
    *copy = *definition;
    int defaultState = Resolve(copy, id, NULL);
    copy->states[0] = copy->states[defaultState];
    *base = copy->states[0];
    free(sets[id]);
    sets[id] = copy;
    return true;
}
