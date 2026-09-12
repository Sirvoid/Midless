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

int LuaTextColors_Define(void);
int LuaTextColors_Remove(void);
int LuaTextColors_Escape(void);
int LuaNametag_Set(lua_State *state, Entity *entity);
#endif
