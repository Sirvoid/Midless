#include "scripting/luaitemactions.h"
#include "raymath.h"
#include "scripting/luadigging.h"
#include <math.h>
#include <stdlib.h>
#include "serverinventory.h"
#include "inventoryprotocol.h"
#include "packet.h"
#include "networkhandler.h"
#include "world/world.h"
#include "blockshape.h"
#include "droppeditems.h"
#include "items.h"
#include "scripting/luabindings.h"
#include "scripting/luametadata.h"
#include "scripting/luainventory.h"

#define BLOCK_INTERACTION_REACH 8.0f

typedef BlockShape BlockPhysics;

static const Vector3 faceNormals[6] = {
    {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}
};

static BlockPhysics GetBlockPhysics(int blockId, Vector3 position) {
    const BlockDefinition *definition = blockId > 0 && blockId < 256 && serverWorld.hasBlockDefinition[blockId]
        ? &serverWorld.blockDefinitions[blockId] : NULL;
    return BlockShape_Get(blockId, definition, position);
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

static bool TryHarvestBlock(Player *player, const InventoryAction *action) {
    Vector3 target = {(float)action->x, (float)action->y, (float)action->z};
    Vector3 position = {target.x + 0.5f, target.y + 0.5f, target.z + 0.5f};
    ItemStack stacks[WORLD_MAX_ENTITIES] = {0};
    ItemStack tool=*Inventory_GetSelected(&player->inventory);
    int loot=LuaItemActions_Drops(player,target,action->targetBlock,tool,stacks,WORLD_MAX_ENTITIES);
    if (loot<0) { ServerPlayer_SendMessage(player,"Cannot break this block: invalid drops."); return false; }
    // Lua callbacks can change the world or the held item. Check again before awarding loot.
    ItemStack held=*Inventory_GetSelected(&player->inventory);
    if (!CanReachTarget(player,action) || held.count!=tool.count || !ItemStack_Matches(held,tool)) return false;
    int contents = LuaMetadata_CollectBlockItems(target, stacks + loot, WORLD_MAX_ENTITIES - loot);
    if (contents < 0) {
        ServerPlayer_SendMessage(player, "Cannot break this block: its inventory could not be read.");
        return false;
    }
    int count = contents + loot, available = 0;
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) if (!serverWorld.entities[i].active) available++;
    if (available < count) {
        ServerPlayer_SendMessage(player, "Cannot break this block: no room to drop all its items.");
        return false;
    }
    int spawned[WORLD_MAX_ENTITIES];
    for (int i = 0; i < count; i++) {
        Vector3 velocity = {(rand() % 201 - 100) / 100.0f, 2.5f, (rand() % 201 - 100) / 100.0f};
        spawned[i] = ServerDrops_Spawn(stacks[i], position, velocity, 0.5f);
        if (spawned[i] < 0) {
            // None have been announced or saved yet. Roll back the entire drop.
            for (int j = 0; j < i; j++) ServerWorld_RemoveEntity(spawned[j]);
            ServerPlayer_SendMessage(player, "Cannot break this block: an item could not be dropped.");
            return false;
        }
    }
    ServerWorld_SetBlock(target, 0, true, true, true);
    return true;
}

