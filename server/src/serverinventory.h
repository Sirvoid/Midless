#ifndef MIDLESS_SERVER_INVENTORY_H
#define MIDLESS_SERVER_INVENTORY_H

#include "player.h"

void ServerInventory_Send(Player *player);
bool ServerInventory_Load(Player *player);
bool ServerInventory_Save(Player *player);
void ServerInventory_ApplyAction(Player *player, InventoryAction action);
void ServerInventory_UpdateHeldBlock(Player *player);

void ServerInventory_UpdateDigging(void);
void ServerInventory_InvalidateDig(Vector3 position);
#endif
