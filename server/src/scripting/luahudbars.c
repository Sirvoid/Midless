/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "luahudbars.h"
#include "../hudbars.h"
#include "../world/textures.h"
#include <string.h>

static const char *ReadName(lua_State *L, int index) {
    size_t length;
    const char *name = luaL_checklstring(L, index, &length);
    const char *separator = strchr(name, ':');
    if (!length || length > 64 || memchr(name, 0, length) || !separator || separator == name ||
        !separator[1])
        luaL_error(L, "expected a namespaced HUD bar name (mod:name)");
    return name;
}

static int ReadNumber(lua_State *L, int table, const char *field, int fallback, int min, int max) {
    lua_getfield(L, table, field);
    lua_Integer value = lua_isnil(L, -1) ? fallback : luaL_checkinteger(L, -1);
    if (value < min || value > max)
        luaL_error(L, "%s must be between %d and %d", field, min, max);
    lua_pop(L, 1);
    return (int)value;
}

int LuaHudBars_Define(lua_State *L) {
    const char *name = ReadName(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    HudBarDefinition bar = {.defined = true};
    lua_getfield(L, 2, "texture");
    int texture = ServerTextures_Find(luaL_checkstring(L, -1));
    if (!ServerTextures_HudSize(texture))
        return luaL_error(L, "HUD bar texture must be a defined 9x9 PNG");
    lua_pop(L, 1);
    bar.texture = texture;
    bar.icons = ReadNumber(L, 2, "icons", 10, 1, HUD_BAR_MAX_ICONS);
    bar.priority = ReadNumber(L, 2, "priority", 0, 0, 65535);
    bar.max = ReadNumber(L, 2, "max", 0, 1, 65535);
    const char *error = ServerHudBars_Define(name, &bar);
    if (error)
        return luaL_error(L, "%s", error);
    return 0;
}

int LuaHudBars_Set(lua_State *L, Player *player) {
    int id = ServerHudBars_Find(ReadName(L, 2));
    if (id < 0)
        return luaL_error(L, "HUD bar is not defined");
    luaL_checktype(L, 3, LUA_TTABLE);
    HudBarState state = player->hudBars[id];
    state.value = ReadNumber(L, 3, "value", state.value, 0, 65535);
    lua_getfield(L, 3, "visible");
    if (!lua_isnil(L, -1)) {
        luaL_checktype(L, -1, LUA_TBOOLEAN);
        state.visible = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);
    ServerHudBars_Set(player, id, state);
    return 0;
}

int LuaHudBars_Remove(lua_State *L) {
    ServerHudBars_Remove(ReadName(L, 1));
    return 0;
}
