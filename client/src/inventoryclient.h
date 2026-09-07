#ifndef ISLEFORGE_INVENTORY_CLIENT_H
#define ISLEFORGE_INVENTORY_CLIENT_H

#include "inventory.h"
#include "raylib.h"

void ClientInventory_Reset(void);
void ClientInventory_HandleState(void);
void ClientInventory_Update(void);
bool ClientInventory_IsOpen(void);
const Inventory *ClientInventory_Get(void);
bool ClientInventory_Toggle(void);
void ClientInventory_Click(int slot, bool rightClick);
void ClientInventory_Throw(bool oneItem);
void ClientInventory_Select(int hotbarSlot);
void ClientInventory_Scroll(int direction);
void ClientInventory_Interact(bool place, Vector3 hit, Vector3 normal, int targetBlock);

bool ClientInventory_CloseBlocked(void);

#endif
