/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "luaitems.h"
#include "luacrafting.h"
#include "../crafting.h"
#include "../items.h"
#include "luametadata.h"
#include <string.h>

extern lua_State *L;

static ItemStack ReadIngredient(int index, bool *exact) {
    ItemStack stack = {0};
    index = lua_absindex(L, index);
    if (lua_istable(L, index)) {
        lua_getfield(L, index, "id");
        stack.itemId = LuaItems_Id(L, -1, false, false);
        lua_pop(L, 1);
        lua_getfield(L, index, "metadata");
        *exact = !lua_isnil(L, -1);
        lua_pop(L, 1);
        LuaMetadata_ReadItem(L, index, &stack);
    } else
        stack.itemId = LuaItems_Id(L, index, false, false);
    stack.count = stack.itemId ? 1 : 0;
    return stack;
}

int LuaCrafting_Register(void) {
    luaL_checktype(L, 1, LUA_TTABLE);
    CraftingRecipe recipe = {0};
    lua_getfield(L, 1, "group");
    size_t length;
    const char *group = lua_isnil(L, -1) ? "crafting" : luaL_checklstring(L, -1, &length);
    if (lua_isnil(L, -1))
        length = strlen(group);
    if (!length || length > 64 || memchr(group, 0, length))
        return luaL_error(L, "invalid recipe group");
    memcpy(recipe.group, group, length);
    lua_pop(L, 1);
    lua_getfield(L, 1, "output");
    luaL_checktype(L, -1, LUA_TTABLE);
    LuaItems_ReadStack(L, -1, &recipe.output);
    lua_pop(L, 1);
    lua_getfield(L, 1, "pattern");
    lua_getfield(L, 1, "ingredients");
    if (lua_isnil(L, -1) == lua_isnil(L, -2))
        return luaL_error(L, "provide pattern or ingredients, not both");
    recipe.shapeless = !lua_isnil(L, -1);
    if (recipe.shapeless) {
        luaL_checktype(L, -1, LUA_TTABLE);
        int count = lua_rawlen(L, -1);
        if (count < 1 || count > CRAFTING_SLOTS)
            return luaL_error(L, "recipes require 1..9 ingredients");
        recipe.count = count;
        for (int i = 0; i < count; i++) {
            lua_rawgeti(L, -1, i + 1);
            recipe.ingredients[i] = ReadIngredient(-1, &recipe.exact[i]);
            lua_pop(L, 1);
            if (!recipe.ingredients[i].itemId)
                return luaL_error(L, "shapeless ingredients cannot be empty");
        }
    } else {
        lua_pop(L, 1);
        luaL_checktype(L, -1, LUA_TTABLE);
        int height = lua_rawlen(L, -1), width = 0;
        if (height < 1 || height > CRAFTING_SIZE)
            return luaL_error(L, "patterns support 1..3 rows");
        ItemStack cells[CRAFTING_SLOTS] = {0};
        bool exact[CRAFTING_SLOTS] = {0};
        int minX = 3, minY = 3, maxX = -1, maxY = -1;
        for (int y = 0; y < height; y++) {
            lua_rawgeti(L, -1, y + 1);
            luaL_checktype(L, -1, LUA_TTABLE);
            int rowWidth = lua_rawlen(L, -1);
            if (rowWidth < 1 || rowWidth > 3 || (y && rowWidth != width))
                return luaL_error(
                    L, "pattern rows must have the same width, 1..3; use 0 for empty cells");
            width = rowWidth;
            for (int x = 0; x < width; x++) {
                lua_rawgeti(L, -1, x + 1);
                ItemStack ingredient = ReadIngredient(-1, &exact[y * 3 + x]);
                lua_pop(L, 1);
                int id = ingredient.itemId;
                cells[y * 3 + x] = ingredient;
                if (id) {
                    recipe.count++;
                    if (x < minX)
                        minX = x;
                    if (y < minY)
                        minY = y;
                    if (x > maxX)
                        maxX = x;
                    if (y > maxY)
                        maxY = y;
                }
            }
            lua_pop(L, 1);
        }
        if (!recipe.count)
            return luaL_error(L, "recipe cannot be empty");
        recipe.width = maxX - minX + 1;
        recipe.height = maxY - minY + 1;
        for (int y = 0; y < recipe.height; y++)
            for (int x = 0; x < recipe.width; x++) {
                recipe.ingredients[y * recipe.width + x] = cells[(y + minY) * 3 + x + minX];
                recipe.exact[y * recipe.width + x] = exact[(y + minY) * 3 + x + minX];
            }
    }
    if (!Crafting_Register(&recipe))
        return luaL_error(L, "recipe limit reached");
    return 0;
}
