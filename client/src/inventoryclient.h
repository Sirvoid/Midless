#ifndef MIDLESS_INVENTORY_CLIENT_H
#define MIDLESS_INVENTORY_CLIENT_H

#include "inventory.h"
#include "inventoryview.h"
#include "raylib.h"

void ClientInventory_Reset(void);
void ClientInventory_HandleState(void);
void ClientInventory_HandleView(void);
const InventoryView *ClientInventory_GetView(void);
void ClientInventory_ClickView(bool container, int slot, bool right, bool shift);
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
