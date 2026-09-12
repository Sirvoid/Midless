/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_DIGGING_H
#define MIDLESS_LUA_DIGGING_H
#include "minilua.h"
#include "../scripthooks.h"
#include "../player.h"
void LuaDigging_Init(void);
void LuaDigging_Shutdown(void);
void LuaDigging_Define(int id, int table, bool block);
int LuaDigging_Register(lua_State *state);
#endif
