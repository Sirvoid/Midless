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
#include "networkhandler.h"
#include "packet.h"
#include "world.h"
#include "utils.h"
#include "stb_ds.h"

typedef struct LMethod {
  const char *name;
  void* func;
} LMethod;

//---System---

static int LD_Sleep(void) {
    int timeWaiting = Lua_GetNumber(1);
    long long beginning = GetTimeMilliseconds();
    
    while(GetTimeMilliseconds() < beginning + timeWaiting) {
        //Wait
    }

    return 0;
}

//---------World---------

int *LD_OnBlockUpdates = NULL;
static int LD_OnBlockUpdateAdd(void) {
    arrput(LD_OnBlockUpdates, Lua_Ref(Lua_GetRegistryIndex()));
    return 0;
}

void LD_OnBlockUpdateCall(Vector3 position, unsigned short blockID, unsigned short prevBlockID) {
    if(Lua_running == 0) return;
    for(int i = 0; i < arrlen(LD_OnBlockUpdates); i++) {
        Lua_GetRawI(Lua_GetRegistryIndex(), LD_OnBlockUpdates[i]);
            Lua_PushInt(position.x);
            Lua_PushInt(position.y);
            Lua_PushInt(position.z);
            Lua_PushInt(blockID);
            Lua_PushInt(prevBlockID);
        Lua_CallFunc(5, 0);
    }
}

static int LD_SetBlock(void) {
    int x = Lua_GetInt(1);
    int y = Lua_GetInt(2);
    int z = Lua_GetInt(3);
    int blockID = Lua_GetInt(4);
    World_SetBlock((Vector3) {x, y, z}, blockID, true);
    return 0;
}

static int LD_GetBlock(void) {
    int x = Lua_GetInt(1);
    int y = Lua_GetInt(2);
    int z = Lua_GetInt(3);

    int blockID = World_GetBlock((Vector3){x, y, z});
    Lua_PushInt(blockID);
    return 1;
}

static const struct LMethod worldLib[] = {
    {"set_block", LD_SetBlock},
    {"on_block_update", LD_OnBlockUpdateAdd},
    {"get_block", LD_GetBlock},
    {NULL, NULL}
};

//---------Chat---------

int *LD_OnChatMessages = NULL;
static int LD_OnChatMessageAdd() {
    arrput(LD_OnChatMessages, Lua_Ref(Lua_GetRegistryIndex()));
    return 0;
}

void LD_OnChatMessageCall(const char *name, const char *message) {
    if(Lua_running == 0) return;
    for(int i = 0; i < arrlen(LD_OnChatMessages); i++) {
        Lua_GetRawI(Lua_GetRegistryIndex(), LD_OnChatMessages[i]);
            Lua_PushString(name);
            Lua_PushString(message);
        Lua_CallFunc(2, 0);
    }
}

int LD_BroadcastMessage(void) {
    const char *message = Lua_GetString(1);
    World_SendMessage(message);
    return 0;
}

static const struct LMethod chatLib[] = {
    {"broadcast", LD_BroadcastMessage},
    {"on_player_message", LD_OnChatMessageAdd},
    {NULL, NULL}
};

//-------

void LuaDefinition_Init(void) {
    Lua_DefineLib("chat", chatLib);
    Lua_DefineLib("world", worldLib);

    Lua_DefineGlobalFunc("sleep", LD_Sleep);
}

