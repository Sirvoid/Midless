#include "luainventory.h"
#include "luametadata.h"
#include "../world/world.h"
#include "../serverinventory.h"
#include <string.h>
#include <math.h>
#include <limits.h>

extern lua_State *L;
#define PLAYER_INVENTORY "midless.PlayerInventory"
typedef struct PlayerInventory { int id; uint64_t connection; } PlayerInventory;

static Player *Owner(lua_State *state, int index) {
    PlayerInventory *handle = luaL_checkudata(state, index, PLAYER_INVENTORY);
    Player *p = serverWorld.players ? serverWorld.players[handle->id] : NULL;
    if (!p || p->disconnected || p->connectionId != handle->connection)
        luaL_error(state, "player is no longer connected");
    return p;
}
static int Integer(lua_State *state, int index, int low, int high) {
    lua_Integer n = luaL_checkinteger(state, index);
    if (n < low || n > high) luaL_error(state, "inventory value out of range");
    return n;
}
static int GetStack(lua_State *state) {
    Player *p = Owner(state, 1);
    ItemStack stack = p->inventory.slots[Integer(state, 2, 1, INVENTORY_SLOT_COUNT) - 1];
    if (!stack.count) { lua_pushnil(state); return 1; }
    lua_createtable(state, 0, 2);
    lua_pushinteger(state, stack.itemId); lua_setfield(state, -2, "id");
    lua_pushinteger(state, stack.count); lua_setfield(state, -2, "count");
    return 1;
}
static int SetStack(lua_State *state) {
    Player *p = Owner(state, 1);
    int slot = Integer(state, 2, 1, INVENTORY_SLOT_COUNT) - 1;
    ItemStack stack = {0};
    if (!lua_isnoneornil(state, 3)) {
        luaL_checktype(state, 3, LUA_TTABLE);
        lua_getfield(state, 3, "id"); stack.itemId = Integer(state, -1, 1, 65535); lua_pop(state, 1);
        lua_getfield(state, 3, "count"); stack.count = Integer(state, -1, 1, Item_GetMaxStack(stack.itemId)); lua_pop(state, 1);
    }
    p->inventory.slots[slot] = stack;
    p->inventoryRevision++;
    ServerInventory_UpdateHeldBlock(p);
    ServerInventory_Send(p);
    return 0;
}
static int AddItem(lua_State *state) {
    Player *p = Owner(state, 1);
    luaL_checktype(state, 2, LUA_TTABLE);
    lua_getfield(state, 2, "id"); int id = Integer(state, -1, 1, 65535); lua_pop(state, 1);
    lua_getfield(state, 2, "count"); int count = Integer(state, -1, 1, INT_MAX); lua_pop(state, 1);
    bool added = Inventory_Add(&p->inventory, id, count);
    if (added) {
        p->inventoryRevision++;
        ServerInventory_UpdateHeldBlock(p);
        ServerInventory_Send(p);
    }
    lua_pushboolean(state, added);
    return 1;
}
int LuaInventory_Get(lua_State *state, Player *p) {
    PlayerInventory *handle = lua_newuserdata(state, sizeof(*handle));
    *handle = (PlayerInventory){p->id, p->connectionId};
    luaL_setmetatable(state, PLAYER_INVENTORY);
    return 1;
}
static void Text(lua_State *state, int table, const char *key, char value[65]) {
    lua_getfield(state, table, key);
    size_t length;
    const char *s = luaL_checklstring(state, -1, &length);
    if (length > 64 || memchr(s, 0, length)) luaL_error(state, "UI text supports up to 64 bytes without NUL");
    memcpy(value, s, length); value[length] = 0;
    lua_pop(state, 1);
}
static float Number(lua_State *state, int table, const char *key) {
    lua_getfield(state, table, key);
    double n = luaL_checknumber(state, -1);
    if (!isfinite(n) || n < 0 || n > 32) luaL_error(state, "UI dimensions must be between 0 and 32");
    lua_pop(state, 1);
    return n;
}
int LuaInventory_Show(lua_State *state, Player *p) {
    luaL_checktype(state, 2, LUA_TTABLE);
    InventoryWindow window = {0};
    InventoryView *v = &window.view;
    v->session = 1; // replaced with the player's next session on successful open
    Text(state, 2, "title", v->title);
    v->width = Number(state, 2, "width"); v->height = Number(state, 2, "height");
    lua_getfield(state, 2, "block"); window.position = LuaMetadata_CheckBlock(state, -1); lua_pop(state, 1);
    window.blockId = ServerWorld_GetBlock(window.position);
    lua_getfield(state, 2, "elements");
    luaL_checktype(state, -1, LUA_TTABLE);
    size_t count = lua_rawlen(state, -1);
    if (!count || count > INVENTORY_VIEW_ELEMENTS) return luaL_error(state, "inventory UI supports 1..16 elements");
    v->count = count;
    for (int i = 0; i < v->count; i++) {
        InventoryElement *e = &v->elements[i];
        lua_rawgeti(state, -1, i + 1);
        luaL_checktype(state, -1, LUA_TTABLE);
        int element = lua_gettop(state);
        e->x = Number(state, element, "x"); e->y = Number(state, element, "y");
        lua_getfield(state, element, "type"); const char *type = luaL_checkstring(state, -1);
        bool label = !strcmp(type, "label"); e->grid = !strcmp(type, "inventory");
        lua_pop(state, 1);
        if (label) Text(state, element, "text", e->text);
        else if (e->grid) {
            lua_getfield(state, element, "columns"); e->columns = Integer(state, -1, 1, 32); lua_pop(state, 1);
            lua_getfield(state, element, "rows"); e->rows = Integer(state, -1, 1, 32); lua_pop(state, 1);
            lua_getfield(state, element, "inventory");
            if (luaL_testudata(state, -1, PLAYER_INVENTORY)) {
                if (Owner(state, -1) != p) return luaL_error(state, "screen must use this player's inventory");
            } else {
                e->container = true;
                v->slotCount = LuaMetadata_CheckBlockInventory(state, -1, window.position, window.field);
            }
            lua_pop(state, 1);
        } else return luaL_error(state, "unknown inventory UI element type");
        lua_pop(state, 1);
    }
    if (!InventoryView_Validate(v)) return luaL_error(state, "screen requires one non-overlapping grid per inventory, covering all slots within its dimensions");
    lua_pushboolean(state, InventoryWindow_Open(p, &window));
    return 1;
}
int LuaInventory_Close(lua_State *state, Player *p) {
    bool closed = InventoryWindow_Close(p);
    p->inventoryRevision++;
    ServerInventory_UpdateHeldBlock(p);
    ServerInventory_Send(p);
    lua_pushboolean(state, closed);
    return 1;
}
void LuaInventory_Init(void) {
    const luaL_Reg methods[] = {{"get_stack", GetStack}, {"set_stack", SetStack}, {"add_item", AddItem}, {NULL, NULL}};
    luaL_newmetatable(L, PLAYER_INVENTORY);
    lua_newtable(L); luaL_setfuncs(L, methods, 0); lua_setfield(L, -2, "__index");
    lua_pushliteral(L, PLAYER_INVENTORY); lua_setfield(L, -2, "__metatable");
    lua_pop(L, 1);
}
