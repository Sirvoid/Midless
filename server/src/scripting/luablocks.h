/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_BLOCKS_H
#define MIDLESS_LUA_BLOCKS_H
#include "minilua.h"

int LuaBlocks_SetBlock(lua_State *state);
int LuaBlocks_SetBlocks(lua_State *state);
int LuaBlocks_DefineBlock(lua_State *state);
int LuaBlocks_RegisterBlockUpdate(lua_State *state);
void LuaBlocks_Init(void);
void LuaBlocks_Shutdown(void);
#endif
