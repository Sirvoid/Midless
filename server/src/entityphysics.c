#include "blockstates.h"
#include <math.h>
#include "entityphysics.h"
#include "world/world.h"

#define SPATIAL_BUCKETS 2048
#define PHYSICS_STEP (1.0f / 60.0f)
static double accumulator;
static int buckets[SPATIAL_BUCKETS], nextEntity[WORLD_MAX_ENTITIES];
static bool indexDirty = true;

static unsigned Bucket(int x, int y, int z) {
    return ((unsigned)x * 73856093u ^ (unsigned)y * 19349663u ^ (unsigned)z * 83492791u) % SPATIAL_BUCKETS;
}

void ServerPhysics_InvalidateIndex(void) { indexDirty = true; }
void ServerPhysics_Reset(void) { accumulator = 0; indexDirty = true; }

static void RebuildIndex(void) {
    for (int i = 0; i < SPATIAL_BUCKETS; i++) buckets[i] = -1;
    for (int id = 0; id < WORLD_MAX_ENTITIES; id++) {
        Entity *entity = &serverWorld.entities[id];
        if (!entity->active || entity->pendingRemoval) continue;
        Vector3 position = entity->position;
        unsigned bucket = Bucket((int)floorf(position.x / CHUNK_SIZE_X),
            (int)floorf(position.y / CHUNK_SIZE_Y), (int)floorf(position.z / CHUNK_SIZE_Z));
        nextEntity[id] = buckets[bucket];
        buckets[bucket] = id;
    }
    indexDirty = false;
}

int ServerPhysics_QueryEntities(BoundingBox bounds, int *ids, int capacity) {
    if (!serverWorld.entities || !ids || capacity <= 0) return 0;
    float coordinates[] = {bounds.min.x, bounds.min.y, bounds.min.z, bounds.max.x, bounds.max.y, bounds.max.z};
    for (int i = 0; i < 6; i++) if (!isfinite(coordinates[i]) || fabsf(coordinates[i]) > 1000000) return 0;
    if (bounds.min.x > bounds.max.x || bounds.min.y > bounds.max.y || bounds.min.z > bounds.max.z) return 0;
    if (indexDirty) RebuildIndex();
    // Bodies extend at most four blocks from their origin. Include neighboring chunks.
    int minX = (int)floorf((bounds.min.x - 4) / CHUNK_SIZE_X), maxX = (int)floorf((bounds.max.x + 4) / CHUNK_SIZE_X);
    int minY = (int)floorf((bounds.min.y - 4) / CHUNK_SIZE_Y), maxY = (int)floorf((bounds.max.y + 4) / CHUNK_SIZE_Y);
    int minZ = (int)floorf((bounds.min.z - 4) / CHUNK_SIZE_Z), maxZ = (int)floorf((bounds.max.z + 4) / CHUNK_SIZE_Z);
    bool visited[SPATIAL_BUCKETS] = {0};
    double volume = (double)(maxX-minX+1) * (maxY-minY+1) * (maxZ-minZ+1);
    if (volume > SPATIAL_BUCKETS) {
        for (int i = 0; i < SPATIAL_BUCKETS; i++) visited[i] = true;
    } else {
        for (int x = minX; x <= maxX; x++)
        for (int y = minY; y <= maxY; y++)
        for (int z = minZ; z <= maxZ; z++) visited[Bucket(x,y,z)] = true;
    }
    int count = 0;
    for (int bucket = 0; bucket < SPATIAL_BUCKETS; bucket++) {
        if (!visited[bucket]) continue;
        for (int id = buckets[bucket]; id >= 0; id = nextEntity[id]) {
            Entity *entity = &serverWorld.entities[id];
            if (!entity->active || entity->pendingRemoval) continue;
            BoundingBox body = EntityBody_Bounds(&entity->body, entity->position);
            if (body.min.x > bounds.max.x || body.max.x < bounds.min.x || body.min.y > bounds.max.y ||
                body.max.y < bounds.min.y || body.min.z > bounds.max.z || body.max.z < bounds.min.z) continue;
            ids[count++] = id;
            if (count == capacity) return count;
        }
    }
    return count;
}

bool ServerPhysics_SetBody(Entity *entity, EntityBody body) {
    if (!entity || entity->ownerPlayerId >= 0 || !EntityBody_Validate(&body)) return false;
    if (body.enabled && (!isfinite(entity->position.x) || !isfinite(entity->position.y) || !isfinite(entity->position.z) ||
        fabsf(entity->position.x) > 1000000 || fabsf(entity->position.y) > 1000000 || fabsf(entity->position.z) > 1000000)) return false;
    body.grounded = body.sleeping = body.blockedByUnloaded = false;
    entity->body = body;
    indexDirty = true;
    return true;
}

bool ServerPhysics_SetVelocity(Entity *entity, Vector3 velocity) {
    if (!entity || entity->ownerPlayerId >= 0 || !entity->body.enabled) return false;
    EntityBody body = entity->body;
    body.velocity = velocity;
    if (!EntityBody_Validate(&body)) return false;
    body.sleeping = body.grounded = body.blockedByUnloaded = false;
    entity->body = body;
    return true;
}

bool ServerPhysics_ApplyImpulse(Entity *entity, Vector3 impulse) {
    if (!entity) return false;
    Vector3 velocity = entity->body.velocity;
    return ServerPhysics_SetVelocity(entity, (Vector3){velocity.x + impulse.x, velocity.y + impulse.y, velocity.z + impulse.z});
}

static bool QueryBlock(void *context, Vector3 cell, BlockShape *shape) {
    (void)context;
    Vector3 chunk = {floorf(cell.x / CHUNK_SIZE_X), floorf(cell.y / CHUNK_SIZE_Y), floorf(cell.z / CHUNK_SIZE_Z)};
    if (!ServerWorld_GetChunkAt(chunk)) return false;
    int id = ServerWorld_GetBlock(cell);
    *shape = ServerBlockStates_Shape(id, cell);
    return true;
}

void ServerPhysics_Update(float dt) {
    if (!isfinite(dt) || dt <= 0 || !serverWorld.entities) return;
    accumulator += fminf(dt, 0.1f);
    int steps = 0;
    while (accumulator >= PHYSICS_STEP && steps++ < 6) {
        accumulator -= PHYSICS_STEP;
        for (int id = 0; id < WORLD_MAX_ENTITIES; id++) {
            Entity *entity = &serverWorld.entities[id];
            if (!entity->active || entity->pendingRemoval || entity->ownerPlayerId >= 0 || !entity->body.enabled) continue;
            Vector3 previous = entity->position;
            EntityBody_Step(&entity->body, &entity->position, PHYSICS_STEP, QueryBlock, NULL);
            if (previous.x != entity->position.x || previous.y != entity->position.y || previous.z != entity->position.z) {
                entity->dirty = true;
                indexDirty = true;
            }
        }
    }
}
