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
int LuaEntities_Register(void);
int LuaEntities_Spawn(void);
int LuaEntities_Instance(const Entity *entity);
#endif
