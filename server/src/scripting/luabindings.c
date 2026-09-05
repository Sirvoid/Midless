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

static int *luaReadyCallbacks = NULL;
static bool luaReadyInvoked;

static int LuaBindings_RegisterReady(void) {
    if (luaReadyInvoked) return Lua_Error("world.on_ready must be registered during script startup");
    int callback = Lua_RefFunction(1);
    arrput(luaReadyCallbacks, callback);
    return 0;
}

void LuaBindings_InvokeReady(void) {
    if (!luaRunning || luaReadyInvoked) return;
    luaReadyInvoked = true;
    for (int i = 0; i < arrlen(luaReadyCallbacks); i++) {
        Lua_GetRawI(Lua_GetRegistryIndex(), luaReadyCallbacks[i]);
        Lua_CallFunc(0, 0);
    }
}

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

static int LuaBindings_IntField(int table, const char *name, int fallback, int min, int max) {
    int value = fallback;
    if (Lua_PushField(table, name)) value = Lua_GetIntRange(-1, min, max);
    Lua_Pop();
    return value;
}

static void LuaBindings_ReadBounds(int table, const char *name, uint8_t values[3]) {
    if (Lua_PushField(table, name)) {
        Lua_CheckTable(-1);
        int vector = Lua_GetTop();
        for (int i = 0; i < 3; i++) {
            Lua_GetRawI(vector, i + 1);
            values[i] = Lua_GetIntRange(-1, 0, 16);
            Lua_Pop();
        }
    }
    Lua_Pop();
}

static void LuaBindings_ReadBlockTable(BlockDefinition *d) {
    Lua_PushField(2, "name");
    Lua_CopyString(-1, d->name, sizeof(d->name));
    Lua_Pop();
    d->modelType = LuaBindings_IntField(2, "model", BLOCK_MODEL_SOLID, BLOCK_MODEL_GAS, BLOCK_MODEL_SPRITE);
    d->renderType = LuaBindings_IntField(2, "render", BLOCK_RENDER_OPAQUE, BLOCK_RENDER_OPAQUE, BLOCK_RENDER_TRANSLUCENT);
    d->colliderType = LuaBindings_IntField(2, "collider", BLOCK_COLLIDER_SOLID, BLOCK_COLLIDER_NONE, BLOCK_COLLIDER_LIQUID);
    d->lightType = LuaBindings_IntField(2, "light", BLOCK_LIGHT_NONE, BLOCK_LIGHT_NONE, BLOCK_LIGHT_EMIT);
    for (int i = 0; i < 3; i++) d->max[i] = 16;
    if (Lua_PushField(2, "bounds")) {
        Lua_CheckTable(-1);
        int bounds = Lua_GetTop();
        LuaBindings_ReadBounds(bounds, "min", d->min);
        LuaBindings_ReadBounds(bounds, "max", d->max);
    }
    Lua_Pop();

    // Require textures, with each face resolved from all -> sides -> face.
    Lua_PushField(2, "textures");
    Lua_CheckTable(-1);
    int textures = Lua_GetTop();
    int all = LuaBindings_IntField(textures, "all", -1, 0, 255);
    int sides = LuaBindings_IntField(textures, "sides", all, 0, 255);
    const char *faces[] = {"left", "right", "top", "bottom", "front", "back"};
    for (int i = 0; i < 6; i++) {
        int fallback = (i == 2 || i == 3) ? all : sides;
        int texture = LuaBindings_IntField(textures, faces[i], fallback, 0, 255);
        if (texture < 0) Lua_Error("textures must specify all faces, using all, sides, or individual face names");
        d->textures[i] = texture;
    }
    Lua_Pop();
}

static int LuaBindings_DefineBlock(void) {
    int blockId = Lua_GetIntRange(1, 1, 255);
    BlockDefinition definition = {0};
    Lua_CheckTable(2);
    LuaBindings_ReadBlockTable(&definition);
    if (!BlockDefinition_Validate(blockId, &definition)) {
        return Lua_Error("invalid block definition");
    }
    if (!serverWorld.players) {
        return Lua_Error("world.define_block must run after world initialization.");
    }
    ServerWorld_DefineBlock(blockId, &definition);
    return 0;
}

static const struct LuaMethod worldLib[] = {
    {"on_ready", LuaBindings_RegisterReady},
    {"set_block", LuaBindings_SetBlock},
    {"on_block_update", LuaBindings_RegisterBlockUpdate},
    {"get_block", LuaBindings_GetBlock},
    {"define_block", LuaBindings_DefineBlock},
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

typedef struct LuaConstant {
    const char *name;
    int value;
} LuaConstant;

static void LuaBindings_ConstantTable(const LuaConstant *constants) {
    Lua_MakeTable(0);
    for (int i = 0; constants[i].name; i++) {
        Lua_PushInt(constants[i].value);
        Lua_SetField(-2, constants[i].name);
    }
}

static void LuaBindings_DefineBlockConstants(void) {
    static const LuaConstant models[] = {{"GAS", BLOCK_MODEL_GAS}, {"SOLID", BLOCK_MODEL_SOLID}, {"SPRITE", BLOCK_MODEL_SPRITE}, {NULL, 0}};
    static const LuaConstant renders[] = {{"OPAQUE", BLOCK_RENDER_OPAQUE}, {"TRANSPARENT", BLOCK_RENDER_TRANSPARENT}, {"TRANSLUCENT", BLOCK_RENDER_TRANSLUCENT}, {NULL, 0}};
    static const LuaConstant colliders[] = {{"NONE", BLOCK_COLLIDER_NONE}, {"SOLID", BLOCK_COLLIDER_SOLID}, {"LIQUID", BLOCK_COLLIDER_LIQUID}, {NULL, 0}};
    static const LuaConstant lights[] = {{"NONE", BLOCK_LIGHT_NONE}, {"EMIT", BLOCK_LIGHT_EMIT}, {NULL, 0}};
    Lua_MakeTable(4);
    LuaBindings_ConstantTable(models); Lua_SetField(-2, "model");
    LuaBindings_ConstantTable(renders); Lua_SetField(-2, "render");
    LuaBindings_ConstantTable(colliders); Lua_SetField(-2, "collider");
    LuaBindings_ConstantTable(lights); Lua_SetField(-2, "light");
    Lua_SetGlobal("block");
}

void LuaBindings_Init(void) {
    luaReadyInvoked = false;
    LuaBindings_DefineBlockConstants();
    Lua_DefineLib("chat", chatLib);
    Lua_DefineLib("world", worldLib);

    Lua_DefineGlobalFunc("sleep", LuaBindings_Sleep);
}

void LuaBindings_Shutdown(void) {
    arrfree(luaReadyCallbacks);
    luaReadyCallbacks = NULL;
    luaReadyInvoked = false;
    arrfree(luaBlockUpdateCallbacks);
    luaBlockUpdateCallbacks = NULL;
    arrfree(luaChatMessageCallbacks);
    luaChatMessageCallbacks = NULL;
}

