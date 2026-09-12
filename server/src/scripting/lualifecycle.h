/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_LIFECYCLE_H
#define MIDLESS_LUA_LIFECYCLE_H
#include "minilua.h"

int LuaLifecycle_RegisterReady(lua_State *state);
int LuaLifecycle_RegisterStep(lua_State *state);
int LuaLifecycle_Sleep(lua_State *state);
void LuaLifecycle_Init(void);
void LuaLifecycle_Shutdown(void);
#endif
