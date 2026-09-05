/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "raylib.h"
#include "luaengine.h"
#include "luaentities.h"
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
    if (luaReadyInvoked) return Lua_Error("midless.register_on_ready must be registered during script startup");
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

static int *luaStepCallbacks = NULL;

static int LuaBindings_RegisterStep(void) {
    int callback = Lua_RefFunction(1);
    arrput(luaStepCallbacks, callback);
    return 0;
}

void LuaBindings_InvokeStep(float delta) {
    if (!luaRunning) return;

    for (int i = 0; i < arrlen(luaStepCallbacks); i++) {
        Lua_GetRawI(Lua_GetRegistryIndex(), luaStepCallbacks[i]);
        Lua_PushNumber(delta);
        Lua_CallFunc(1, 0);
    }
}

int *luaBlockUpdateCallbacks = NULL;
static int LuaBindings_RegisterBlockUpdate(void) {
    int callback = Lua_RefFunction(1);
    arrput(luaBlockUpdateCallbacks, callback);
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
        return Lua_Error("midless.define_block must run after world initialization.");
    }
    ServerWorld_DefineBlock(blockId, &definition);
    return 0;
}

//---------Entity models---------

static void LuaBindings_ModelVector(int table, const char *name, int16_t values[3], bool optional) {
    if (!Lua_PushField(table, name) && optional) { Lua_Pop(); return; }
    int vector = Lua_GetTop();
    if (Lua_TableLength(vector) != 3) Lua_Error("model vectors require exactly three coordinates");
    for (int i = 0; i < 3; i++) {
        Lua_GetRawI(vector, i + 1);
        float value = Lua_GetNumber(-1);
        if (!isfinite(value) || value < -512.0f || value > 511.984375f)
            Lua_Error("model coordinates must be finite and between -512 and 511.984375");
        values[i] = (int16_t)roundf(value * 64.0f);
        Lua_Pop();
    }
    Lua_Pop();
}

static int LuaBindings_DefineEntityModel(void) {
    int id = Lua_GetIntRange(1, 1, 255);
    Lua_CheckTable(2);
    ModelDefinition d = {0};
    Lua_PushField(2, "name"); Lua_CopyString(-1, d.name, sizeof(d.name)); Lua_Pop();
    Lua_PushField(2, "texture");
    const char *texture = Lua_GetString(-1);
    if (!strcmp(texture, "humanoid")) d.texture = 0;
    else if (!strcmp(texture, "terrain")) d.texture = 1;
    else return Lua_Error("model texture must be 'humanoid' or 'terrain'");
    Lua_Pop();
    Lua_PushField(2, "parts");
    int parts = Lua_GetTop();
    int count = Lua_TableLength(parts);
    if (count < 1 || count > ENTITY_MODEL_MAX_PARTS) return Lua_Error("models require 1 to 64 parts");
    d.partCount = count;
    static const char *faces[] = {"east", "west", "up", "down", "north", "south"};
    for (int i = 0; i < count; i++) {
        Lua_GetRawI(parts, i + 1);
        int part = Lua_GetTop();
        Lua_CheckTable(part);
        ModelPartDefinition *p = &d.parts[i];
        p->role = LuaBindings_IntField(part, "role", 0, 0, 5);
        if (Lua_PushField(part, "first_person_visible")) p->firstPersonVisible = Lua_GetBoolean(-1);
        Lua_Pop();
        LuaBindings_ModelVector(part, "position", p->position, true);
        LuaBindings_ModelVector(part, "min", p->min, false);
        LuaBindings_ModelVector(part, "max", p->max, false);
        Lua_PushField(part, "uv");
        int uv = Lua_GetTop(); Lua_CheckTable(uv);
        for (int f = 0; f < 6; f++) {
            Lua_PushField(uv, faces[f]);
            int rectangle = Lua_GetTop();
            if (Lua_TableLength(rectangle) != 4) return Lua_Error("UV rectangles require x, y, width, height");
            for (int a = 0; a < 4; a++) {
                Lua_GetRawI(rectangle, a + 1);
                p->uv[f][a] = Lua_GetIntRange(-1, -32768, 32767);
                Lua_Pop();
            }
            Lua_Pop();
        }
        Lua_Pop(); Lua_Pop();
    }
    Lua_Pop();
    if (!ServerWorld_DefineEntityModel(id, &d)) return Lua_Error("invalid entity model or allocation failed");
    return 0;
}
static int LuaBindings_RemoveEntityModel(void) {
    ServerWorld_RemoveEntityModel(Lua_GetIntRange(1, 1, 255));
    return 0;
}
static int LuaBindings_SetEntityModel(void) {
    int entityId = Lua_GetIntRange(1, 0, WORLD_MAX_ENTITIES - 1);
    int modelId = Lua_GetIntRange(2, 0, 255);
    if (!ServerWorld_SetEntityModel(entityId, modelId)) return Lua_Error("entity or model is not defined");
    return 0;
}

//---------Players---------

#define LUA_PLAYER_TYPE "midless.Player"
static int *luaJoinCallbacks, *luaLeaveCallbacks;
static Player *luaLeavingPlayer;
typedef struct LuaPlayerHandle {
    int id;
    uint64_t connectionId;
} LuaPlayerHandle;

