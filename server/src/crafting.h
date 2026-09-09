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

int Crafting_Register(void);
void Crafting_Reset(void);
bool Crafting_Find(const char *group, const ItemStack *slots, int columns, int rows, CraftingMatch *match);
#endif
