#include <math.h>
#include "serverinventory.h"
#include "inventoryprotocol.h"
#include "packet.h"
#include "networkhandler.h"
#include "world/world.h"

#define BLOCK_INTERACTION_REACH 8.0f

typedef struct BlockPhysics {
    BoundingBox bounds;
    bool solid, targetable, liquid;
} BlockPhysics;

static const Vector3 faceNormals[6] = {
    {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}
};

static BlockPhysics GetBlockPhysics(int blockId, Vector3 position) {
    BlockPhysics physics = {.bounds = {{0, 0, 0}, {1, 1, 1}}};
    if (!blockId || !ServerWorld_IsBlockDefined(blockId)) return physics;
    physics.solid = physics.targetable = true;
    if (serverWorld.hasBlockDefinition[blockId]) {
        const BlockDefinition *definition = &serverWorld.blockDefinitions[blockId];
        physics.bounds.min = (Vector3){definition->min[0] / 16.0f, definition->min[1] / 16.0f, definition->min[2] / 16.0f};
        physics.bounds.max = (Vector3){definition->max[0] / 16.0f, definition->max[1] / 16.0f, definition->max[2] / 16.0f};
        physics.solid = definition->colliderType == BLOCK_COLLIDER_SOLID;
        physics.liquid = definition->colliderType == BLOCK_COLLIDER_LIQUID;
        physics.targetable = definition->modelType != BLOCK_MODEL_GAS && !physics.liquid;
    } else {
        if (blockId == 5 || blockId == 16) {
            physics.solid = physics.targetable = false;
            physics.liquid = true;
        } else if (blockId == 12 || blockId == 13) {
            physics.solid = false;
            physics.bounds = (BoundingBox){{0.25f, 0, 0.25f}, {0.75f, 0.625f, 0.75f}};
        } else if (blockId == 15) {
            physics.solid = false;
        } else if (blockId == 17 || blockId == 18) {
            physics.bounds.max.y = 0.5f;
        }
    }
    physics.bounds.min.x += position.x;
    physics.bounds.min.y += position.y;
    physics.bounds.min.z += position.z;
    physics.bounds.max.x += position.x;
    physics.bounds.max.y += position.y;
    physics.bounds.max.z += position.z;
    return physics;
}

static bool IsLoaded(Vector3 position) {
    Vector3 chunk = {floorf(position.x / CHUNK_SIZE_X), floorf(position.y / CHUNK_SIZE_Y), floorf(position.z / CHUNK_SIZE_Z)};
    return ServerWorld_GetChunkAt(chunk) != NULL;
}

static bool SegmentHitsBox(Vector3 eye, Vector3 end, BoundingBox box, float *entry) {
    float start[3] = {eye.x, eye.y, eye.z};
    float delta[3] = {end.x - eye.x, end.y - eye.y, end.z - eye.z};
    float minimum[3] = {box.min.x, box.min.y, box.min.z};
    float maximum[3] = {box.max.x, box.max.y, box.max.z};
    float near = 0, far = 1;
    for (int axis = 0; axis < 3; axis++) {
        if (fabsf(delta[axis]) < 0.000001f) {
            if (start[axis] < minimum[axis] || start[axis] > maximum[axis]) return false;
            continue;
        }
        float first = (minimum[axis] - start[axis]) / delta[axis];
        float last = (maximum[axis] - start[axis]) / delta[axis];
        if (first > last) { float swap = first; first = last; last = swap; }
        near = fmaxf(near, first);
        far = fminf(far, last);
        if (near > far) return false;
    }
    *entry = near;
    return true;
}