static void SendDigState(Player *player, int milliseconds) {
    unsigned char *packet=MemAlloc(21);
    if (!packet) return;
    packet[0]=25;
    int values[]={player->digAction.x,player->digAction.y,player->digAction.z,
        (int)player->digAction.sequence,milliseconds};
    for (int i=0;i<5;i++) for (int b=0;b<4;b++) packet[1+i*4+b]=(uint32_t)values[i]>>(24-b*8);
    ServerNetwork_Send(player,packet);
}
static void CancelDig(Player *player) {
    if (!player->digging) return;
    player->digging=false; SendDigState(player,-1);
}
void ServerInventory_InvalidateDig(Vector3 position) {
    for (int i=0;i<WORLD_MAX_PLAYERS;i++) {
        Player *p=serverWorld.players[i];
        if (p && p->digging && p->digAction.x==floorf(position.x) &&
            p->digAction.y==floorf(position.y) && p->digAction.z==floorf(position.z)) CancelDig(p);
    }
}
static bool ValidDig(Player *p) {
    ItemStack held=*Inventory_GetSelected(&p->inventory);
    return !p->disconnected && !p->inventory.open && p->inventory.selectedHotbar==p->digSlot &&
        held.count==p->digStack.count && ItemStack_Matches(held,p->digStack) && CanReachTarget(p,&p->digAction);
}
void ServerInventory_UpdateDigging(void) {
    for (int i=0;i<WORLD_MAX_PLAYERS;i++) {
        Player *p=serverWorld.players[i];
        if (!p || !p->digging) continue;
        if (!ValidDig(p)) { CancelDig(p); continue; }
        if (GetTime()<p->digEnd) continue;
        InventoryAction action=p->digAction;
        ItemStack stack=p->digStack;
        CancelDig(p);
        if (TryHarvestBlock(p,&action)) {
            LuaDigging_Finished(p,(Vector3){action.x,action.y,action.z},stack);
            p->inventoryRevision++; ServerInventory_UpdateHeldBlock(p); ServerInventory_Send(p);
        }
    }
}
static void StartDig(Player *p, const InventoryAction *action) {
    CancelDig(p);
    p->digAction=*action;
    p->digStack=*Inventory_GetSelected(&p->inventory);
    p->digSlot=p->inventory.selectedHotbar;
    double seconds=LuaDigging_Time(p,(Vector3){action->x,action->y,action->z},action->targetBlock,p->digStack);
    if (seconds<0 || !ValidDig(p)) { SendDigState(p,-1); return; }
    p->digging=true; p->digEnd=GetTime()+seconds;
    SendDigState(p,(int)ceil(seconds*1000));
    if (seconds==0) ServerInventory_UpdateDigging();
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
    if (!stack->count) *stack = (ItemStack){0};
    ServerWorld_SetBlock(position, blockId, true, true, true);
}

// Resolve entity and empty-space uses on the server; walls and unloaded chunks stop the ray.
static Entity *FindUseTarget(Player *player, InventoryAction *block) {
    *block=(InventoryAction){0};
    Entity *owner=&serverWorld.entities[player->entityId];
    Vector3 eye=owner->position; eye.y+=1.5f;
    Vector3 rotation=owner->rotation;
    Vector3 direction={sinf(rotation.y)*cosf(rotation.x),-sinf(rotation.x),cosf(rotation.y)*cosf(rotation.x)};
    if (!isfinite(eye.x) || !isfinite(eye.y) || !isfinite(eye.z) ||
        fabsf(eye.x)>1000000 || fabsf(eye.y)>1000000 || fabsf(eye.z)>1000000 ||
        !isfinite(direction.x) || !isfinite(direction.y) || !isfinite(direction.z)) return NULL;
    Vector3 end=Vector3Add(eye,Vector3Scale(direction,BLOCK_INTERACTION_REACH));
    float closest=BLOCK_INTERACTION_REACH;
    for (int x=floorf(fminf(eye.x,end.x));x<=floorf(fmaxf(eye.x,end.x));x++)
    for (int y=floorf(fminf(eye.y,end.y));y<=floorf(fmaxf(eye.y,end.y));y++)
    for (int z=floorf(fminf(eye.z,end.z));z<=floorf(fmaxf(eye.z,end.z));z++) {
        Vector3 cell={x,y,z}; bool loaded=IsLoaded(cell);
        int id=loaded?ServerWorld_GetBlock(cell):0;
        BlockPhysics shape=GetBlockPhysics(id,cell);
        if (loaded && !shape.targetable) continue;
        BoundingBox bounds=loaded?shape.bounds:(BoundingBox){cell,{x+1,y+1,z+1}};
        float entry;
        if (!SegmentHitsBox(eye,end,bounds,&entry) || entry*BLOCK_INTERACTION_REACH>=closest) continue;
        closest=entry*BLOCK_INTERACTION_REACH;
        Vector3 hit=Vector3Add(eye,Vector3Scale(direction,closest));
        *block=(InventoryAction){.targetBlock=id,.x=x,.y=y,.z=z};
        float distances[]={fabsf(hit.x-bounds.min.x),fabsf(hit.x-bounds.max.x),
            fabsf(hit.y-bounds.min.y),fabsf(hit.y-bounds.max.y),fabsf(hit.z-bounds.min.z),fabsf(hit.z-bounds.max.z)};
        for (int face=1;face<6;face++) if (distances[face]<distances[block->face]) block->face=face;
        block->hit[0]=Clamp((hit.x-x)*255,0,255);
        block->hit[1]=Clamp((hit.y-y)*255,0,255);
        block->hit[2]=Clamp((hit.z-z)*255,0,255);
    }
    Entity *target=NULL;
    for (int i=0;i<WORLD_MAX_ENTITIES;i++) {
        Entity *entity=&serverWorld.entities[i];
        if (i==player->entityId || !entity->active || entity->pendingRemoval || entity->type==ENTITY_TYPE_DROPPED_ITEM) continue;
        BoundingBox bounds=EntityBody_Bounds(&entity->body,entity->position);
        float entry;
        if (SegmentHitsBox(eye,end,bounds,&entry) && entry*BLOCK_INTERACTION_REACH<closest) {
            closest=entry*BLOCK_INTERACTION_REACH; target=entity;
        }
    }
    return target;
}
static void TryUseItem(Player *player, const InventoryAction *requested) {
    if (player->inventory.open) return;
    InventoryAction rayBlock;
    Entity *entity=FindUseTarget(player,&rayBlock);
    // A block request must still be valid even when an entity is in front of it.
    bool blockRequest=requested->type==INVENTORY_PLACE;
    if (blockRequest && !CanReachTarget(player,requested)) return;
    if (entity) { LuaItemActions_Use(player,NULL,entity); return; }
    const InventoryAction *block=blockRequest?requested:rayBlock.targetBlock?&rayBlock:NULL;
    if (!block) { LuaItemActions_Use(player,NULL,NULL); return; }
    if (!CanReachTarget(player,block)) return;
    Vector3 position={block->x,block->y,block->z};
    if (LuaBindings_InteractBlock(player,position,block->targetBlock)) return;
    ItemStack held=*Inventory_GetSelected(&player->inventory);
    if (LuaItemActions_Use(player,block,NULL)) return;
    ItemStack after=*Inventory_GetSelected(&player->inventory);
    if (!player->inventory.open && after.count==held.count && ItemStack_Matches(after,held) && CanReachTarget(player,block))
        TryPlaceBlock(player,block);
}