static void LuaBindings_PushPlayer(Player *player) {
    if (!player || (player->disconnected && player != luaLeavingPlayer)) {
        Lua_PushString(NULL);
        return;
    }
    LuaPlayerHandle *handle = Lua_NewObject(LUA_PLAYER_TYPE, sizeof(*handle));
    handle->id = player->id;
    handle->connectionId = player->connectionId;
}

static Player *LuaBindings_CheckPlayer(void) {
    LuaPlayerHandle *handle = Lua_CheckObject(1, LUA_PLAYER_TYPE);
    Player *player = serverWorld.players ? serverWorld.players[handle->id] : NULL;
    if (!player || (player->disconnected && player != luaLeavingPlayer) || player->connectionId != handle->connectionId) {
        Lua_Error("player is no longer connected");
        return NULL;
    }
    return player;
}

static int LuaBindings_RegisterPlayerJoin(void) {
    int callback = Lua_RefFunction(1);
    arrput(luaJoinCallbacks, callback);
    return 0;
}

static int LuaBindings_RegisterPlayerLeave(void) {
    int callback = Lua_RefFunction(1);
    arrput(luaLeaveCallbacks, callback);
    return 0;
}

static void LuaBindings_InvokePlayerEvent(int playerId, bool leaving) {
    if (!luaRunning || !serverWorld.players || playerId < 0 || playerId >= WORLD_MAX_PLAYERS) return;
    Player *player = serverWorld.players[playerId];
    if (!player || (!leaving && player->disconnected)) return;
    Player *previousLeavingPlayer = luaLeavingPlayer;
    if (leaving) luaLeavingPlayer = player;
    int count = leaving ? arrlen(luaLeaveCallbacks) : arrlen(luaJoinCallbacks);
    for (int i = 0; i < count; i++) {
        int callback = leaving ? luaLeaveCallbacks[i] : luaJoinCallbacks[i];
        Lua_GetRawI(Lua_GetRegistryIndex(), callback);
        LuaBindings_PushPlayer(player);
        Lua_CallFunc(1, 0);
    }
    luaLeavingPlayer = previousLeavingPlayer;
}

void LuaBindings_InvokePlayerJoin(int playerId) {
    LuaBindings_InvokePlayerEvent(playerId, false);
}

void LuaBindings_InvokePlayerLeave(int playerId) {
    LuaBindings_InvokePlayerEvent(playerId, true);
}

static int LuaBindings_GetPlayerById(void) {
    int id = Lua_GetIntRange(1, 0, WORLD_MAX_PLAYERS - 1);
    LuaBindings_PushPlayer(serverWorld.players ? serverWorld.players[id] : NULL);
    return 1;
}

static int LuaBindings_GetPlayerByName(void) {
    const char *name = Lua_GetString(1);
    if (serverWorld.players) {
        for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
            Player *player = serverWorld.players[i];
            if (player && !player->disconnected && player->name && !strcmp(player->name, name)) {
                LuaBindings_PushPlayer(player);
                return 1;
            }
        }
    }
    Lua_PushString(NULL);
    return 1;
}

static int LuaBindings_GetPlayerId(void) {
    Lua_PushInt(LuaBindings_CheckPlayer()->id);
    return 1;
}

static int LuaBindings_ListPlayers(void) {
    Lua_MakeTable(0);
    if (!serverWorld.players) return 1;

    int index = 1;
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *player = serverWorld.players[i];
        if (!player || player->disconnected) continue;
        LuaBindings_PushPlayer(player);
        Lua_SetRawI(-2, index++);
    }
    return 1;
}

static int LuaBindings_GetPlayerName(void) {
    Lua_PushString(LuaBindings_CheckPlayer()->name);
    return 1;
}

static int LuaBindings_GetPlayerPosition(void) {
    int id = LuaBindings_CheckPlayer()->entityId;
    if (!serverWorld.entities || id < 0 || !serverWorld.entities[id].active) return Lua_Error("player has no entity");
    Vector3 position = serverWorld.entities[id].position;
    Lua_PushNumber(position.x);
    Lua_PushNumber(position.y);
    Lua_PushNumber(position.z);
    return 3;
}

static int LuaBindings_TeleportPlayer(void) {
    Player *player = LuaBindings_CheckPlayer();
    if (player->disconnected || player == luaLeavingPlayer) return Lua_Error("player is leaving");
    int id = player->entityId;
    float x = Lua_GetNumber(2);
    float y = Lua_GetNumber(3);
    float z = Lua_GetNumber(4);
    // Positions are sent as signed 32-bit integers in 1/64-block units.
    if (!isfinite(x) || !isfinite(y) || !isfinite(z) ||
        fabsf(x) > 33554430.0f || fabsf(y) > 33554430.0f || fabsf(z) > 33554430.0f) {
        return Lua_Error("teleport coordinates must be finite and within the network coordinate range");
    }
    if (!serverWorld.entities || id < 0 || !serverWorld.entities[id].active) {
        return Lua_Error("player is not connected");
    }
    ServerPlayer_Teleport(player, (Vector3){x, y, z});
    return 0;
}

