/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_INVENTORY_CLIENT_H
#define MIDLESS_INVENTORY_CLIENT_H

#include "inventory.h"
#include "inventoryview.h"
#include "raylib.h"

void ClientInventory_Reset(void);
void ClientInventory_SetState(const Inventory *inventory, uint32_t revision, uint32_t acknowledged, const InventoryView *view);
const InventoryView *ClientInventory_GetView(void);
void ClientInventory_ClickView(int binding, int slot, bool right, bool shift);
void ClientInventory_Craft(int element, bool all);
void ClientInventory_Update(void);
bool ClientInventory_IsOpen(void);
const Inventory *ClientInventory_Get(void);
bool ClientInventory_Toggle(void);
void ClientInventory_Click(int slot, bool rightClick);
void ClientInventory_Throw(bool oneItem);
void ClientInventory_Select(int hotbarSlot);
void ClientInventory_Scroll(int direction);
bool ClientInventory_CloseBlocked(void);
bool ClientInventory_Ready(void);
uint32_t ClientInventory_Sequence(void);
#endif