void ServerInventory_UpdateHeldBlock(Player *player) {
    if (player->entityId < 0 || player->entityId >= WORLD_MAX_ENTITIES) return;
    Entity *entity = &serverWorld.entities[player->entityId];
    ItemStack *stack = Inventory_GetSelected(&player->inventory);
    int heldBlock = stack->count && ServerItems_IsDefined(stack->itemId) ? stack->itemId : 0;
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
    if (InventoryWindow_Send(player)) return;
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
    if (action.type != INVENTORY_BREAK) CancelDig(player);
    if (action.type == INVENTORY_PLACE || action.type == INVENTORY_USE) {
        TryUseItem(player,&action);
    } else if (action.type == INVENTORY_BREAK) {
        if (!player->inventory.open && CanReachTarget(player, &action)) StartDig(player, &action);
    } else if (action.type >= INVENTORY_VIEW_LEFT && action.type <= INVENTORY_CRAFT_ALL) {
        InventoryWindow_Action(player, &action);
    } else if ((uint32_t)action.x != player->inventoryWindow.view.session) {
        // Ignore clicks and closes belonging to an older screen.
    } else if (action.type == INVENTORY_THROW_STACK || action.type == INVENTORY_THROW_ONE) {
        if (!ServerDrops_Throw(player, action.type == INVENTORY_THROW_ONE) && player->inventory.cursor.count)
            ServerPlayer_SendMessage(player, "Cannot drop items here right now.");
    } else if (action.type == INVENTORY_CLOSE) {
        if (!InventoryWindow_Close(player)) {
            ServerPlayer_SendMessage(player, "Cannot drop the held stack here right now.");
        }
    } else if (action.type == INVENTORY_OPEN && !player->inventoryWindow.view.session) {
        if (!LuaInventory_OpenPlayer(player)) Inventory_ApplyAction(&player->inventory, &action);
    } else if (!player->inventoryWindow.view.session || action.type == INVENTORY_SELECT) {
        Inventory_ApplyAction(&player->inventory, &action);
    }
    player->inventoryRevision++;
    ServerInventory_UpdateHeldBlock(player);
    ServerInventory_Send(player);
    InventoryWindow_Update();
}
