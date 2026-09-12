/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "playerinventories.h"
#include "player.h"
#include <string.h>

static struct { char name[65]; int slots; } definitions[PLAYER_INVENTORIES];
static int definitionCount;

bool PlayerInventories_Define(const char *name, int slots) {
    if (!name[0] || strlen(name) > 64 || slots < 1 || slots > 255 || definitionCount == PLAYER_INVENTORIES) return false;
    for (int i = 0; i < definitionCount; i++) if (!strcmp(definitions[i].name, name)) return false;
    strcpy(definitions[definitionCount].name, name);
    definitions[definitionCount++].slots = slots;
    return true;
}
void PlayerInventories_Reset(void) { definitionCount = 0; }
NamedInventory *PlayerInventories_Get(Player *player, const char *name) {
    int slots = 0;
    for (int i = 0; i < definitionCount; i++) if (!strcmp(definitions[i].name, name)) slots = definitions[i].slots;
    if (!slots) return NULL;
    for (int i = 0; i < player->namedInventoryCount; i++) {
        NamedInventory *inventory = &player->namedInventories[i];
        if (!strcmp(inventory->name, name)) return inventory->count == slots ? inventory : NULL;
    }
    if (player->namedInventoryCount == PLAYER_INVENTORIES) return NULL;
    NamedInventory *inventory = &player->namedInventories[player->namedInventoryCount++];
    *inventory = (NamedInventory){.count = slots};
    strcpy(inventory->name, name);
    return inventory;
}
