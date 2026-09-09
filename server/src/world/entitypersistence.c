#include "entitypersistence.h"
#include "world.h"
#include "../scripting/luaentities.h"
#include "../scripting/luametadata.h"
#include "../entityphysics.h"
#include "../droppeditems.h"
#include "../packet.h"
#include "../items.h"
#include "binarydata.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SAVED_ENTITIES WORLD_MAX_ENTITIES
typedef struct SavedEntity {
    char name[65]; // Empty name denotes an engine dropped item.
    Vector3 position, rotation;
    EntityBody body;
    ItemStack stack;
    float age, pickupDelay;
    Metadata metadata;
    char model[65];
    uint16_t heldBlock;
} SavedEntity;

static bool ChunkEntity(const Entity *entity) {
    return entity->active && entity->ownerPlayerId < 0 &&
        (entity->definitionId >= 0 || entity->type == ENTITY_TYPE_DROPPED_ITEM);
}
static bool ShouldSave(const Entity *entity) {
    return ChunkEntity(entity) && !entity->pendingRemoval &&
        (entity->type == ENTITY_TYPE_DROPPED_ITEM || LuaEntities_ShouldSave(entity->definitionId));
}
static Vector3 ChunkPosition(Vector3 position) {
    return (Vector3){floorf(position.x / 16), floorf(position.y / 16), floorf(position.z / 16)};
}
static bool SamePosition(Vector3 a, Vector3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
static bool Belongs(const Entity *entity, const Chunk *chunk) {
    return ChunkEntity(entity) && SamePosition(ChunkPosition(entity->position), chunk->position);
}
void EntityPersistence_EnsureChunks(void) {
    if (!serverWorld.entities) return;
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        Entity *entity = &serverWorld.entities[i];
        if (!ChunkEntity(entity) || entity->pendingRemoval) continue;
        // Scripts can teleport into an unloaded chunk. Load it before the
        // unload pass, so its existing contents join the current live contents.
        Vector3 position = ChunkPosition(entity->position);
        if (!ServerWorld_GetChunkAt(position)) ServerWorld_AddChunk(position);
    }
}
static void WriteVector(BinaryWriter *out, Vector3 value) {
    Binary_Float(out, value.x); Binary_Float(out, value.y); Binary_Float(out, value.z);
}
static Vector3 ReadVector(BinaryReader *in) {
    Vector3 value;
    value.x = Binary_ReadFloat(in); value.y = Binary_ReadFloat(in); value.z = Binary_ReadFloat(in);
    if (!isfinite(value.x) || !isfinite(value.y) || !isfinite(value.z)) in->failed = true;
    return value;
}
static void WriteName(BinaryWriter *out, const char *name) {
    size_t length = strlen(name); Binary_U8(out, length); Binary_Write(out, name, length);
}
static bool ReadName(BinaryReader *in, char name[65]) {
    int length = Binary_ReadU8(in);
    const uint8_t *bytes = Binary_Read(in, length);
    if (in->failed || length > 64 || (length && memchr(bytes, 0, length))) return false;
    if (length) memcpy(name, bytes, length);
    name[length] = 0;
    return true;
}
static void FreeRecords(SavedEntity *records, int count) {
    if (records) for (int i = 0; i < count; i++) Metadata_Free(&records[i].metadata);
    free(records);
}
static bool ReadRecords(const Chunk *chunk, SavedEntity **records, int *count) {
    *records = NULL; *count = 0;
    if (!chunk->savedEntitiesSize) return true;
    BinaryReader in = {chunk->savedEntities, chunk->savedEntitiesSize};
    char names[MAX_SAVED_ENTITIES][65];
    uint32_t types = Binary_ReadVarUInt(&in);
    if (types > MAX_SAVED_ENTITIES) return false;
    for (unsigned i = 0; i < types; i++) if (!ReadName(&in, names[i])) return false;
    uint32_t total = Binary_ReadVarUInt(&in);
    if (in.failed || total > MAX_SAVED_ENTITIES) return false;
    SavedEntity *saved = calloc(total ? total : 1, sizeof(*saved));
    if (!saved) return false;
    *records = saved; *count = total;
    for (unsigned i = 0; i < total; i++) {
        SavedEntity *e = &saved[i];
        uint32_t type = Binary_ReadVarUInt(&in), length = Binary_ReadVarUInt(&in);
        const uint8_t *bytes = Binary_Read(&in, length);
        if (in.failed || type >= types) return false;
        strcpy(e->name, names[type]);
        BinaryReader record = {bytes, length};
        e->position = ReadVector(&record); e->rotation = ReadVector(&record);
        e->body.enabled = Binary_ReadU8(&record);
        e->body.velocity = ReadVector(&record);
        e->body.localBounds.min = ReadVector(&record); e->body.localBounds.max = ReadVector(&record);
        e->body.gravityScale = Binary_ReadFloat(&record);
        e->body.groundFriction = Binary_ReadFloat(&record); e->body.restitution = Binary_ReadFloat(&record);
        if (!ReadName(&record, e->model)) return false;
        e->heldBlock = Binary_ReadU8(&record);
        e->metadata.version = Binary_ReadU16(&record);
        uint32_t size = Binary_ReadVarUInt(&record);
        const uint8_t *payload = Binary_Read(&record, size);
        if (record.failed || size > 65535 || (size && !e->metadata.version)) return false;
        Metadata value = {(uint8_t *)payload, size, e->metadata.version};
        if (!Metadata_Copy(&e->metadata, &value)) return false;
        if (!e->name[0]) {
            e->stack = ItemStack_Read(&record);
            e->age = Binary_ReadFloat(&record); e->pickupDelay = Binary_ReadFloat(&record);
            if (!e->stack.itemId || !e->stack.count || e->stack.count > Item_GetMaxStack(e->stack.itemId) ||
                !isfinite(e->age) || e->age < 0 || !isfinite(e->pickupDelay) || e->pickupDelay < 0) return false;
        }
        if (record.offset + 2 == record.size) e->heldBlock = Binary_ReadU16(&record);
        if (!Binary_End(&record) || !SamePosition(ChunkPosition(e->position), chunk->position) ||
            !EntityBody_Validate(&e->body)) return false;
    }
    return Binary_End(&in);
}
static void WriteRecord(BinaryWriter *out, int type, const SavedEntity *e) {
    BinaryWriter record = {0};
    WriteVector(&record, e->position); WriteVector(&record, e->rotation);
    Binary_U8(&record, e->body.enabled); WriteVector(&record, e->body.velocity);
    WriteVector(&record, e->body.localBounds.min); WriteVector(&record, e->body.localBounds.max);
    Binary_Float(&record, e->body.gravityScale); Binary_Float(&record, e->body.groundFriction); Binary_Float(&record, e->body.restitution);
    WriteName(&record, e->model); Binary_U8(&record, e->heldBlock);
    Binary_U16(&record, e->metadata.version); Binary_VarUInt(&record, e->metadata.size);
    Binary_Write(&record, e->metadata.data, e->metadata.size);
    if (!e->name[0]) {
        ItemStack_Write(&record, e->stack);
        Binary_Float(&record, e->age); Binary_Float(&record, e->pickupDelay);
    }
    if (e->heldBlock > 255) Binary_U16(&record, e->heldBlock);
    if (record.failed) out->failed = true;
    Binary_VarUInt(out, type); Binary_VarUInt(out, record.size); Binary_Write(out, record.data, record.size);
    free(record.data);
}
static int TypeIndex(const char **names, int *count, const char *name) {
    for (int i = 0; i < *count; i++) if (!strcmp(names[i], name)) return i;
    names[*count] = name;
    return (*count)++;
}
static void WriteHeader(BinaryWriter *out, const char **names, int types, int count) {
    Binary_VarUInt(out, types);
    for (int i = 0; i < types; i++) WriteName(out, names[i]);
    Binary_VarUInt(out, count);
}
static bool WriteRecords(BinaryWriter *out, const SavedEntity *records, int count) {
    if (!count) return true;
    const char *names[MAX_SAVED_ENTITIES];
    int types[MAX_SAVED_ENTITIES], typeCount = 0;
    for (int i = 0; i < count; i++) types[i] = TypeIndex(names, &typeCount, records[i].name);
    WriteHeader(out, names, typeCount, count);
    for (int i = 0; i < count; i++) WriteRecord(out, types[i], &records[i]);
    return !out->failed;
}
static int ModelId(const char *name) {
    if (!name[0] || !strcmp(name, "humanoid")) return 0;
    for (int i = 1; i < 256; i++) if (serverWorld.modelDefinitions[i] && !strcmp(name, serverWorld.modelNames[i])) return i;
    return -1;
}
static bool Available(const SavedEntity *e) {
    int definition = LuaEntities_Find(e->name);
    return (!e->name[0] ? ServerItems_IsDefined(e->stack.itemId) :
        definition >= 0 && LuaMetadata_CanRead(LuaEntities_MetadataSchema(definition), &e->metadata)) && ModelId(e->model) >= 0;
}
static bool SaveDisabled(const SavedEntity *entity) {
    int definition = LuaEntities_Find(entity->name);
    return definition >= 0 && !LuaEntities_ShouldSave(definition);
}
bool EntityPersistence_Activate(Chunk *chunk) {
    // The chunk map is the authority: a second load of the same position cannot
    // instantiate another copy, even when disk reads finished asynchronously.
    if (ServerWorld_GetChunkAt(chunk->position) != chunk) return false;
    if (chunk->entitiesActivated) return true;
    if (!LuaMetadata_ValidateChunk(chunk)) return false;
    if (!chunk->savedEntitiesSize) { chunk->entitiesActivated = true; return true; }
    SavedEntity *records = NULL; int count = 0;
    if (!ReadRecords(chunk, &records, &count)) { FreeRecords(records, count); return false; }
    int needed = 0, freeSlots = 0;
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) if (!serverWorld.entities[i].active) freeSlots++;
    for (int i = 0; i < count; i++) {
        SavedEntity *e = &records[i];
        if (SaveDisabled(e)) continue;
        if (Available(e)) needed++;
        if (!LuaMetadata_Validate(LuaEntities_MetadataSchema(LuaEntities_Find(e->name)), &e->metadata)) {
            FreeRecords(records, count); return false;
        }
    }
    if (needed > freeSlots) { FreeRecords(records, count); return false; }
    SavedEntity *opaque = calloc(count ? count : 1, sizeof(*opaque));
    if (!opaque) { FreeRecords(records, count); return false; }
    int opaqueCount = 0;
    for (int i = 0; i < count; i++)
        if (!SaveDisabled(&records[i]) && !Available(&records[i])) opaque[opaqueCount++] = records[i];
    BinaryWriter remaining = {0};
    if (!WriteRecords(&remaining, opaque, opaqueCount)) {
        free(remaining.data); free(opaque); FreeRecords(records, count); return false;
    }
    free(chunk->savedEntities);
    chunk->savedEntities = remaining.data;
    chunk->savedEntitiesSize = remaining.size;
    chunk->entitiesActivated = true;
    free(opaque); // Payloads still belong to records.
    int restored[MAX_SAVED_ENTITIES], restoredCount = 0;
    for (int i = 0; i < count; i++) {
        SavedEntity *saved = &records[i];
        if (SaveDisabled(saved) || !Available(saved)) continue;
        int definition = LuaEntities_Find(saved->name);
        int id = saved->name[0] ? LuaEntities_Restore(definition, saved->position) :
            ServerWorld_AddEntity(ENTITY_TYPE_DROPPED_ITEM, 0, saved->position, -1);
        // Capacity was checked above; restoration invokes no callbacks until all records exist.
        Entity *e = &serverWorld.entities[id];
        e->position = saved->position; e->rotation = saved->rotation;
        e->body = saved->body; e->model = ModelId(saved->model); e->heldBlock = saved->heldBlock;
        e->metadata = saved->metadata; saved->metadata = (Metadata){0};
        e->drop.stack = saved->stack; e->drop.age = saved->age; e->drop.pickupDelay = saved->pickupDelay;
        restored[restoredCount++] = id;
    }
    FreeRecords(records, count);
    ServerPhysics_InvalidateIndex();
    for (int i = 0; i < restoredCount; i++) LuaEntities_Loaded(&serverWorld.entities[restored[i]]);
    return true;
}
static bool Snapshot(const Chunk *chunk, BinaryWriter *out) {
    SavedEntity *pending = NULL;
    int pendingCount = 0;
    if (!ReadRecords(chunk, &pending, &pendingCount)) { FreeRecords(pending, pendingCount); return false; }
    const char *names[MAX_SAVED_ENTITIES];
    const Entity *live[MAX_SAVED_ENTITIES];
    int types[MAX_SAVED_ENTITIES], typeCount = 0, liveCount = 0;
    for (int i = 0; i < pendingCount; i++) types[i] = TypeIndex(names, &typeCount, pending[i].name);
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        const Entity *entity = &serverWorld.entities[i];
        if (!Belongs(entity, chunk) || !ShouldSave(entity)) continue;
        if (pendingCount + liveCount == MAX_SAVED_ENTITIES) { FreeRecords(pending, pendingCount); return false; }
        const char *name = LuaEntities_Name(entity->definitionId);
        types[pendingCount + liveCount] = TypeIndex(names, &typeCount, name ? name : "");
        live[liveCount++] = entity;
    }
    if (pendingCount + liveCount) WriteHeader(out, names, typeCount, pendingCount + liveCount);
    for (int i = 0; i < pendingCount; i++) WriteRecord(out, types[i], &pending[i]);
    for (int i = 0; i < liveCount; i++) {
        const Entity *entity = live[i];
        // Borrow metadata only while writing. No intermediate array or payload copies.
        SavedEntity record = {.position = entity->position, .rotation = entity->rotation,
            .body = entity->body, .heldBlock = entity->heldBlock, .metadata = entity->metadata,
            .stack = entity->drop.stack, .age = entity->drop.age, .pickupDelay = entity->drop.pickupDelay};
        strcpy(record.name, names[types[pendingCount + i]]);
        if (entity->model) {
            if (!serverWorld.modelNames[entity->model][0]) { out->failed = true; break; }
            strcpy(record.model, serverWorld.modelNames[entity->model]);
        }
        WriteRecord(out, types[pendingCount + i], &record);
    }
    FreeRecords(pending, pendingCount);
    return !out->failed;
}
void EntityPersistence_Unload(Chunk *chunk) {
    chunk->entitiesActivated = false;
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        Entity *e = &serverWorld.entities[i];
        if (!Belongs(e, chunk) || e->pendingRemoval) continue;
        // Unloading is not a gameplay removal: no on_remove, drops, or respawns.
        LuaEntities_Detach(e);
        if (e->type == ENTITY_TYPE_DROPPED_ITEM) ServerDrops_Remove(e);
        else if (e->announced) ServerWorld_BroadcastExcluding(ServerPacket_CreateDespawnEntity(e), -1);
        Metadata_Free(&e->metadata);
        e->active = false;
    }
    ServerPhysics_InvalidateIndex();
}
bool EntityPersistence_Save(Chunk *chunk) {
    BinaryWriter entities = {0};
    bool ok = Snapshot(chunk, &entities);
    if (ok) {
        // Encode a temporary view. A failed write never changes the live chunk's
        // pending records, so retrying cannot append its live entities twice.
        Chunk snapshot = *chunk;
        snapshot.savedEntities = entities.data;
        snapshot.savedEntitiesSize = entities.size;
        ok = ServerChunk_SaveFile(&snapshot);
    } else {
        TraceLog(LOG_ERROR, "Could not snapshot chunk entities; keeping the live chunk for retry");
    }
    free(entities.data);
    return ok;
}
