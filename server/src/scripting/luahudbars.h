/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_HUDBARS_H
#define MIDLESS_LUA_HUDBARS_H
#include "minilua.h"
#include "../player.h"

int LuaHudBars_Define(lua_State *L);
int LuaHudBars_Remove(lua_State *L);
int LuaHudBars_Set(lua_State *L, Player *player);
#endif
