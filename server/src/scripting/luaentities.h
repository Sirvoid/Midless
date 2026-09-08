#ifndef MIDLESS_LUA_ENTITIES_H
#define MIDLESS_LUA_ENTITIES_H
#include "../entity.h"
#include "minilua.h"
Entity *LuaEntities_Check(lua_State *state, int index);
void LuaEntities_Init(void);
void LuaEntities_Shutdown(void);
int LuaEntities_Register(void);
int LuaEntities_Spawn(void);
void LuaEntities_Step(Entity *entity, float dt);
void LuaEntities_Remove(Entity *entity);
const char *LuaEntities_Name(int definition);
int LuaEntities_Find(const char *name);
int LuaEntities_MetadataSchema(int definition);
bool LuaEntities_ShouldSave(int definition);
int LuaEntities_Restore(int definition, Vector3 position);
void LuaEntities_Loaded(Entity *entity);
void LuaEntities_Detach(Entity *entity);
#endif
