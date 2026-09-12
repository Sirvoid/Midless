/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_TEXTURES_H
#define MIDLESS_LUA_TEXTURES_H
#include "minilua.h"

int LuaTextures_Define(lua_State *state);
int LuaTextures_SetTerrain(lua_State *state);
int LuaTextures_SetBreaking(lua_State *state);
#endif
