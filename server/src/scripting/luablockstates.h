/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_BLOCKSTATES_H
#define MIDLESS_LUA_BLOCKSTATES_H
#include "minilua.h"
#include "blockdefinition.h"

void LuaBlockStates_Define(lua_State *L, int id, int table, BlockDefinition *base);
#endif
