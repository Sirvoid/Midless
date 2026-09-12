/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_MOBS_H
#define MIDLESS_LUA_MOBS_H
#include "../scripthooks.h"
#include "luaentities.h"
int LuaMobs_Register(void);
void LuaMobs_Reset(void);
int LuaMobs_Follow(lua_State *state);
int LuaMobs_Wander(lua_State *state);
int LuaMobs_Steer(lua_State *state);
int LuaMobs_Teleport(lua_State *state);
#endif
