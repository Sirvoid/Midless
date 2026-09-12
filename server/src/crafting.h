/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CRAFTING_H
#define MIDLESS_CRAFTING_H
#include "inventory.h"
#define CRAFTING_SIZE 3
#define CRAFTING_SLOTS 9

typedef struct CraftingMatch {
    int recipe;
    ItemStack output;
    uint8_t consume[CRAFTING_SLOTS];
} CraftingMatch;

typedef struct CraftingRecipe {
    char group[65];
    bool shapeless;
    int width, height, count;
    ItemStack ingredients[CRAFTING_SLOTS];
    bool exact[CRAFTING_SLOTS];
    ItemStack output;
} CraftingRecipe;

bool Crafting_Register(const CraftingRecipe *recipe);
void Crafting_Reset(void);
bool Crafting_Find(const char *group, const ItemStack *slots, int columns, int rows,
                   CraftingMatch *match);
#endif