static bool CanReachTarget(Player *player, const InventoryAction *action) {
    if (action->face >= 6 || !action->targetBlock || action->targetBlock > 255) return false;
    // Bound integer-to-float conversion and chunk coordinates before consulting the world.
    if (action->x < -1000000 || action->x > 1000000 || action->y < -1000000 || action->y > 1000000 ||
        action->z < -1000000 || action->z > 1000000) return false;
    Vector3 target = {(float)action->x, (float)action->y, (float)action->z};
    Vector3 eye = serverWorld.entities[player->entityId].position;
    eye.y += 1.5f;
    if (!isfinite(eye.x) || !isfinite(eye.y) || !isfinite(eye.z)) return false;
    float dx = target.x + 0.5f - eye.x, dy = target.y + 0.5f - eye.y, dz = target.z + 0.5f - eye.z;
    if (dx * dx + dy * dy + dz * dz > 9 * 9) return false;
    if (!IsLoaded(target) || ServerWorld_GetBlock(target) != action->targetBlock) return false;
    BlockPhysics physics = GetBlockPhysics(action->targetBlock, target);
    if (!physics.targetable) return false;

    float minimum[3] = {physics.bounds.min.x, physics.bounds.min.y, physics.bounds.min.z};
    float maximum[3] = {physics.bounds.max.x, physics.bounds.max.y, physics.bounds.max.z};
    float hit[3] = {target.x + action->hit[0] / 255.0f, target.y + action->hit[1] / 255.0f, target.z + action->hit[2] / 255.0f};
    for (int axis = 0; axis < 3; axis++) hit[axis] = fminf(maximum[axis], fmaxf(minimum[axis], hit[axis]));
    int faceAxis = action->face / 2;
    hit[faceAxis] = action->face % 2 ? maximum[faceAxis] : minimum[faceAxis];
    Vector3 end = {hit[0], hit[1], hit[2]};
    Vector3 normal = faceNormals[action->face];
    if ((eye.x - end.x) * normal.x + (eye.y - end.y) * normal.y + (eye.z - end.z) * normal.z < -0.001f) return false;
    dx = end.x - eye.x; dy = end.y - eye.y; dz = end.z - eye.z;
    if (dx * dx + dy * dy + dz * dz > BLOCK_INTERACTION_REACH * BLOCK_INTERACTION_REACH) return false;

    for (int x = (int)floorf(fminf(eye.x, end.x)); x <= (int)floorf(fmaxf(eye.x, end.x)); x++) {
        for (int y = (int)floorf(fminf(eye.y, end.y)); y <= (int)floorf(fmaxf(eye.y, end.y)); y++) {
            for (int z = (int)floorf(fminf(eye.z, end.z)); z <= (int)floorf(fmaxf(eye.z, end.z)); z++) {
                if (x == action->x && y == action->y && z == action->z) continue;
                Vector3 cell = {(float)x, (float)y, (float)z};
                float entry;
                if (!IsLoaded(cell)) {
                    BoundingBox bounds = {cell, {x + 1.0f, y + 1.0f, z + 1.0f}};
                    if (SegmentHitsBox(eye, end, bounds, &entry) && entry < 0.9999f) return false;
                } else {
                    BlockPhysics obstacle = GetBlockPhysics(ServerWorld_GetBlock(cell), cell);
                    if (obstacle.targetable && SegmentHitsBox(eye, end, obstacle.bounds, &entry) && entry < 0.9999f) return false;
                }
            }
        }
    }
    return true;
}

static bool OverlapsPlayer(BoundingBox block) {
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *player = serverWorld.players[i];
        if (!player || player->disconnected || player->entityId < 0) continue;
        Vector3 position = serverWorld.entities[player->entityId].position;
        BoundingBox body = {{position.x - 0.3f, position.y, position.z - 0.3f},
                            {position.x + 0.3f, position.y + 1.5f, position.z + 0.3f}};
        if (block.min.x < body.max.x && block.max.x > body.min.x &&
            block.min.y < body.max.y && block.max.y > body.min.y &&
            block.min.z < body.max.z && block.max.z > body.min.z) return true;
    }
    return false;
}

static void TryHarvestBlock(Player *player, const InventoryAction *action) {
    Inventory result = player->inventory;
    if (!Inventory_Add(&result, action->targetBlock, 1)) {
        ServerPlayer_SendMessage(player, "Inventory full");
        return;
    }

    player->inventory = result;
    Vector3 target = {(float)action->x, (float)action->y, (float)action->z};
    ServerWorld_SetBlock(target, 0, true, true, true);
}