static int LuaBindings_SendPlayerMessage(void) {
    Player *player = LuaBindings_CheckPlayer();
    const char *message = Lua_GetString(2);
    if (player->disconnected || player == luaLeavingPlayer) return Lua_Error("player is leaving");
    ServerPlayer_SendMessage(player, message);
    return 0;
}

static int LuaBindings_SetPlayerModel(void) {
    Player *player = LuaBindings_CheckPlayer();
    if (player->disconnected || player == luaLeavingPlayer) return Lua_Error("player is leaving");
    int modelId = Lua_GetIntRange(2, 0, 255);
    if (!ServerWorld_SetEntityModel(player->entityId, modelId)) return Lua_Error("model is not defined");
    return 0;
}

static const struct LuaMethod playerLib[] = {
    {"get_id", LuaBindings_GetPlayerId},
    {"set_model", LuaBindings_SetPlayerModel},
    {"get_name", LuaBindings_GetPlayerName},
    {"get_position", LuaBindings_GetPlayerPosition},
    {"teleport", LuaBindings_TeleportPlayer},
    {"send_message", LuaBindings_SendPlayerMessage},
    {NULL, NULL}
};

//---------Chat---------

int *luaChatMessageCallbacks = NULL;
static int LuaBindings_RegisterChatMessage() {
    int callback = Lua_RefFunction(1);
    arrput(luaChatMessageCallbacks, callback);
    return 0;
}

void LuaBindings_InvokeChatMessage(int playerId, const char *message) {
    if(luaRunning == 0) return;
    if (!serverWorld.players || playerId < 0 || playerId >= WORLD_MAX_PLAYERS) return;
    Player *player = serverWorld.players[playerId];
    if (!player || player->disconnected) return;
    for(int i = 0; i < arrlen(luaChatMessageCallbacks); i++) {
        Lua_GetRawI(Lua_GetRegistryIndex(), luaChatMessageCallbacks[i]);
            LuaBindings_PushPlayer(player);
            Lua_PushString(message);
        Lua_CallFunc(2, 0);
    }
}

int LuaBindings_BroadcastMessage(void) {
    const char *message = Lua_GetString(1);
    ServerWorld_SendMessage(message);
    return 0;
}

static const struct LuaMethod midlessLib[] = {
    {"define_entity", LuaEntities_Register},
    {"spawn_entity", LuaEntities_Spawn},
    {"get_player_by_id", LuaBindings_GetPlayerById},
    {"get_player_by_name", LuaBindings_GetPlayerByName},
    {"get_players", LuaBindings_ListPlayers},
    {"get_block", LuaBindings_GetBlock},
    {"set_block", LuaBindings_SetBlock},
    {"define_block", LuaBindings_DefineBlock},
    {"define_entity_model", LuaBindings_DefineEntityModel},
    {"remove_entity_model", LuaBindings_RemoveEntityModel},
    {"set_entity_model", LuaBindings_SetEntityModel},
    {"register_on_ready", LuaBindings_RegisterReady},
    {"register_on_step", LuaBindings_RegisterStep},
    {"register_on_player_message", LuaBindings_RegisterChatMessage},
    {"register_on_player_join", LuaBindings_RegisterPlayerJoin},
    {"register_on_player_leave", LuaBindings_RegisterPlayerLeave},
    {"register_on_block_update", LuaBindings_RegisterBlockUpdate},
    {"broadcast", LuaBindings_BroadcastMessage},
    {"sleep", LuaBindings_Sleep},
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

static void LuaBindings_DefineModelConstants(void) {
    static const LuaConstant roles[] = {
        {"NONE", 0}, {"HEAD", 1}, {"RIGHT_ARM", 2}, {"LEFT_ARM", 3},
        {"RIGHT_LEG", 4}, {"LEFT_LEG", 5}, {NULL, 0}
    };
    Lua_MakeTable(1);
    LuaBindings_ConstantTable(roles);
    Lua_SetField(-2, "part");
    Lua_SetGlobal("model");
}

void LuaBindings_Init(void) {
    LuaEntities_Init();
    luaReadyInvoked = false;
    LuaBindings_DefineBlockConstants();
    LuaBindings_DefineModelConstants();
    Lua_DefineObjectType(LUA_PLAYER_TYPE, playerLib);
    Lua_DefineLib("midless", midlessLib);
}

void LuaBindings_Shutdown(void) {
    LuaEntities_Shutdown();
    arrfree(luaJoinCallbacks);
    luaJoinCallbacks = NULL;
    arrfree(luaLeaveCallbacks);
    luaLeaveCallbacks = NULL;
    luaLeavingPlayer = NULL;
    arrfree(luaReadyCallbacks);
    luaReadyCallbacks = NULL;
    luaReadyInvoked = false;
    arrfree(luaBlockUpdateCallbacks);
    luaBlockUpdateCallbacks = NULL;
    arrfree(luaChatMessageCallbacks);
    luaChatMessageCallbacks = NULL;
    arrfree(luaStepCallbacks);
    luaStepCallbacks = NULL;
}

