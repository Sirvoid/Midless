/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "world/entitypersistence.h"
#include "blockstates.h"
#include <math.h>
#include <stdlib.h>
#include "droppeditems.h"
#include "entityphysics.h"
#include "serverinventory.h"
#include "networkhandler.h"
#include "packet.h"
#include "world/world.h"
#include "items.h"
#include "textcolors.h"

#define PICKUP_RADIUS 1.5f
#define MERGE_RADIUS 0.75f
#define DROP_RADIUS 0.15f

static float pickupTimer, replicationTimer;
static bool replicateThisUpdate;

static Chunk *GetChunk(Vector3 position) {
    return ServerWorld_GetChunkAt((Vector3){floorf(position.x / CHUNK_SIZE_X),
        floorf(position.y / CHUNK_SIZE_Y), floorf(position.z / CHUNK_SIZE_Z)});
}

static BlockShape GetShape(Vector3 cell) {
    int id = ServerWorld_GetBlock(cell);
    return ServerBlockStates_Shape(id, cell);
}

static bool SegmentHitsBox(Vector3 start, Vector3 end, BoundingBox bounds) {
    float origin[] = {start.x, start.y, start.z};
    float delta[] = {end.x - start.x, end.y - start.y, end.z - start.z};
    float minimum[] = {bounds.min.x, bounds.min.y, bounds.min.z};
    float maximum[] = {bounds.max.x, bounds.max.y, bounds.max.z};
    float near = 0, far = 1;
    for (int axis = 0; axis < 3; axis++) {
        if (fabsf(delta[axis]) < 0.000001f) {
            if (origin[axis] <= minimum[axis] || origin[axis] >= maximum[axis]) return false;
            continue;
        }
        float first = (minimum[axis] - origin[axis]) / delta[axis];
        float last = (maximum[axis] - origin[axis]) / delta[axis];
        if (first > last) { float swap = first; first = last; last = swap; }
        near = fmaxf(near, first);
        far = fminf(far, last);
        if (near >= far) return false;
    }
    return near < 1 && far > 0;
}

// Exact block bounds prevent pickup and merging through walls, including slabs.
static bool ClearPath(Vector3 start, Vector3 end) {
    for (int x = (int)floorf(fminf(start.x,end.x)); x <= (int)floorf(fmaxf(start.x,end.x)); x++)
    for (int y = (int)floorf(fminf(start.y,end.y)); y <= (int)floorf(fmaxf(start.y,end.y)); y++)
    for (int z = (int)floorf(fminf(start.z,end.z)); z <= (int)floorf(fmaxf(start.z,end.z)); z++) {
        Vector3 cell = {x,y,z};
        if (!GetChunk(cell)) return false;
        BlockShape shape = GetShape(cell);
        for(int box=0;box<shape.collisionCount;box++)
            if (shape.solid && SegmentHitsBox(start, end, shape.collision[box])) return false;
    }
    return true;
}

static bool ClearSpawn(Vector3 position) {
    BoundingBox bounds = {{position.x - DROP_RADIUS, position.y - DROP_RADIUS, position.z - DROP_RADIUS},
        {position.x + DROP_RADIUS, position.y + DROP_RADIUS, position.z + DROP_RADIUS}};
    for (int x = (int)floorf(bounds.min.x); x <= (int)floorf(bounds.max.x); x++)
    for (int y = (int)floorf(bounds.min.y); y <= (int)floorf(bounds.max.y); y++)
    for (int z = (int)floorf(bounds.min.z); z <= (int)floorf(bounds.max.z); z++) {
        Vector3 cell = {x,y,z};
        if (!GetChunk(cell)) return false;
        BlockShape shape = GetShape(cell);
        for(int box=0;box<shape.collisionCount;box++) {
            BoundingBox block = shape.collision[box];
            if (shape.solid && bounds.min.x < block.max.x && bounds.max.x > block.min.x &&
                bounds.min.y < block.max.y && bounds.max.y > block.min.y &&
                bounds.min.z < block.max.z && bounds.max.z > block.min.z) return false;
        }
    }
    return true;
}