static void TryPlaceBlock(Player *player, const InventoryAction *action) {
    ItemStack *stack = Inventory_GetSelected(&player->inventory);
    if (!stack->count || !ServerWorld_IsBlockDefined(stack->itemId)) return;
    int blockId = stack->itemId;
    Vector3 normal = faceNormals[action->face];
    Vector3 position = {action->x + normal.x, action->y + normal.y, action->z + normal.z};
    bool mergeSlab = !serverWorld.hasBlockDefinition[blockId] && action->face == 3 &&
                     action->targetBlock == blockId && (blockId == 17 || blockId == 18);
    if (mergeSlab) {
        position = (Vector3){(float)action->x, (float)action->y, (float)action->z};
        blockId = blockId == 17 ? 1 : 4;
    }
    if (!IsLoaded(position)) return;
    int previousBlock = ServerWorld_GetBlock(position);
    if (!mergeSlab && previousBlock != 0 && !GetBlockPhysics(previousBlock, position).liquid) return;
    if (previousBlock == blockId) return;
    if (!serverWorld.hasBlockDefinition[blockId] && (blockId == 12 || blockId == 13)) {
        int support = ServerWorld_GetBlock((Vector3){position.x, position.y - 1, position.z});
        if (support != 2 && support != 3 && support != 6) return;
    }
    BlockPhysics physics = GetBlockPhysics(blockId, position);
    if (physics.solid && OverlapsPlayer(physics.bounds)) return;
    stack->count--;
    if (!stack->count) stack->itemId = 0;
    ServerWorld_SetBlock(position, blockId, true, true, true);
}

void ServerInventory_UpdateHeldBlock(Player *player) {
    if (player->entityId < 0 || player->entityId >= WORLD_MAX_ENTITIES) return;
    Entity *entity = &serverWorld.entities[player->entityId];
    ItemStack *stack = Inventory_GetSelected(&player->inventory);
    int heldBlock = stack->count && ServerWorld_IsBlockDefined(stack->itemId) ? stack->itemId : 0;
    if (entity->heldBlock == heldBlock) return;
    entity->heldBlock = heldBlock;
    if (entity->announced) ServerWorld_BroadcastExcluding(ServerPacket_CreateHeldBlock(entity), player->id);
}

void ServerInventory_GiveStartingBlocks(Player *player) {
    for (int blockId = 1; blockId < 256; blockId++) {
        if (!ServerWorld_IsBlockDefined(blockId)) continue;
        if (serverWorld.hasBlockDefinition[blockId] &&
            serverWorld.blockDefinitions[blockId].modelType == BLOCK_MODEL_GAS) continue;
        if (!Inventory_Add(&player->inventory, blockId, Item_GetMaxStack(blockId))) break;
    }
    ServerInventory_UpdateHeldBlock(player);
}

void ServerInventory_Send(Player *player) {
    unsigned char *packet = MemAlloc(INVENTORY_STATE_PACKET_SIZE);
    if (!packet) return;
    InventoryProtocol_WriteState(packet, &player->inventory, player->inventoryRevision, player->inventorySequence);
    ServerNetwork_Send(player, packet);
}

void ServerInventory_HandleAction(void) {
    InventoryAction action;
    if (!InventoryProtocol_ReadAction(serverPacketData, serverPacketDataLength, &action)) return;
    Player *player = serverPacketPlayer;
    if (player->entityId < 0 || player->entityId >= WORLD_MAX_ENTITIES) return;
    // Only the next sequence executes. Retries and out-of-order requests get a snapshot.
    if (player->inventorySequence == UINT32_MAX || action.sequence != player->inventorySequence + 1) {
        ServerInventory_Send(player);
        return;
    }
    player->inventorySequence = action.sequence;
    if (action.type == INVENTORY_BREAK || action.type == INVENTORY_PLACE) {
        if (!player->inventory.open && CanReachTarget(player, &action)) {
            if (action.type == INVENTORY_BREAK) TryHarvestBlock(player, &action);
            else TryPlaceBlock(player, &action);
        }
    } else {
        Inventory_ApplyAction(&player->inventory, &action);
        if (action.type == INVENTORY_CLOSE && player->inventory.open) {
            ServerPlayer_SendMessage(player, "Make room for the cursor stack before closing.");
        }
    }
    player->inventoryRevision++;
    ServerInventory_UpdateHeldBlock(player);
    ServerInventory_Send(player);
}
