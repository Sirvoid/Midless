/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"
#include "luaengine.h"
#include "../networkhandler.h"
#include "../packet.h"
#include "../world/world.h"
#include "../utils.h"
#include "stb_ds.h"

typedef struct LuaMethod {
  const char *name;
  void* func;
} LuaMethod;

//---System---

static int LuaBindings_Sleep(void) {
    int timeWaiting = Lua_GetNumber(1);
    long long beginning = GetTimeMilliseconds();
    
    while(GetTimeMilliseconds() < beginning + timeWaiting) {
        //Wait
    }

    return 0;
}

//---------World---------

int *luaBlockUpdateCallbacks = NULL;
static int LuaBindings_RegisterBlockUpdate(void) {
    arrput(luaBlockUpdateCallbacks, Lua_Ref(Lua_GetRegistryIndex()));
    return 0;
}

void LuaBindings_InvokeBlockUpdate(Vector3 position, unsigned short blockId, unsigned short previousBlockId) {
    if(luaRunning == 0) return;
    for(int i = 0; i < arrlen(luaBlockUpdateCallbacks); i++) {
        Lua_GetRawI(Lua_GetRegistryIndex(), luaBlockUpdateCallbacks[i]);
            Lua_PushInt(position.x);
            Lua_PushInt(position.y);
            Lua_PushInt(position.z);
            Lua_PushInt(blockId);
            Lua_PushInt(previousBlockId);
        Lua_CallFunc(5, 0);
    }
}

static int LuaBindings_SetBlock(void) {
    int x = Lua_GetInt(1);
    int y = Lua_GetInt(2);
    int z = Lua_GetInt(3);
    int blockId = Lua_GetInt(4);
    ServerWorld_SetBlock((Vector3) {x, y, z}, blockId, true);
    return 0;
}

static int LuaBindings_GetBlock(void) {
    int x = Lua_GetInt(1);
    int y = Lua_GetInt(2);
    int z = Lua_GetInt(3);

    int blockId = ServerWorld_GetBlock((Vector3){x, y, z});
    Lua_PushInt(blockId);
    return 1;
}

static const struct LuaMethod worldLib[] = {
    {"set_block", LuaBindings_SetBlock},
    {"on_block_update", LuaBindings_RegisterBlockUpdate},
    {"get_block", LuaBindings_GetBlock},
    {NULL, NULL}
};

//---------Chat---------

int *luaChatMessageCallbacks = NULL;
static int LuaBindings_RegisterChatMessage() {
    arrput(luaChatMessageCallbacks, Lua_Ref(Lua_GetRegistryIndex()));
    return 0;
}

void LuaBindings_InvokeChatMessage(const char *name, const char *message) {
    if(luaRunning == 0) return;
    for(int i = 0; i < arrlen(luaChatMessageCallbacks); i++) {
        Lua_GetRawI(Lua_GetRegistryIndex(), luaChatMessageCallbacks[i]);
            Lua_PushString(name);
            Lua_PushString(message);
        Lua_CallFunc(2, 0);
    }
}

int LuaBindings_BroadcastMessage(void) {
    const char *message = Lua_GetString(1);
    ServerWorld_SendMessage(message);
    return 0;
}

static const struct LuaMethod chatLib[] = {
    {"broadcast", LuaBindings_BroadcastMessage},
    {"on_player_message", LuaBindings_RegisterChatMessage},
    {NULL, NULL}
};

//-------

void LuaBindings_Init(void) {
    Lua_DefineLib("chat", chatLib);
    Lua_DefineLib("world", worldLib);

    Lua_DefineGlobalFunc("sleep", LuaBindings_Sleep);
}

void LuaBindings_Shutdown(void) {
    arrfree(luaBlockUpdateCallbacks);
    luaBlockUpdateCallbacks = NULL;
    arrfree(luaChatMessageCallbacks);
    luaChatMessageCallbacks = NULL;
}

