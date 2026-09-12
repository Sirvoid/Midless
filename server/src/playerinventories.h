/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_PLAYER_INVENTORIES_H
#define MIDLESS_PLAYER_INVENTORIES_H
#include "inventory.h"
#define PLAYER_INVENTORIES 16
typedef struct NamedInventory {
    char name[65];
    uint8_t count;
    ItemStack slots[255];
} NamedInventory;
struct Player;
bool PlayerInventories_Define(const char *name, int slots);
void PlayerInventories_Reset(void);
NamedInventory *PlayerInventories_Get(struct Player *player, const char *name);
#endif
