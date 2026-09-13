/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_BLOCK_PHYSICS_H
#define MIDLESS_LUA_BLOCK_PHYSICS_H
#include "minilua.h"
#include "../world/blockphysics.h"
BlockPhysicsDefinition LuaBlockPhysics_Read(lua_State *L, int table, BlockDefinition *block);
void LuaBlockPhysics_Define(lua_State *L, int id, int table);
void LuaBlockPhysics_Init(void);
void LuaBlockPhysics_Shutdown(void);
int LuaBlockPhysics_Move(lua_State *L);
int LuaBlockPhysics_Schedule(lua_State *L);
int LuaBlockPhysics_SetState(lua_State *L);
void LuaBlockPhysics_CheckField(lua_State *L, int id, int key, int value);
void LuaBlockPhysics_ReadState(lua_State *L, int id, int table, Metadata *value);
#endif
