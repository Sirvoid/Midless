#include "crafting.h"
#include "minilua.h"
#include <string.h>

extern lua_State *L;
#define MAX_RECIPES 1024
typedef struct Recipe {
    char group[65];
    bool shapeless;
    int width, height, count;
    uint16_t ingredients[CRAFTING_SLOTS];
    ItemStack output;
} Recipe;
static Recipe recipes[MAX_RECIPES];
static int recipeCount;

static int Integer(int index, int low, int high) {
    lua_Integer value = luaL_checkinteger(L, index);
    if (value < low || value > high) luaL_error(L, "recipe value out of range");
    return value;
}
int Crafting_Register(void) {
    luaL_checktype(L, 1, LUA_TTABLE);
    if (recipeCount == MAX_RECIPES) return luaL_error(L, "recipe limit reached");
    Recipe recipe = {0};
    lua_getfield(L, 1, "group");
    size_t length;
    const char *group = lua_isnil(L, -1) ? "crafting" : luaL_checklstring(L, -1, &length);
    if (lua_isnil(L, -1)) length = strlen(group);
    if (!length || length > 64 || memchr(group, 0, length)) return luaL_error(L, "invalid recipe group");
    memcpy(recipe.group, group, length); lua_pop(L, 1);
    lua_getfield(L, 1, "output"); luaL_checktype(L, -1, LUA_TTABLE);
    lua_getfield(L, -1, "id"); recipe.output.itemId = Integer(-1, 1, 65535); lua_pop(L, 1);
    lua_getfield(L, -1, "count"); recipe.output.count = Integer(-1, 1, Item_GetMaxStack(recipe.output.itemId)); lua_pop(L, 2);
    lua_getfield(L, 1, "pattern");
    lua_getfield(L, 1, "ingredients");
    if (lua_isnil(L, -1) == lua_isnil(L, -2)) return luaL_error(L, "provide pattern or ingredients, not both");
    recipe.shapeless = !lua_isnil(L, -1);
    if (recipe.shapeless) {
        luaL_checktype(L, -1, LUA_TTABLE);
        int count = lua_rawlen(L, -1);
        if (count < 1 || count > CRAFTING_SLOTS) return luaL_error(L, "recipes require 1..9 ingredients");
        recipe.count = count;
        for (int i = 0; i < count; i++) {
            lua_rawgeti(L, -1, i + 1); recipe.ingredients[i] = Integer(-1, 1, 65535); lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
        luaL_checktype(L, -1, LUA_TTABLE);
        int height = lua_rawlen(L, -1), width = 0;
        if (height < 1 || height > CRAFTING_SIZE) return luaL_error(L, "patterns support 1..3 rows");
        uint16_t cells[CRAFTING_SLOTS] = {0};
        int minX = 3, minY = 3, maxX = -1, maxY = -1;
        for (int y = 0; y < height; y++) {
            lua_rawgeti(L, -1, y + 1); luaL_checktype(L, -1, LUA_TTABLE);
            int rowWidth = lua_rawlen(L, -1);
            if (rowWidth < 1 || rowWidth > 3 || (y && rowWidth != width))
                return luaL_error(L, "pattern rows must have the same width, 1..3; use 0 for empty cells");
            width = rowWidth;
            for (int x = 0; x < width; x++) {
                lua_rawgeti(L, -1, x + 1); int id = Integer(-1, 0, 65535); lua_pop(L, 1);
                cells[y * 3 + x] = id;
                if (id) {
                    recipe.count++;
                    if (x < minX) minX = x;
                    if (y < minY) minY = y;
                    if (x > maxX) maxX = x;
                    if (y > maxY) maxY = y;
                }
            }
            lua_pop(L, 1);
        }
        if (!recipe.count) return luaL_error(L, "recipe cannot be empty");
        recipe.width = maxX - minX + 1; recipe.height = maxY - minY + 1;
        for (int y = 0; y < recipe.height; y++) for (int x = 0; x < recipe.width; x++)
            recipe.ingredients[y * recipe.width + x] = cells[(y + minY) * 3 + x + minX];
    }
    recipes[recipeCount++] = recipe;
    return 0;
}
void Crafting_Reset(void) { recipeCount = 0; }

bool Crafting_Find(const char *group, const ItemStack *slots, int columns, int rows, CraftingMatch *match) {
    *match = (CraftingMatch){0};
    if (columns < 1 || rows < 1 || columns > 3 || rows > 3) return false;
    int minX = columns, minY = rows, maxX = -1, maxY = -1, occupied = 0;
    for (int y = 0; y < rows; y++) for (int x = 0; x < columns; x++) if (slots[y * columns + x].count) {
        occupied++;
        if (x < minX) minX = x;
        if (y < minY) minY = y;
        if (x > maxX) maxX = x;
        if (y > maxY) maxY = y;
    }
    for (int r = 0; r < recipeCount; r++) {
        const Recipe *recipe = &recipes[r];
        if (strcmp(group, recipe->group) || occupied != recipe->count) continue;
        bool ok = true;
        if (recipe->shapeless) {
            bool used[CRAFTING_SLOTS] = {0};
            for (int i = 0; i < rows * columns && ok; i++) if (slots[i].count) {
                int j;
                for (j = 0; j < recipe->count; j++) if (!used[j] && recipe->ingredients[j] == slots[i].itemId) { used[j] = true; break; }
                if (j == recipe->count) ok = false;
            }
        } else {
            if (maxX - minX + 1 != recipe->width || maxY - minY + 1 != recipe->height) continue;
            for (int y = 0; y < recipe->height && ok; y++) for (int x = 0; x < recipe->width; x++) {
                const ItemStack *slot = &slots[(y + minY) * columns + x + minX];
                if ((slot->count ? slot->itemId : 0) != recipe->ingredients[y * recipe->width + x]) { ok = false; break; }
            }
        }
        if (!ok) continue;
        match->recipe = r;
        match->output = recipe->output;
        for (int i = 0; i < rows * columns; i++) match->consume[i] = slots[i].count ? 1 : 0;
        return true;
    }
    return false;
}
