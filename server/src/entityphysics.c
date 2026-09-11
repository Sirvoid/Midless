#include "world/entitypersistence.h"
#include "blockstates.h"
#include <math.h>
#include "entityphysics.h"
#include "mobs.h"
#include "scripting/luamobs.h"
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
    entity->moveEnabled = entity->recovering = false;
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
    entity->moveEnabled = false;
    entity->recovering = false;
    return true;
}

bool ServerPhysics_ApplyImpulse(Entity *entity, Vector3 impulse) {
    if (!entity) return false;
    if (impulse.x == 0 && impulse.y == 0 && impulse.z == 0)
        return entity->ownerPlayerId < 0 && entity->body.enabled;
    Vector3 velocity = entity->body.velocity;
    bool moving = entity->moveEnabled;
    if (!ServerPhysics_SetVelocity(entity, (Vector3){velocity.x + impulse.x, velocity.y + impulse.y, velocity.z + impulse.z})) return false;
    entity->moveEnabled = moving;
    if (impulse.x != 0 || impulse.y != 0 || impulse.z != 0) entity->recovering = true;
    return true;
}

bool ServerPhysics_Move(Entity *entity, Vector3 direction, float speed, float acceleration) {
    if (!entity || entity->ownerPlayerId >= 0 || !entity->body.enabled || !isfinite(speed) || speed < 0 || speed > 20 ||
        !isfinite(acceleration) || acceleration <= 0 || acceleration > 100 ||
        !isfinite(direction.x) || !isfinite(direction.z) || fabsf(direction.x)>1000000 || fabsf(direction.z)>1000000) return false;
    float length = hypotf(direction.x,direction.z);
    entity->moveVelocity = length > 0 ? (Vector3){direction.x/length*speed,0,direction.z/length*speed} : (Vector3){0};
    entity->move3D = false;
    entity->moveAcceleration = acceleration;
    entity->moveEnabled = true;
    return true;
}
bool ServerPhysics_Jump(Entity *entity, float speed) {
    if (!entity || entity->ownerPlayerId >= 0 || !entity->body.enabled || entity->recovering ||
        !entity->body.grounded || !isfinite(speed) || speed <= 0 || speed > 20) return false;
    entity->body.velocity.y = speed;
    entity->body.grounded = entity->body.sleeping = false;
    return true;
}
static void Steer(Entity *entity) {
    EntityBody *body = &entity->body;
    if (entity->recovering) {
        // Protect the airborne arc, then let acceleration blend back into
        // walking on landing instead of waiting for friction to stop the mob.
        if (!body->grounded) return;
        entity->recovering = false;
    }
    if (!entity->moveEnabled) return;
    float x = entity->moveVelocity.x-body->velocity.x, z = entity->moveVelocity.z-body->velocity.z;
    float y = entity->move3D ? entity->moveVelocity.y-body->velocity.y : 0;
    float distance = sqrtf(x*x+y*y+z*z), change = entity->moveAcceleration*PHYSICS_STEP;
    if (distance <= 0.00001f) return;
    float scale = fminf(1,change/distance);
    body->velocity.x += x*scale; body->velocity.z += z*scale;
    if (entity->move3D) body->velocity.y += y*scale;
    body->sleeping = false;
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
    ServerMobs_BeginTick();
    accumulator += fminf(dt, 0.1f);
    int steps = 0;
    while (accumulator >= PHYSICS_STEP && steps++ < 6) {
        accumulator -= PHYSICS_STEP;
        for (int id = 0; id < WORLD_MAX_ENTITIES; id++) {
            Entity *entity = &serverWorld.entities[id];
            if (!entity->active || entity->pendingRemoval || entity->ownerPlayerId >= 0) continue;
            if (EntityPersistence_IsSaving(entity)) continue;
            if (entity->recovering && entity->body.grounded) entity->recovering = false;
            LuaMobs_Physics(entity,PHYSICS_STEP);
            if (!entity->active || entity->pendingRemoval || !entity->body.enabled) continue;
            Vector3 previous = entity->position;
            Steer(entity);
            EntityBody_Step(&entity->body, &entity->position, PHYSICS_STEP, QueryBlock, NULL);
            if (previous.x != entity->position.x || previous.y != entity->position.y || previous.z != entity->position.z) {
                entity->dirty = true;
                indexDirty = true;
            }
        }
    }
}
