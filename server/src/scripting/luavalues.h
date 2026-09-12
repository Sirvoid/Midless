/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_VALUES_H
#define MIDLESS_LUA_VALUES_H
#include <stdbool.h>
#include "raylib.h"

typedef struct LuaConstant {
    const char *name;
    int value;
} LuaConstant;

void LuaValues_PushPosition(Vector3 position);
Vector3 LuaValues_ReadPosition(int argument, bool blockPosition);
int LuaValues_IntField(int table, const char *name, int fallback, int min, int max);
void LuaValues_ConstantTable(const LuaConstant *constants);
#endif
