/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_QUERIES_H
#define MIDLESS_LUA_QUERIES_H
#include "minilua.h"
#include "../scripthooks.h"
#include "../player.h"
int LuaQueries_Raycast(lua_State *state);
int LuaQueries_Entities(lua_State *state);
int LuaQueries_Players(lua_State *state);
int LuaQueries_Light(lua_State *state);
int LuaQueries_NearestPlayer(lua_State *state);
int LuaQueries_FindPath(lua_State *state);
int LuaQueries_CanWalk(lua_State *state);
int LuaQueries_RegisterAttack(lua_State *state);
void LuaQueries_Reset(void);
#endif
