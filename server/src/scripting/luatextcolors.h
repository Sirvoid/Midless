/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_TEXTCOLORS_H
#define MIDLESS_LUA_TEXTCOLORS_H
#include "minilua.h"
#include "../entity.h"

int LuaTextColors_Define(lua_State *state);
int LuaTextColors_Remove(lua_State *state);
int LuaTextColors_Escape(lua_State *state);
int LuaNametag_Set(lua_State *state, Entity *entity);
#endif
