/**
 * Copyright (c) 2021 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "luavalues.h"
#include "luaengine.h"
#include <math.h>

void LuaValues_PushPosition(Vector3 position) {
    Lua_MakeTable(3);
    Lua_PushNumber(position.x);
    Lua_SetField(-2, "x");
    Lua_PushNumber(position.y);
    Lua_SetField(-2, "y");
    Lua_PushNumber(position.z);
    Lua_SetField(-2, "z");
}

Vector3 LuaValues_ReadPosition(int arg, bool blockPosition) {
    Lua_CheckTable(arg);
    const char *fields[] = {"x", "y", "z"};
    float values[3];
    for (int i = 0; i < 3; i++) {
        Lua_PushField(arg, fields[i]);
        values[i] = blockPosition ? Lua_GetIntRange(-1, -33554430, 33554430) : Lua_GetNumber(-1);
        Lua_Pop();
        if (!isfinite(values[i]) || fabsf(values[i]) > 33554430.0f)
            Lua_Error(
                "position coordinates must be finite and within the network coordinate range");
    }
    return (Vector3){values[0], values[1], values[2]};
}

int LuaValues_IntField(int table, const char *name, int fallback, int min, int max) {
    int value = fallback;
    if (Lua_PushField(table, name))
        value = Lua_GetIntRange(-1, min, max);
    Lua_Pop();
    return value;
}

void LuaValues_ConstantTable(const LuaConstant *constants) {
    Lua_MakeTable(0);
    for (int i = 0; constants[i].name; i++) {
        Lua_PushInt(constants[i].value);
        Lua_SetField(-2, constants[i].name);
    }
}
