/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "luatextures.h"
#include "luaengine.h"
#include "minilua.h"
#include "../world/textures.h"

extern lua_State *L;

int LuaTextures_Define(lua_State *state) {
    (void)state;
    char name[65];
    Lua_CopyString(1, name, sizeof(name));
    char path[1024];
    Lua_CopyString(2, path, sizeof(path));
    if (!ServerTextures_Define(name, path))
        return Lua_Error("texture registration failed: invalid PNG, limits exceeded, reserved "
                         "name, or incompatible dimensions");
    return 0;
}

int LuaTextures_SetTerrain(lua_State *state) {
    (void)state;
    int id = ServerTextures_Find(Lua_GetString(1));
    if (!ServerTextures_SetTerrain(id))
        return Lua_Error("terrain texture must be a defined 256x256 PNG or 'terrain'");
    return 0;
}

int LuaTextures_SetBreaking(lua_State *state) {
    (void)state;
    int id = ServerTextures_Find(luaL_checkstring(L, 1));
    if (!ServerTextures_SetBreaking(id)) {
        return luaL_error(L, "breaking texture must contain ten square frames horizontally, up to "
                             "64 pixels per frame");
    }
    return 0;
}
