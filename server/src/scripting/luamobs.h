#ifndef MIDLESS_LUA_MOBS_H
#define MIDLESS_LUA_MOBS_H
#include "luaentities.h"
int LuaMobs_Register(void);
void LuaMobs_Physics(Entity *entity, float dt);
void LuaMobs_Reset(void);
int LuaMobs_Follow(lua_State *state);
int LuaMobs_Wander(lua_State *state);
int LuaMobs_Steer(lua_State *state);
int LuaMobs_Teleport(lua_State *state);
#endif
