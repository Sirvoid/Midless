/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_DAMAGE_H
#define MIDLESS_LUA_DAMAGE_H
#include "luaentities.h"
Vector3 LuaDamage_Impulse(lua_State *state, int context, Entity *target);
int LuaDamage_Result(lua_State *state, int index, int amount);
int LuaDamage_RegisterPlayer(void);
int LuaDamage_PlayerHooks(Entity *target, int context, int amount);
void LuaDamage_Reset(void);
#endif
