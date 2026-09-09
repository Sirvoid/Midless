#ifndef MIDLESS_SERVER_INVENTORY_H
#define MIDLESS_SERVER_INVENTORY_H

#include "player.h"

void ServerInventory_Send(Player *player);
void ServerInventory_GiveStartingBlocks(Player *player);
bool ServerInventory_Load(Player *player);
bool ServerInventory_Save(Player *player);
void ServerInventory_HandleAction(void);
void ServerInventory_UpdateHeldBlock(Player *player);

void ServerInventory_UpdateDigging(void);
void ServerInventory_InvalidateDig(Vector3 position);
#endif
