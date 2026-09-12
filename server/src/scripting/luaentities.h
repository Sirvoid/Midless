/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_ENTITIES_H
#define MIDLESS_LUA_ENTITIES_H
#include "../scripthooks.h"
#include "../entity.h"
#include "minilua.h"
#include "../entityregistry.h"
Entity *LuaEntities_Check(lua_State *state, int index);
Entity *LuaEntities_Test(lua_State *state, int index);
void LuaEntities_Push(Entity *entity);
void LuaEntities_Init(void);
void LuaEntities_Shutdown(void);
int LuaEntities_Register(lua_State *state);
int LuaEntities_Spawn(lua_State *state);
int LuaEntities_Instance(const Entity *entity);
#endif