int ServerDrops_Spawn(ItemStack stack, Vector3 position, Vector3 velocity, float pickupDelay) {
    if (!stack.count || stack.count > Item_GetMaxStack(stack.itemId) || !ServerItems_IsDefined(stack.itemId) ||
        !isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z) ||
        fabsf(position.x) > 1000000 || fabsf(position.y) > 1000000 || fabsf(position.z) > 1000000 ||
        !isfinite(pickupDelay) || pickupDelay < 0 || !GetChunk(position)) return -1;
    EntityBody body = EntityBody_Default();
    body.enabled = true;
    body.localBounds = (BoundingBox){{-DROP_RADIUS,-DROP_RADIUS,-DROP_RADIUS}, {DROP_RADIUS,DROP_RADIUS,DROP_RADIUS}};
    body.velocity = velocity;
    body.groundFriction = 10;
    body.restitution = 0.15f;
    if (!EntityBody_Validate(&body)) return -1;
    int id = ServerWorld_AddEntity(ENTITY_TYPE_DROPPED_ITEM, 0, position, -1);
    if (id < 0) return -1;
    Entity *entity = &serverWorld.entities[id];
    ServerPhysics_SetBody(entity, body);
    entity->drop.stack = stack;
    entity->drop.pickupDelay = pickupDelay;
    entity->dirty = true;
    return id;
}


bool ServerDrops_Throw(Player *player, bool oneItem) {
    ItemStack *cursor = &player->inventory.cursor;
    if (!player->inventory.open || !cursor->count || player->entityId < 0) return false;
    Entity *owner = &serverWorld.entities[player->entityId];
    Vector3 position = owner->position;
    position.y += 1.0f;
    if (!isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z) ||
        fabsf(position.x) > 1000000 || fabsf(position.y) > 1000000 || fabsf(position.z) > 1000000 || !ClearSpawn(position)) return false;
    float pitch = owner->rotation.x, yaw = owner->rotation.y;
    Vector3 velocity = {sinf(yaw) * cosf(pitch) * 4, 2 - sinf(pitch) * 4, cosf(yaw) * cosf(pitch) * 4};
    ItemStack thrown = *cursor;
    if (oneItem) thrown.count = 1;
    // Reserve the entity first; failed spawns leave the inventory untouched.
    if (ServerDrops_Spawn(thrown, position, velocity, 1.0f) < 0) return false;
    cursor->count -= thrown.count;
    if (!cursor->count) {
        *cursor = (ItemStack){0};
        player->inventory.cursorOrigin = INVENTORY_NO_SLOT;
    }
    return true;
}

static void MergeNearby(Entity *entity) {
    if (entity->drop.stack.count == Item_GetMaxStack(entity->drop.stack.itemId)) return;
    Vector3 position = entity->position;
    BoundingBox area = {{position.x-MERGE_RADIUS,position.y-MERGE_RADIUS,position.z-MERGE_RADIUS},
        {position.x+MERGE_RADIUS,position.y+MERGE_RADIUS,position.z+MERGE_RADIUS}};
    int nearby[WORLD_MAX_ENTITIES];
    int count = ServerPhysics_QueryEntities(area, nearby, WORLD_MAX_ENTITIES);
    for (int i = 0; i < count; i++) {
        Entity *other = &serverWorld.entities[nearby[i]];
        // Each pair is considered only by the lower entity ID.
        if (other->id <= entity->id || other->type != ENTITY_TYPE_DROPPED_ITEM || other->pendingRemoval ||
            !ItemStack_Matches(other->drop.stack, entity->drop.stack)) continue;
        float dx = other->position.x-position.x, dy = other->position.y-position.y, dz = other->position.z-position.z;
        if (dx*dx+dy*dy+dz*dz > MERGE_RADIUS*MERGE_RADIUS || !ClearPath(position, other->position)) continue;
        int moved = Item_GetMaxStack(entity->drop.stack.itemId) - entity->drop.stack.count;
        if (moved > other->drop.stack.count) moved = other->drop.stack.count;
        entity->drop.stack.count += moved;
        other->drop.stack.count -= moved;
        // Merging cannot bypass a pickup delay or refresh an old item's lifetime.
        entity->drop.pickupDelay = fmaxf(entity->drop.pickupDelay, other->drop.pickupDelay);
        entity->drop.age = fmaxf(entity->drop.age, other->drop.age);
        entity->dirty = other->dirty = true;
        if (!other->drop.stack.count) ServerWorld_RemoveEntity(other->id);
        if (entity->drop.stack.count == Item_GetMaxStack(entity->drop.stack.itemId)) break;
    }
}

