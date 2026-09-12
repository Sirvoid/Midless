/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <math.h>
#include "spawnmanager.h"
#include "blockstates.h"
#include "entityphysics.h"
#include "world/world.h"

static bool Shape(Vector3 cell, BlockShape *shape) {
    Chunk *chunk = ServerWorld_GetChunkAt((Vector3){floorf(cell.x / CHUNK_SIZE_X),
        floorf(cell.y / CHUNK_SIZE_Y), floorf(cell.z / CHUNK_SIZE_Z)});
    if (!chunk || chunk->loadFailed) return false;
    *shape = ServerBlockStates_Shape(ServerWorld_GetBlock(cell), cell);
    return true;
}
static bool Overlap(BoundingBox a, BoundingBox b) {
    return a.min.x < b.max.x && a.max.x > b.min.x && a.min.y < b.max.y &&
        a.max.y > b.min.y && a.min.z < b.max.z && a.max.z > b.min.z;
}
bool ServerSpawnPlacement_Clear(EntityBody body, Vector3 position, bool avoidLiquids) {
    if (!EntityBody_Validate(&body) || !body.enabled || !isfinite(position.x) ||
        !isfinite(position.y) || !isfinite(position.z) || fabsf(position.x) > 999990 ||
        fabsf(position.y) > 999990 || fabsf(position.z) > 999990) return false;
    BoundingBox bounds = EntityBody_Bounds(&body, position);
    for (int x = floorf(bounds.min.x); x < ceilf(bounds.max.x); x++)
    for (int y = floorf(bounds.min.y); y < ceilf(bounds.max.y); y++)
    for (int z = floorf(bounds.min.z); z < ceilf(bounds.max.z); z++) {
        BlockShape shape;
        if (!Shape((Vector3){x,y,z}, &shape)) return false;
        if (avoidLiquids && shape.liquid) return false;
        if (shape.solid) for (int i = 0; i < shape.collisionCount; i++)
            if (Overlap(bounds, shape.collision[i])) return false;
    }
    int ids[WORLD_MAX_ENTITIES];
    int count = ServerPhysics_QueryEntities(bounds, ids, WORLD_MAX_ENTITIES);
    for (int i = 0; i < count; i++) {
        Entity *e = &serverWorld.entities[ids[i]];
        if ((e->body.enabled || e->ownerPlayerId >= 0) &&
            Overlap(bounds, EntityBody_Bounds(&e->body, e->position))) return false;
    }
    return true;
}
bool ServerSpawnPlacement_Find(const SpawnRule *rule, EntityBody body, Vector3 column, Vector3 *position) {
    if (!isfinite(column.x) || !isfinite(column.y) || !isfinite(column.z) ||
        fabsf(column.x) > 999900 || fabsf(column.y) > 999900 || fabsf(column.z) > 999900) return false;
    int x = floorf(column.x), z = floorf(column.z), center = floorf(column.y);
    // Search nearest elevations first; cave floors and partial blocks are eligible.
    for (int n = 0; n <= rule->verticalRange * 2; n++) {
        int y = center + (n % 2 ? (n + 1) / 2 : -n / 2);
        Vector3 cell = {x,y,z};
        BlockShape shape;
        if (!Shape(cell, &shape) || !shape.solid || (rule->avoidLiquids && shape.liquid)) continue;
        int id = ServerWorld_GetBlock(cell);
        if (rule->filterGround && (id < 0 || id > 255 || !rule->groundBlocks[id])) continue;
        for (int i = 0; i < shape.collisionCount; i++) {
            BoundingBox support = shape.collision[i];
            Vector3 p = {x + 0.5f, support.max.y - body.localBounds.min.y, z + 0.5f};
            BoundingBox bounds = EntityBody_Bounds(&body, p);
            // Require a supporting face beneath the entire footprint.
            if (bounds.min.x < support.min.x || bounds.max.x > support.max.x ||
                bounds.min.z < support.min.z || bounds.max.z > support.max.z) continue;
            if (ServerSpawnPlacement_Clear(body, p, rule->avoidLiquids)) { *position = p; return true; }
        }
    }
    return false;
}
bool ServerSpawnPlacement_Valid(const SpawnRule *rule, EntityBody body, Vector3 position) {
    if (!ServerSpawnPlacement_Clear(body, position, rule->avoidLiquids)) return false;
    BoundingBox bounds = EntityBody_Bounds(&body, position);
    Vector3 cell = {floorf(position.x), floorf(bounds.min.y - 0.001f), floorf(position.z)};
    BlockShape shape;
    if (!Shape(cell, &shape) || !shape.solid || (rule->avoidLiquids && shape.liquid)) return false;
    int id = ServerWorld_GetBlock(cell);
    if (rule->filterGround && (id < 0 || id > 255 || !rule->groundBlocks[id])) return false;
    for (int i = 0; i < shape.collisionCount; i++) {
        BoundingBox support = shape.collision[i];
        if (fabsf(support.max.y - bounds.min.y) < 0.0001f && bounds.min.x >= support.min.x &&
            bounds.max.x <= support.max.x && bounds.min.z >= support.min.z && bounds.max.z <= support.max.z) return true;
    }
    return false;
}
