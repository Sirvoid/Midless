/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_MODELS_H
#define MIDLESS_LUA_MODELS_H
#include <stdbool.h>
int LuaModels_Resolve(int argument, bool defining);
void LuaModels_BindName(int argument, int id);
int LuaModels_Define(void);
int LuaModels_Remove(void);
int LuaModels_SetEntity(void);
void LuaModels_Init(void);
#endif