static void TryPickup(Entity *entity) {
    if (entity->drop.pickupDelay > 0) return;
    Vector3 position = entity->position;
    BoundingBox area = {{position.x-PICKUP_RADIUS,position.y-PICKUP_RADIUS,position.z-PICKUP_RADIUS},
        {position.x+PICKUP_RADIUS,position.y+PICKUP_RADIUS,position.z+PICKUP_RADIUS}};
    int nearby[WORLD_MAX_ENTITIES];
    int count = ServerPhysics_QueryEntities(area, nearby, WORLD_MAX_ENTITIES);
    for (int i = 0; i < count && entity->drop.stack.count; i++) {
        Entity *owner = &serverWorld.entities[nearby[i]];
        int playerId = owner->ownerPlayerId;
        if (playerId < 0 || playerId >= WORLD_MAX_PLAYERS) continue;
        Player *player = serverWorld.players[playerId];
        if (!player || player->disconnected || player->entityId != owner->id) continue;
        Vector3 center = owner->position;
        center.y += 0.75f;
        BoundingBox body = {{owner->position.x-0.3f,owner->position.y,owner->position.z-0.3f},
            {owner->position.x+0.3f,owner->position.y+1.5f,owner->position.z+0.3f}};
        float dx = position.x - fminf(body.max.x,fmaxf(body.min.x,position.x));
        float dy = position.y - fminf(body.max.y,fmaxf(body.min.y,position.y));
        float dz = position.z - fminf(body.max.z,fmaxf(body.min.z,position.z));
        if (dx*dx+dy*dy+dz*dz > PICKUP_RADIUS*PICKUP_RADIUS || !ClearPath(position, center)) continue;
        int inserted = Inventory_AddStackPartial(&player->inventory, entity->drop.stack);
        if (!inserted) continue;
        entity->drop.stack.count -= inserted;
        entity->dirty = true;
        player->inventoryRevision++;
        ServerInventory_UpdateHeldBlock(player);
        ServerInventory_Send(player);
    }
    if (!entity->drop.stack.count) ServerWorld_RemoveEntity(entity->id);
}

void ServerDrops_Update(float dt) {
    if (!isfinite(dt) || dt <= 0) return;
    pickupTimer += dt;
    replicationTimer += dt;
    bool checkPickups = pickupTimer >= 0.1f;
    replicateThisUpdate = replicationTimer >= 0.05f;
    if (checkPickups) pickupTimer = fmodf(pickupTimer, 0.1f);
    if (replicateThisUpdate) replicationTimer = fmodf(replicationTimer, 0.05f);
    for (int id = 0; id < WORLD_MAX_ENTITIES; id++) {
        Entity *entity = &serverWorld.entities[id];
        if (!entity->active || entity->pendingRemoval || entity->type != ENTITY_TYPE_DROPPED_ITEM) continue;
        if (EntityPersistence_IsSaving(entity)) continue;
        entity->drop.age += dt;
        entity->drop.pickupDelay = fmaxf(0, entity->drop.pickupDelay-dt);
        if (DROPPED_ITEM_LIFETIME > 0 && entity->drop.age >= DROPPED_ITEM_LIFETIME) {
            ServerWorld_RemoveEntity(id);
            continue;
        }
        if (!checkPickups || !GetChunk(entity->position)) continue;
        MergeNearby(entity);
        TryPickup(entity);
    }
}

void ServerDrops_Replicate(Entity *entity) {
    if (!replicateThisUpdate) return;
    Chunk *chunk = GetChunk(entity->position);
    for (int id = 0; id < WORLD_MAX_PLAYERS; id++) {
        Player *player = serverWorld.players[id];
        bool visible = player && !player->disconnected && chunk && ServerChunk_PlayerInChunk(chunk, player);
        if (visible && (!entity->drop.viewers[id] || entity->dirty)) {
            ServerNetwork_Send(player, ServerPacket_CreateDroppedItem(entity));
        } else if (!visible && entity->drop.viewers[id] && player && !player->disconnected) {
            ServerNetwork_Send(player, ServerPacket_CreateDespawnEntity(entity));
        }
        if (visible && (!entity->drop.viewers[id] || entity->nametagDirty))
            ServerNetwork_Send(player, ServerNametag_CreatePacket(entity));
        entity->drop.viewers[id] = visible;
    }
    entity->announced = true;
    entity->nametagDirty = false;
    entity->dirty = false;
}

void ServerDrops_Remove(Entity *entity) {
    for (int id = 0; id < WORLD_MAX_PLAYERS; id++) {
        Player *player = serverWorld.players[id];
        if (entity->drop.viewers[id] && player && !player->disconnected)
            ServerNetwork_Send(player, ServerPacket_CreateDespawnEntity(entity));
    }
}

void ServerDrops_ForgetPlayer(int playerId) {
    if (playerId < 0 || playerId >= WORLD_MAX_PLAYERS) return;
    for (int id = 0; id < WORLD_MAX_ENTITIES; id++) serverWorld.entities[id].drop.viewers[playerId] = false;
}

void ServerDrops_Reset(void) {
    pickupTimer = replicationTimer = 0;
    replicateThisUpdate = false;
}
