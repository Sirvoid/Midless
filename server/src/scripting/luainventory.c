#include "luainventory.h"
#include "luametadata.h"
#include "luabindings.h"
#include "../world/world.h"
#include "../serverinventory.h"
#include "../items.h"
#include <string.h>
#include <math.h>
#include <limits.h>

extern lua_State *L;
#define PLAYER_INVENTORY "midless.PlayerInventory"
typedef struct PlayerInventory { int id; uint64_t connection; char name[65]; } PlayerInventory;
static int screenCallback = LUA_NOREF;

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
static ItemStack *Slots(lua_State *state, int index, int *count) {
    Player *p = Owner(state, index);
    PlayerInventory *handle = luaL_checkudata(state, index, PLAYER_INVENTORY);
    if (!handle->name[0]) { *count = INVENTORY_SLOT_COUNT; return p->inventory.slots; }
    NamedInventory *inventory = PlayerInventories_Get(p, handle->name);
    if (!inventory) luaL_error(state, "player inventory is undefined or its saved size differs");
    *count = inventory->count;
    return inventory->slots;
}
static int GetStack(lua_State *state) {
    int count; ItemStack *slots = Slots(state, 1, &count);
    ItemStack stack = slots[Integer(state, 2, 1, count) - 1];
    ServerItems_PushStack(state, stack);
    return 1;
}
static int SetStack(lua_State *state) {
    Player *p = Owner(state, 1);
    int count; ItemStack *slots = Slots(state, 1, &count);
    int slot = Integer(state, 2, 1, count) - 1;
    ItemStack stack = {0};
    if (!lua_isnoneornil(state, 3)) ServerItems_ReadStack(state, 3, &stack);
    slots[slot] = stack;
    p->inventoryRevision++;
    ServerInventory_UpdateHeldBlock(p);
    ServerInventory_Send(p);
    return 0;
}
static int AddItem(lua_State *state) {
    Player *p = Owner(state, 1);
    luaL_checktype(state, 2, LUA_TTABLE);
    lua_getfield(state, 2, "id"); int id = ServerItems_Id(state, -1, false, false); lua_pop(state, 1);
    if (!id) return luaL_error(state, "air is not an item");
    lua_getfield(state, 2, "count"); int count = Integer(state, -1, 1, INT_MAX); lua_pop(state, 1);
    ItemStack stack = {.itemId=id}; LuaMetadata_ReadItem(state, 2, &stack);
    int slotCount; ItemStack *slots = Slots(state, 1, &slotCount);
    ItemStack next[255]; memcpy(next, slots, slotCount * sizeof(ItemStack));
    bool added = Inventory_AddStackToSlots(next, slotCount, slots == p->inventory.slots ? INVENTORY_STORAGE_SLOTS : 0, stack, count) == count;
    if (added) {
        memcpy(slots, next, slotCount * sizeof(ItemStack));
        p->inventoryRevision++;
        ServerInventory_UpdateHeldBlock(p);
        ServerInventory_Send(p);
    }
    lua_pushboolean(state, added);
    return 1;
}
int LuaInventory_Get(lua_State *state, Player *p) {
    char name[65] = {0};
    if (!lua_isnoneornil(state, 2)) {
        size_t length; const char *value = luaL_checklstring(state, 2, &length);
        if (!length || length > 64 || memchr(value, 0, length)) return luaL_error(state, "invalid inventory name");
        memcpy(name, value, length);
        if (!PlayerInventories_Get(p, name)) return luaL_error(state, "player inventory is undefined or its saved size differs");
    }
    PlayerInventory *handle = lua_newuserdata(state, sizeof(*handle));
    *handle = (PlayerInventory){p->id, p->connectionId};
    strcpy(handle->name, name);
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
static int Bind(lua_State *state, Player *p, InventoryWindow *window, int index) {
    bool block = !luaL_testudata(state, index, PLAYER_INVENTORY);
    char name[65] = {0};
    int slots;
    if (!block) {
        if (Owner(state, index) != p) return luaL_error(state, "screen must use this player's inventories");
        PlayerInventory *handle = luaL_checkudata(state, index, PLAYER_INVENTORY);
        strcpy(name, handle->name);
        Slots(state, index, &slots);
        if (!name[0]) return 0;
    } else {
        if (!window->block) return luaL_error(state, "block inventories require layout.block");
        slots = LuaMetadata_CheckBlockInventory(state, index, window->position, name);
    }
    InventoryView *view = &window->view;
    for (int i = 1; i < view->bindingCount; i++) {
        if (window->bindings[i].block == block && !strcmp(window->bindings[i].name, name)) return i;
        if (block && window->bindings[i].block) return luaL_error(state, "a screen supports one block inventory");
    }
    if (view->bindingCount == INVENTORY_VIEW_BINDINGS || view->slotCount + slots > INVENTORY_VIEW_SLOTS)
        return luaL_error(state, "screen inventory capacity exceeded");
    int binding = view->bindingCount++;
    view->bindingSlots[binding] = slots;
    view->slotCount += slots;
    window->bindings[binding].block = block;
    strcpy(window->bindings[binding].name, name);
    return binding;
}
int LuaInventory_Show(lua_State *state, Player *p) {
    luaL_checktype(state, 2, LUA_TTABLE);
    InventoryWindow window = {0};
    InventoryView *v = &window.view;
    v->session = 1; // replaced with the player's next session on successful open
    v->bindingCount = 1; v->bindingSlots[0] = INVENTORY_SLOT_COUNT;
    Text(state, 2, "title", v->title);
    v->width = Number(state, 2, "width"); v->height = Number(state, 2, "height");
    lua_getfield(state, 2, "block");
    window.block = !lua_isnil(state, -1);
    if (window.block) {
        window.position = LuaMetadata_CheckBlock(state, -1);
        window.blockId = ServerWorld_GetBlock(window.position);
    }
    lua_pop(state, 1);
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
        e->crafting = !strcmp(type, "crafting_output");
        e->progress = !strcmp(type, "progress");
        lua_pop(state, 1);
        if (label) Text(state, element, "text", e->text);
        else if (e->progress) {
            e->width = Number(state, element, "width"); e->height = Number(state, element, "height");
            lua_getfield(state, element, "max");
            e->maximum = luaL_checknumber(state, -1); lua_pop(state, 1);
            lua_getfield(state, element, "value");
            if (lua_type(state, -1) == LUA_TSTRING) {
                if (!window.block) return luaL_error(state, "metadata progress requires layout.block");
                Text(state, element, "value", window.progressFields[i]);
                if (!LuaMetadata_Progress(window.position, window.progressFields[i], &e->value))
                    return luaL_error(state, "progress requires numeric block metadata");
            } else e->value = luaL_checknumber(state, -1);
            lua_pop(state, 1);
        }
        else if (e->grid || e->crafting) {
            lua_getfield(state, element, "slot");
            e->first = lua_isnil(state, -1) ? 0 : Integer(state, -1, 1, 255) - 1;
            lua_pop(state, 1);
            lua_getfield(state, element, "columns"); e->columns = Integer(state, -1, 1, 32); lua_pop(state, 1);
            lua_getfield(state, element, "rows"); e->rows = Integer(state, -1, 1, 32); lua_pop(state, 1);
            lua_getfield(state, element, "inventory");
            e->binding = Bind(state, p, &window, -1);
            lua_pop(state, 1);
            if (e->crafting) {
                Text(state, element, "recipes", window.recipes[i]);
                if (!window.recipes[i][0]) return luaL_error(state, "recipe group cannot be empty");
            }
        } else return luaL_error(state, "unknown inventory UI element type");
        lua_pop(state, 1);
    }
    if (!InventoryView_Validate(v)) return luaL_error(state, "screen grids must cover each inventory slot once, without overlapping, within the layout");
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
    screenCallback = LUA_NOREF;
    PlayerInventories_Reset();
    const luaL_Reg methods[] = {{"get_stack", GetStack}, {"set_stack", SetStack}, {"add_item", AddItem}, {NULL, NULL}};
    luaL_newmetatable(L, PLAYER_INVENTORY);
    lua_newtable(L); luaL_setfuncs(L, methods, 0); lua_setfield(L, -2, "__index");
    lua_pushliteral(L, PLAYER_INVENTORY); lua_setfield(L, -2, "__metatable");
    lua_pop(L, 1);
}
int LuaInventory_Define(void) {
    size_t length; const char *name = luaL_checklstring(L, 1, &length);
    if (!length || length > 64 || memchr(name, 0, length)) return luaL_error(L, "invalid inventory name");
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_getfield(L, 2, "slots"); int count = Integer(L, -1, 1, 255);
    if (!PlayerInventories_Define(name, count)) return luaL_error(L, "player inventory already defined or definition limit reached");
    return 0;
}
int LuaInventory_DefineScreen(void) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    luaL_unref(L, LUA_REGISTRYINDEX, screenCallback);
    lua_pushvalue(L, 1); screenCallback = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}
