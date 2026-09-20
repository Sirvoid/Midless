/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "interaction.h"

#include "scripthooks.h"
#include "serverinventory.h"
#include "inventory.h"
#include "packet.h"
#include "networkhandler.h"
#include "world/world.h"
#include "droppeditems.h"
#include "items.h"

void ServerInventory_UpdateHeldBlock(Player *player) {
    if (player->entityId < 0 || player->entityId >= WORLD_MAX_ENTITIES) return;
    Entity *entity = &serverWorld.entities[player->entityId];
    ItemStack *stack = Inventory_GetSelected(&player->inventory);
    int heldBlock = stack->count && ServerItems_IsDefined(stack->itemId) ? stack->itemId : 0;
    if (entity->heldBlock == heldBlock) return;
    entity->heldBlock = heldBlock;
    if (entity->announced) ServerWorld_BroadcastExcluding(ServerPacket_CreateHeldBlock(entity), player->id);
}

void ServerInventory_Send(Player *player) {
    if (InventoryWindow_Send(player)) return;
    unsigned char *packet = ServerPacket_CreateInventoryState(&player->inventory,
        player->inventoryRevision, player->inventorySequence);
    if (packet) ServerNetwork_Send(player, packet);
}

void ServerInventory_ApplyAction(Player *player, InventoryAction action) {
    if (player->entityId < 0 || player->entityId >= WORLD_MAX_ENTITIES) return;
    // Only the next sequence executes. Retries and out-of-order requests get a snapshot.
    if (player->inventorySequence == UINT32_MAX || action.sequence != player->inventorySequence + 1) {
        ServerInventory_Send(player);
        return;
    }
    player->inventorySequence = action.sequence;
    ServerInteraction_CancelDig(player);
    if (action.type >= INVENTORY_VIEW_LEFT && action.type <= INVENTORY_CRAFT_ALL) {
        InventoryWindow_Action(player, &action);
    } else if (action.session != player->inventoryWindow.view.session) {
        // Ignore clicks and closes belonging to an older screen.
    } else if (action.type == INVENTORY_THROW_STACK || action.type == INVENTORY_THROW_ONE) {
        if (!ServerDrops_Throw(player, action.type == INVENTORY_THROW_ONE) && player->inventory.cursor.count)
            ServerPlayer_SendMessage(player, "Cannot drop items here right now.");
    } else if (action.type == INVENTORY_CLOSE) {
        if (!InventoryWindow_Close(player)) {
            ServerPlayer_SendMessage(player, "Cannot drop the held stack here right now.");
        }
    } else if (action.type == INVENTORY_OPEN && !player->inventoryWindow.view.session) {
        if (!ScriptHooks_InventoryOpenPlayer(player)) Inventory_ApplyAction(&player->inventory, &action);
    } else if (!player->inventoryWindow.view.session || action.type == INVENTORY_SELECT) {
        Inventory_ApplyAction(&player->inventory, &action);
    }
    player->inventoryRevision++;
    ServerInventory_UpdateHeldBlock(player);
    ServerInventory_Send(player);
    InventoryWindow_Update();
}
