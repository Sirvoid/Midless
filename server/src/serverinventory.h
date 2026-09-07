#ifndef ISLEFORGE_SERVER_INVENTORY_H
#define ISLEFORGE_SERVER_INVENTORY_H

#include "player.h"

void ServerInventory_Send(Player *player);
void ServerInventory_GiveStartingBlocks(Player *player);
void ServerInventory_HandleAction(void);
void ServerInventory_UpdateHeldBlock(Player *player);

#endif
