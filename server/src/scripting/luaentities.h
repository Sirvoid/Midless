#ifndef ISLEFORGE_LUA_ENTITIES_H
#define ISLEFORGE_LUA_ENTITIES_H
#include "../entity.h"
void LuaEntities_Init(void);
void LuaEntities_Shutdown(void);
int LuaEntities_Register(void);
int LuaEntities_Spawn(void);
void LuaEntities_Step(Entity *entity, float dt);
void LuaEntities_Remove(Entity *entity);
#endif
