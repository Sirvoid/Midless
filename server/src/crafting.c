#include "crafting.h"
#include "items.h"
#include <string.h>

#define MAX_RECIPES 1024
static CraftingRecipe recipes[MAX_RECIPES];
static int recipeCount;

static bool Matches(const CraftingRecipe *recipe, int ingredient, ItemStack stack) {
    return recipe->ingredients[ingredient].itemId == stack.itemId &&
           (!recipe->exact[ingredient] ||
            ItemStack_Matches(recipe->ingredients[ingredient], stack));
}
void Crafting_Reset(void) {
    recipeCount = 0;
}

bool Crafting_Find(const char *group, const ItemStack *slots, int columns, int rows,
                   CraftingMatch *match) {
    *match = (CraftingMatch){0};
    if (columns < 1 || rows < 1 || columns > 3 || rows > 3)
        return false;
    int minX = columns, minY = rows, maxX = -1, maxY = -1, occupied = 0;
    for (int y = 0; y < rows; y++)
        for (int x = 0; x < columns; x++)
            if (slots[y * columns + x].count) {
                occupied++;
                if (x < minX)
                    minX = x;
                if (y < minY)
                    minY = y;
                if (x > maxX)
                    maxX = x;
                if (y > maxY)
                    maxY = y;
            }
    for (int r = 0; r < recipeCount; r++) {
        const CraftingRecipe *recipe = &recipes[r];
        if (strcmp(group, recipe->group) || occupied != recipe->count)
            continue;
        bool ok = true;
        if (recipe->shapeless) {
            bool used[CRAFTING_SLOTS] = {0};
            // Match constrained ingredients first, then the remaining ID-only entries.
            for (int pass = 0; pass < 2 && ok; pass++)
                for (int i = 0; i < recipe->count && ok; i++) {
                    if (recipe->exact[i] != (pass == 0))
                        continue;
                    int j;
                    for (j = 0; j < rows * columns; j++)
                        if (!used[j] && slots[j].count && Matches(recipe, i, slots[j])) {
                            used[j] = true;
                            break;
                        }
                    if (j == rows * columns)
                        ok = false;
                }
        } else {
            if (maxX - minX + 1 != recipe->width || maxY - minY + 1 != recipe->height)
                continue;
            for (int y = 0; y < recipe->height && ok; y++)
                for (int x = 0; x < recipe->width; x++) {
                    const ItemStack *slot = &slots[(y + minY) * columns + x + minX];
                    if (!Matches(recipe, y * recipe->width + x, *slot)) {
                        ok = false;
                        break;
                    }
                }
        }
        if (!ok)
            continue;
        match->recipe = r;
        match->output = recipe->output;
        for (int i = 0; i < rows * columns; i++)
            match->consume[i] = slots[i].count ? 1 : 0;
        return true;
    }
    return false;
}

bool Crafting_Register(const CraftingRecipe *recipe) {
    if (!recipe || recipeCount == MAX_RECIPES)
        return false;
    recipes[recipeCount++] = *recipe;
    return true;
}
