/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "luaplayers.h"
#include "../digging.h"
#include "luaitems.h"
#include "minilua.h"
extern lua_State *L;
#include "luadigging.h"
#include "luaengine.h"
#include "luametadata.h"
#include "../items.h"
#include <math.h>
#include <string.h>

static int speeds[ITEM_LIMIT], finished[ITEM_LIMIT], callbacks[64], callbackCount;

void LuaDigging_Init(void) {
    ServerDigging_Reset();
    for (int i = 0; i < ITEM_LIMIT; i++)
        speeds[i] = finished[i] = LUA_NOREF;
    callbackCount = 0;
}
void LuaDigging_Shutdown(void) {
    for (int i = 0; i < ITEM_LIMIT; i++) {
        luaL_unref(L, LUA_REGISTRYINDEX, speeds[i]);
        luaL_unref(L, LUA_REGISTRYINDEX, finished[i]);
    }
    for (int i = 0; i < callbackCount; i++)
        luaL_unref(L, LUA_REGISTRYINDEX, callbacks[i]);
}
void LuaDigging_Define(int id, int table, bool block) {
    if (block) {
        lua_getfield(L, table, "hardness");
        double hardness = lua_isnil(L, -1) ? 1 : luaL_checknumber(L, -1);
        lua_pop(L, 1);
        if (!isfinite(hardness) || hardness < 0 || hardness > 86400)
            luaL_error(L, "hardness must be 0..86400");
        lua_getfield(L, table, "unbreakable");
        bool unbreakable = lua_toboolean(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, table, "dig_group");
        const char *group = lua_isnil(L, -1) ? "" : luaL_checkstring(L, -1);
        if (strlen(group) > 64)
            luaL_error(L, "dig_group is too long");
        BlockDigging definition = {.hardness = hardness, .unbreakable = unbreakable};
        strcpy(definition.group, group);
        ServerDigging_Define(id, &definition);
        lua_pop(L, 1);
    }
    lua_getfield(L, table, "dig_speed");
    if (!lua_isnil(L, -1)) {
        luaL_checktype(L, -1, LUA_TTABLE);
        lua_newtable(L); // Own a copy so later Lua edits cannot bypass validation.
        lua_pushnil(L);
        while (lua_next(L, -3)) {
            if (lua_type(L, -2) != LUA_TSTRING)
                luaL_error(L, "dig_speed keys must be group names");
            double value = luaL_checknumber(L, -1);
            if (!isfinite(value) || value <= 0)
                luaL_error(L, "dig_speed must be positive and finite");
            lua_pushvalue(L, -2);
            lua_pushvalue(L, -2);
            lua_settable(L, -5);
            lua_pop(L, 1);
        }
        lua_remove(L, -2);
    }
    int speed = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_getfield(L, table, "on_dig");
    if (!lua_isnil(L, -1))
        luaL_checktype(L, -1, LUA_TFUNCTION);
    luaL_unref(L, LUA_REGISTRYINDEX, speeds[id]);
    speeds[id] = speed;
    luaL_unref(L, LUA_REGISTRYINDEX, finished[id]);
    finished[id] = luaL_ref(L, LUA_REGISTRYINDEX);
}
int LuaDigging_Register(void) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    if (callbackCount == 64)
        return luaL_error(L, "too many dig time callbacks");
    lua_pushvalue(L, 1);
    callbacks[callbackCount++] = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}
static bool Call(int arguments, int results) {
    if (lua_pcall(L, arguments, results, 0) == LUA_OK)
        return true;
    TraceLog(LOG_WARNING, "Digging callback: %s", lua_tostring(L, -1));
    lua_pop(L, 1);
    return false;
}
double ScriptHooks_DiggingTime(Player *player, Vector3 position, ItemStack stack, double seconds) {
    for (int i = 0; i < callbackCount; i++) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks[i]);
        LuaPlayers_Push(player);
        LuaMetadata_PushBlock(L, position);
        LuaItems_PushStack(L, stack);
        lua_pushnumber(L, seconds);
        if (!Call(4, 1))
            return -1;
        if (lua_isboolean(L, -1) && !lua_toboolean(L, -1)) {
            lua_pop(L, 1);
            return -1;
        }
        if (!lua_isnil(L, -1)) {
            if (lua_type(L, -1) != LUA_TNUMBER) {
                lua_pop(L, 1);
                return -1;
            }
            seconds = lua_tonumber(L, -1);
        }
        lua_pop(L, 1);
        if (!isfinite(seconds) || seconds < 0 || seconds > 86400)
            return -1;
    }
    return isfinite(seconds) && seconds <= 86400 ? seconds : -1;
}
void ScriptHooks_DiggingFinished(Player *player, Vector3 position, ItemStack stack) {
    if (!stack.count || stack.itemId >= ITEM_LIMIT || finished[stack.itemId] < 0)
        return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, finished[stack.itemId]);
    LuaPlayers_Push(player);
    LuaMetadata_PushBlock(L, position);
    LuaItems_PushStack(L, stack);
    Call(3, 0);
}

double ScriptHooks_DiggingSpeed(ItemStack stack, const char *group) {
    double speed = 1;
    if (stack.count && stack.itemId < ITEM_LIMIT && speeds[stack.itemId] >= 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, speeds[stack.itemId]);
        lua_getfield(L, -1, group);
        if (lua_isnumber(L, -1))
            speed = lua_tonumber(L, -1);
        lua_pop(L, 2);
    }
    return speed;
}