static int BuildPlayerScreen(lua_State *state) {
    Player *p = lua_touserdata(state, 1);
    lua_rawgeti(state, LUA_REGISTRYINDEX, screenCallback);
    LuaBindings_PushPlayer(p);
    lua_call(state, 1, 1);
    luaL_checktype(state, 2, LUA_TTABLE);
    lua_getfield(state, 2, "block");
    if (!lua_isnil(state, -1)) return luaL_error(state, "the player inventory screen cannot depend on a block");
    lua_pop(state, 1);
    return LuaInventory_Show(state, p);
}
bool LuaInventory_OpenPlayer(Player *p) {
    if (screenCallback == LUA_NOREF) return false;
    int top = lua_gettop(L);
    lua_pushcfunction(L, BuildPlayerScreen); lua_pushlightuserdata(L, p);
    bool ok = lua_pcall(L, 1, 1, 0) == LUA_OK;
    if (!ok) TraceLog(LOG_WARNING, "Player inventory screen: %s", lua_tostring(L, -1));
    else ok = lua_toboolean(L, -1);
    lua_settop(L, top);
    return ok;
}
void LuaInventory_Shutdown(void) {
    luaL_unref(L, LUA_REGISTRYINDEX, screenCallback);
    screenCallback = LUA_NOREF;
    PlayerInventories_Reset();
}
