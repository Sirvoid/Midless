/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "luaitems.h"
#include "luaitemactions.h"
#include "luadigging.h"
#include "luametadata.h"
#include "../world/textures.h"
#include <string.h>
#include <ctype.h>

extern lua_State *L;

void LuaItems_PushId(lua_State *state, int id) {
    if (id >= 0 && id < ITEM_LIMIT && serverItems[id].identifier[0])
        lua_pushstring(state, serverItems[id].identifier);
    else
        lua_pushinteger(state, id);
}

static const char *CheckIdentifier(lua_State *state, int index) {
    size_t length;
    const char *name = luaL_checklstring(state, index, &length);
    if (!length || length > 64 || memchr(name, 0, length))
        luaL_error(state, "invalid item identifier");
    const char *colon = strchr(name, ':');
    if (!colon || colon == name || !colon[1] || strchr(colon + 1, ':'))
        luaL_error(state, "use a namespaced identifier such as example:gem");
    for (size_t i = 0; i < length; i++)
        if (!(islower((unsigned char)name[i]) || isdigit((unsigned char)name[i]) ||
              strchr("_:./-", name[i])))
            luaL_error(state, "invalid character in item identifier");
    return name;
}

int LuaItems_Id(lua_State *state, int index, bool block, bool reserve) {
    if (lua_type(state, index) == LUA_TNUMBER) {
        lua_Integer id = luaL_checkinteger(state, index);
        if (id < 0 || id >= (block ? 256 : 65536))
            return luaL_error(state, "item ID out of range");
        return id;
    }
    const char *name = CheckIdentifier(state, index);
    int id = ServerItems_Find(name);
    if (id >= 0) {
        if (block && id >= 256)
            return luaL_error(state, "identifier belongs to an ordinary item");
        return id;
    }
    if (!reserve || !ServerItems_Ready())
        return luaL_error(state, "unknown item '%s'", name);
    const char *error = ServerItems_Reserve(name, block, &id);
    if (error)
        return luaL_error(state, "%s", error);
    return id;
}

int LuaItems_Define(lua_State *state) {
    (void)state;
    int id = LuaItems_Declare(L, false);
    if (id < 256 || id >= ITEM_LIMIT || serverItems[id].defined)
        return luaL_error(L, "item already defined or ID out of range");
    luaL_checktype(L, 2, LUA_TTABLE);
    ItemDefinition definition = serverItems[id];
    lua_getfield(L, 2, "name");
    size_t length;
    const char *name = luaL_checklstring(L, -1, &length);
    if (length > 64 || memchr(name, 0, length))
        return luaL_error(L, "item name is too long");
    memcpy(definition.name, name, length + 1);
    lua_pop(L, 1);
    lua_getfield(L, 2, "max_stack");
    int max = lua_isnil(L, -1) ? 64 : luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    if (max < 1 || max > 64)
        return luaL_error(L, "max_stack must be 1..64");
    lua_getfield(L, 2, "texture");
    int texture = ServerTextures_Find(luaL_checkstring(L, -1));
    if (texture < 0)
        return luaL_error(L, "unknown item texture; register it with define_texture first");
    definition.texture = texture;
    lua_pop(L, 1);
    if (!ServerTextures_ItemSize(definition.texture))
        return luaL_error(L, "item textures must be at most 64 by 64 pixels");
    lua_getfield(L, 2, "held_model");
    if (!lua_isnil(L, -1) && strcmp(luaL_checkstring(L, -1), "sprite"))
        return luaL_error(L, "ordinary items use held_model = sprite");
    lua_pop(L, 1);
    definition.maxStack = max;
    definition.defined = true;
    LuaDigging_Define(id, 2, false);
    LuaItemActions_Define(id, 2, false);
    LuaMetadata_DefineItem(id, 2);
    LuaMetadata_ItemBar(id, 2, &definition.bar);
    ServerItems_Define(id, &definition);
    return 0;
}

int LuaItems_Declare(lua_State *state, bool block) {
    if (!ServerItems_Ready())
        return luaL_error(state, "item name mapping could not be loaded");
    luaL_checktype(state, 2, LUA_TTABLE);
    int id = LuaItems_Id(state, 1, block, true);
    if (lua_type(state, 1) == LUA_TNUMBER) {
        const char *error = ServerItems_ReserveLegacy(id, block);
        if (error)
            return luaL_error(state, "%s", error);
    }
    return id;
}

void LuaItems_ReadStack(lua_State *state, int index, ItemStack *stack) {
    *stack = (ItemStack){0};
    if (lua_isnil(state, index))
        return;
    index = lua_absindex(state, index);
    luaL_checktype(state, index, LUA_TTABLE);
    lua_getfield(state, index, "id");
    stack->itemId = LuaItems_Id(state, -1, false, false);
    lua_pop(state, 1);
    lua_getfield(state, index, "count");
    int count = luaL_checkinteger(state, -1);
    lua_pop(state, 1);
    if (!stack->itemId || count < 1 || count > Item_GetMaxStack(stack->itemId))
        luaL_error(state, "invalid stack count");
    stack->count = count;
    LuaMetadata_ReadItem(state, index, stack);
}

void LuaItems_PushStack(lua_State *state, ItemStack stack) {
    if (!stack.count) {
        lua_pushnil(state);
        return;
    }
    lua_createtable(state, 0, 3);
    if (stack.itemId < ITEM_LIMIT && serverItems[stack.itemId].identifier[0])
        lua_pushstring(state, serverItems[stack.itemId].identifier);
    else
        lua_pushinteger(state, stack.itemId);
    lua_setfield(state, -2, "id");
    lua_pushinteger(state, stack.count);
    lua_setfield(state, -2, "count");
    LuaMetadata_PushItem(state, stack);
    lua_setfield(state, -2, "metadata");
    if (stack.metadataSize) {
        lua_pushlstring(state, (const char *)stack.metadata, stack.metadataSize);
        lua_setfield(state, -2, "_metadata");
        lua_pushinteger(state, stack.metadataVersion);
        lua_setfield(state, -2, "_metadata_version");
    }
}
