/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_PLAYERS_H
#define MIDLESS_LUA_PLAYERS_H
#include "minilua.h"
#include "../player.h"
#include "../entity.h"

Entity *LuaPlayers_TestEntity(lua_State *state, int index);
void LuaPlayers_Push(Player *player);
Player *LuaPlayers_Check(void);
bool LuaPlayers_IsLeaving(const Player *player);
int LuaPlayers_RegisterJoin(lua_State *state);
int LuaPlayers_RegisterLeave(lua_State *state);
int LuaPlayers_RegisterLand(lua_State *state);
int LuaPlayers_RegisterClick(lua_State *state);
int LuaPlayers_GetById(lua_State *state);
int LuaPlayers_GetByName(lua_State *state);
int LuaPlayers_List(lua_State *state);
void LuaPlayers_Init(void);
void LuaPlayers_Shutdown(void);
#endif
