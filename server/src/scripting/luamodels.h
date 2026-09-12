/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_MODELS_H
#define MIDLESS_LUA_MODELS_H
#include "minilua.h"
#include <stdbool.h>
int LuaModels_Resolve(int argument, bool defining);
void LuaModels_BindName(int argument, int id);
int LuaModels_Define(lua_State *state);
int LuaModels_Remove(lua_State *state);
int LuaModels_SetEntity(lua_State *state);
void LuaModels_Init(void);
#endif
