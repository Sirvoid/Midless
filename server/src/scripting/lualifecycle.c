/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "lualifecycle.h"
#include "luaengine.h"
#include "../utils.h"
#include "stb_ds.h"

static int *luaReadyCallbacks;
static int *luaStepCallbacks;
static bool luaReadyInvoked;

int LuaLifecycle_RegisterReady(void) {
    if (luaReadyInvoked)
        return Lua_Error("midless.register_on_ready must be registered during script startup");
    int callback = Lua_RefFunction(1);
    arrput(luaReadyCallbacks, callback);
    return 0;
}

void ScriptHooks_Ready(void) {
    if (!luaRunning || luaReadyInvoked)
        return;
    luaReadyInvoked = true;
    for (int i = 0; i < arrlen(luaReadyCallbacks); i++) {
        Lua_GetRawI(Lua_GetRegistryIndex(), luaReadyCallbacks[i]);
        Lua_CallFunc(0, 0);
    }
}

int LuaLifecycle_RegisterStep(void) {
    int callback = Lua_RefFunction(1);
    arrput(luaStepCallbacks, callback);
    return 0;
}

void ScriptHooks_Step(float delta) {
    if (!luaRunning)
        return;

    for (int i = 0; i < arrlen(luaStepCallbacks); i++) {
        Lua_GetRawI(Lua_GetRegistryIndex(), luaStepCallbacks[i]);
        Lua_PushNumber(delta);
        Lua_CallFunc(1, 0);
    }
}

int LuaLifecycle_Sleep(void) {
    int timeWaiting = Lua_GetNumber(1);
    long long beginning = GetTimeMilliseconds();

    while (GetTimeMilliseconds() < beginning + timeWaiting) {
        // Wait
    }

    return 0;
}

void LuaLifecycle_Init(void) {
    luaReadyInvoked = false;
}

void LuaLifecycle_Shutdown(void) {
    for (int i = 0; i < arrlen(luaReadyCallbacks); i++) {
        Lua_Unref(Lua_GetRegistryIndex(), luaReadyCallbacks[i]);
    }
    arrfree(luaReadyCallbacks);
    luaReadyCallbacks = NULL;
    for (int i = 0; i < arrlen(luaStepCallbacks); i++) {
        Lua_Unref(Lua_GetRegistryIndex(), luaStepCallbacks[i]);
    }
    arrfree(luaStepCallbacks);
    luaStepCallbacks = NULL;
    luaReadyInvoked = false;
}
