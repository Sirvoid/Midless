/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_ITEMS_H
#define MIDLESS_LUA_ITEMS_H
#include "minilua.h"
#include "../items.h"

int LuaItems_Id(lua_State *state, int index, bool block, bool reserve);
int LuaItems_Declare(lua_State *state, bool block);
void LuaItems_PushId(lua_State *state, int id);
void LuaItems_ReadStack(lua_State *state, int index, ItemStack *stack);
void LuaItems_PushStack(lua_State *state, ItemStack stack);
int LuaItems_Define(void);
#endif
