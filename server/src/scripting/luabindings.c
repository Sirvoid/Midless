#include "blockstates.h"
#include "luaitemactions.h"
#include "../serverinventory.h"
#include "../hudbars.h"
#include "luadigging.h"
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
#include "luametadata.h"
#include "luainventory.h"
#include "../crafting.h"
#include "../items.h"
#include "luamodels.h"
#include "luavector.h"
#include "../networkhandler.h"
#include "../packet.h"
#include "../world/world.h"
#include "../world/worldgen.h"
#include "../world/textures.h"
#include "../utils.h"
#include "stb_ds.h"

typedef struct LuaMethod {
  const char *name;
  void* func;
} LuaMethod;
extern lua_State *L;
static int blockInteractions[256];

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

static void LuaBindings_PushPosition(Vector3 position) {
    Lua_MakeTable(3);
    Lua_PushNumber(position.x); Lua_SetField(-2, "x");
    Lua_PushNumber(position.y); Lua_SetField(-2, "y");
    Lua_PushNumber(position.z); Lua_SetField(-2, "z");
}

static Vector3 LuaBindings_ReadPosition(int arg, bool blockPosition) {
    Lua_CheckTable(arg);
    const char *fields[] = {"x", "y", "z"};
    float values[3];
    for (int i = 0; i < 3; i++) {
        Lua_PushField(arg, fields[i]);
        values[i] = blockPosition
            ? Lua_GetIntRange(-1, -33554430, 33554430)
            : Lua_GetNumber(-1);
        Lua_Pop();
        if (!isfinite(values[i]) || fabsf(values[i]) > 33554430.0f)
            Lua_Error("position coordinates must be finite and within the network coordinate range");
    }
    return (Vector3){values[0], values[1], values[2]};
}

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
            LuaBindings_PushPosition(position);
            ServerItems_PushId(L, blockId);
            ServerItems_PushId(L, previousBlockId);
        Lua_CallFunc(3, 0);
    }
}

static int LuaBindings_SetBlock(void) {
    Vector3 position = LuaBindings_ReadPosition(1, true);
    int blockId = ServerItems_Id(L, 2, true, false);
    if (!ServerWorld_IsBlockDefined(blockId)) return luaL_error(L, "block is not defined");
    ServerWorld_SetBlock(position, blockId, true, false, true);
    return 0;
}

#define LUA_BLOCK_BATCH_SIZE 4096
#define LUA_MAX_BLOCK_UPDATES 1000000

static ServerBlockUpdate LuaBindings_ReadBlockUpdate(int table) {
    Lua_CheckTable(table);

    Lua_PushField(table, "pos");
    Vector3 position = LuaBindings_ReadPosition(-1, true);
    Lua_Pop();

    Lua_PushField(table, "blockId");
    int blockId = ServerItems_Id(L, -1, true, false);
    Lua_Pop();
    if (!ServerWorld_IsBlockDefined(blockId)) Lua_Error("blockId is not defined");

    return (ServerBlockUpdate){
        .position = position,
        .blockId = (unsigned char)blockId
    };
}

static int LuaBindings_SetBlocks(void) {
    Lua_CheckTable(1);
    bool callCallbacks = Lua_GetTop() < 2 || Lua_GetBoolean(2);
    int count = Lua_TableLength(1);
    if (count < 0 || count > LUA_MAX_BLOCK_UPDATES)
        return Lua_Error("set_blocks accepts at most 1000000 updates");

    // Validate the complete input before changing any blocks.
    for (int i = 0; i < count; i++) {
        Lua_GetRawI(1, i + 1);
        LuaBindings_ReadBlockUpdate(-1);
        Lua_Pop();
    }

    ServerBlockUpdate *updates = count > 0 ? MemAlloc(sizeof(*updates) * count) : NULL;
    if (count > 0 && updates == NULL) return Lua_Error("could not allocate block update batch");

    int changedCount = 0;
    for (int i = 0; i < count; i++) {
        Lua_GetRawI(1, i + 1);
        ServerBlockUpdate update = LuaBindings_ReadBlockUpdate(-1);
        Lua_Pop();

        int previousBlockId = ServerWorld_GetBlock(update.position);
        if (previousBlockId == update.blockId) continue;
        ServerWorld_SetBlock(update.position, update.blockId, false, false, callCallbacks);
        if (ServerWorld_GetBlock(update.position) == update.blockId)
            updates[changedCount++] = update;
    }

    for (int offset = 0; offset < changedCount; offset += LUA_BLOCK_BATCH_SIZE) {
        int remaining = changedCount - offset;
        unsigned short batchCount = (unsigned short)(remaining < LUA_BLOCK_BATCH_SIZE
            ? remaining : LUA_BLOCK_BATCH_SIZE);
        for (int playerId = 0; playerId < WORLD_MAX_PLAYERS; playerId++) {
            Player *player = serverWorld.players[playerId];
            if (player == NULL) continue;
            ServerNetwork_Send(player, ServerPacket_CreateBlockBatch(updates + offset, batchCount));
        }
    }

    MemFree(updates);
    return 0;
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
    int blockId = ServerItems_Declare(L, true);
    if (!blockId || serverWorld.hasBlockDefinition[blockId]) return luaL_error(L, "block already defined");
    BlockDefinition definition = {0};
    Lua_CheckTable(2);
    LuaBindings_ReadBlockTable(&definition);
    if (!BlockDefinition_Validate(blockId, &definition)) {
        return Lua_Error("invalid block definition");
    }
    if (!serverWorld.players) {
        return Lua_Error("midless.define_block must run after world initialization.");
    }
    lua_getfield(L, 2, "on_interact");
    if (!lua_isnil(L, -1)) luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_pop(L, 1);
    LuaDigging_Define(blockId, 2, true);
    LuaItemActions_Define(blockId, 2, true);
    LuaMetadata_DefineBlock(blockId, 2);
    ServerBlockStates_Define(L, blockId, 2, &definition);
    lua_getfield(L, 2, "item_metadata");
    if (!lua_isnil(L, -1)) {
        lua_newtable(L); lua_pushvalue(L, -2); lua_setfield(L, -2, "metadata");
        LuaMetadata_DefineItem(blockId, lua_gettop(L)); lua_pop(L, 1);
    }
    lua_pop(L, 1);
    luaL_unref(L, LUA_REGISTRYINDEX, blockInteractions[blockId]);
    lua_getfield(L, 2, "on_interact");
    blockInteractions[blockId] = luaL_ref(L, LUA_REGISTRYINDEX);
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
    int id = LuaModels_Resolve(1, true);
    Lua_CheckTable(2);
    ModelDefinition d = {0};
    Lua_PushField(2, "name"); Lua_CopyString(-1, d.name, sizeof(d.name)); Lua_Pop();
    Lua_PushField(2, "texture");
    const char *texture = Lua_GetString(-1);
    int textureId=ServerTextures_Find(texture);
    if(textureId<0) return Lua_Error("texture is not defined");
    d.texture=textureId;
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
        if (Lua_PushField(part, "grip")) p->hasGrip = true;
        Lua_Pop();
        if (p->hasGrip) LuaBindings_ModelVector(part, "grip", p->grip, false);
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
    LuaModels_BindName(1, id);
    return 0;
}
static int LuaBindings_RemoveEntityModel(void) {
    int id = LuaModels_Resolve(1, false);
    if (id == 0) return Lua_Error("cannot remove the built-in humanoid model");
    ServerWorld_RemoveEntityModel(id);
    return 0;
}
static int LuaBindings_SetEntityModel(void) {
    int entityId = Lua_GetIntRange(1, 0, WORLD_MAX_ENTITIES - 1);
    int modelId = LuaModels_Resolve(2, false);
    if (!ServerWorld_SetEntityModel(entityId, modelId)) return Lua_Error("entity or model is not defined");
    return 0;
}

//---------Players---------

#define LUA_PLAYER_TYPE "midless.Player"
static int *luaJoinCallbacks, *luaLeaveCallbacks, *luaLandCallbacks;
static Player *luaLeavingPlayer;
typedef struct LuaPlayerHandle {
    int id;
    uint64_t connectionId;
} LuaPlayerHandle;

void LuaBindings_PushPlayer(Player *player) {
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

static int LuaBindings_RegisterPlayerLand(void) {
    int callback = Lua_RefFunction(1);
    arrput(luaLandCallbacks, callback);
    return 0;
}

void LuaBindings_InvokePlayerLand(int playerId, float distance) {
    if (!luaRunning || !serverWorld.players || playerId < 0 || playerId >= WORLD_MAX_PLAYERS) return;
    Player *player = serverWorld.players[playerId];
    if (!player || player->disconnected) return;
    int count = arrlen(luaLandCallbacks);
    for (int i = 0; i < count; i++) {
        Lua_GetRawI(Lua_GetRegistryIndex(), luaLandCallbacks[i]);
        LuaBindings_PushPlayer(player);
        lua_pushnumber(L, distance);
        Lua_CallFunc(2, 0);
    }
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

static Entity *LuaBindings_CheckPlayerEntity(void) {
    int id = LuaBindings_CheckPlayer()->entityId;
    if (!serverWorld.entities || id < 0 || id >= WORLD_MAX_ENTITIES || !serverWorld.entities[id].active) {
        Lua_Error("player has no entity");
        return NULL;
    }
    return &serverWorld.entities[id];
}

static int LuaBindings_GetPlayerPosition(void) {
    Vector3 position = LuaBindings_CheckPlayerEntity()->position;
    LuaBindings_PushPosition(position);
    return 1;
}

static int LuaBindings_GetPlayerEyePosition(void) {
    Vector3 position = LuaBindings_CheckPlayerEntity()->position;
    position.y += 1.5f;
    LuaBindings_PushPosition(position);
    return 1;
}

static int LuaBindings_GetPlayerLookDirection(void) {
    Vector3 rotation = LuaBindings_CheckPlayerEntity()->rotation;
    float yaw = rotation.y;
    float pitch = rotation.x;
    float horizontal = cosf(pitch);
    LuaBindings_PushPosition((Vector3){
        sinf(yaw) * horizontal, -sinf(pitch), cosf(yaw) * horizontal
    });
    return 1;
}

static int LuaBindings_TeleportPlayer(void) {
    Player *player = LuaBindings_CheckPlayer();
    if (player->disconnected || player == luaLeavingPlayer) return Lua_Error("player is leaving");
    int id = player->entityId;
    Vector3 position = LuaBindings_ReadPosition(2, false);
    if (!serverWorld.entities || id < 0 || !serverWorld.entities[id].active) {
        return Lua_Error("player is not connected");
    }
    ServerPlayer_Teleport(player, position);
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
    int modelId = LuaModels_Resolve(2, false);
    if (!ServerWorld_SetEntityModel(player->entityId, modelId)) return Lua_Error("model is not defined");
    return 0;
}

static int GetPlayerInventory(void) { return LuaInventory_Get(L, LuaBindings_CheckPlayer()); }
static int ShowPlayerInventory(void) { return LuaInventory_Show(L, LuaBindings_CheckPlayer()); }
static int ClosePlayerInventory(void) { return LuaInventory_Close(L, LuaBindings_CheckPlayer()); }
bool LuaBindings_InteractBlock(Player *player, Vector3 position, int blockId) {
    if (!luaRunning || blockId < 1 || blockId > 255 || !serverWorld.hasBlockDefinition[blockId] ||
        blockInteractions[blockId] < 0) return false;
    int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, blockInteractions[blockId]);
    LuaBindings_PushPlayer(player);
    LuaMetadata_PushBlock(L, position);
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) TraceLog(LOG_WARNING, "Block on_interact: %s", lua_tostring(L, -1));
    lua_settop(L, top);
    return true; // A registered interaction consumes right-click, including on error.
}
static int GetSelectedStack(void) {
    ServerItems_PushStack(L,*Inventory_GetSelected(&LuaBindings_CheckPlayer()->inventory)); return 1;
}
static int GetSelectedSlot(void) {
    Player *player=LuaBindings_CheckPlayer();
    lua_pushinteger(L,Inventory_GetSelected(&player->inventory)-player->inventory.slots+1); return 1;
}
static int SetSelectedStack(void) {
    Player *player=LuaBindings_CheckPlayer();
    if (player->disconnected || player==luaLeavingPlayer) return Lua_Error("player is leaving");
    ItemStack stack; ServerItems_ReadStack(L,2,&stack);
    if (stack.count && !ServerItems_IsDefined(stack.itemId)) return Lua_Error("item is not defined");
    *Inventory_GetSelected(&player->inventory)=stack;
    player->inventoryRevision++;
    ServerInventory_UpdateHeldBlock(player); ServerInventory_Send(player);
    return 0;
}
static int GetPlayerMetadata(void) { return LuaMetadata_Player(L,LuaBindings_CheckPlayer(),false,false); }
static int SetPlayerMetadata(void) { return LuaMetadata_Player(L,LuaBindings_CheckPlayer(),true,false); }
static int ResetPlayerMetadata(void) { return LuaMetadata_Player(L,LuaBindings_CheckPlayer(),true,true); }
static int GetPlayerHP(void) {
    Player *player=LuaBindings_CheckPlayer();
    lua_settop(L,1); lua_pushliteral(L,"midless:hp");
    return LuaMetadata_Player(L,player,false,false);
}
static int SetPlayerHP(void) {
    Player *player=LuaBindings_CheckPlayer();
    lua_Integer hp=luaL_checkinteger(L,2);
    if (hp<0 || hp>65535) return luaL_error(L,"hp must be 0..65535");
    lua_settop(L,1); lua_pushliteral(L,"midless:hp"); lua_pushinteger(L,hp);
    return LuaMetadata_Player(L,player,true,false);
}
static int GetPlayerSpawnPoint(void) {
    LuaBindings_PushPosition(LuaBindings_CheckPlayer()->spawnPoint);
    return 1;
}
static int SetPlayerSpawnPoint(void) {
    Player *player = LuaBindings_CheckPlayer();
    Vector3 position = LuaBindings_ReadPosition(2, false);
    if (!isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z) ||
        fabsf(position.x) > 1000000 || fabsf(position.y) > 1000000 || fabsf(position.z) > 1000000)
        return Lua_Error("spawn point must be within one million blocks of the origin");
    player->spawnPoint = position;
    return 0;
}
static int SetPlayerHudBar(void) { return ServerHudBars_Set(LuaBindings_CheckPlayer()); }
static const struct LuaMethod playerLib[] = {
    {"set_hud_bar", SetPlayerHudBar},
    {"get_hp", GetPlayerHP},
    {"set_hp", SetPlayerHP},
    {"get_spawn_point", GetPlayerSpawnPoint},
    {"set_spawn_point", SetPlayerSpawnPoint},
    {"get_metadata", GetPlayerMetadata},
    {"set_metadata", SetPlayerMetadata},
    {"reset_metadata", ResetPlayerMetadata},
    {"get_selected_stack", GetSelectedStack},
    {"set_selected_stack", SetSelectedStack},
    {"get_selected_slot", GetSelectedSlot},
    {"get_inventory", GetPlayerInventory},
    {"show_inventory", ShowPlayerInventory},
    {"close_inventory", ClosePlayerInventory},
    {"get_id", LuaBindings_GetPlayerId},
    {"set_model", LuaBindings_SetPlayerModel},
    {"get_name", LuaBindings_GetPlayerName},
    {"get_position", LuaBindings_GetPlayerPosition},
    {"get_eye_position", LuaBindings_GetPlayerEyePosition},
    {"get_look_direction", LuaBindings_GetPlayerLookDirection},
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

bool LuaBindings_InvokeChatMessage(int playerId, const char *message) {
    if(luaRunning == 0) return false;
    if (!serverWorld.players || playerId < 0 || playerId >= WORLD_MAX_PLAYERS) return false;
    Player *player = serverWorld.players[playerId];
    if (!player || player->disconnected) return false;
    int count = arrlen(luaChatMessageCallbacks);
    for(int i = 0; i < count; i++) {
        Lua_GetRawI(Lua_GetRegistryIndex(), luaChatMessageCallbacks[i]);
            LuaBindings_PushPlayer(player);
            Lua_PushString(message);
        if (Lua_CallFuncHandled(2)) return true;
    }
    return false;
}

static int *luaPlayerClickCallbacks;

static int LuaBindings_RegisterPlayerClick(void) {
    int callback = Lua_RefFunction(1);
    arrput(luaPlayerClickCallbacks, callback);
    return 0;
}

void LuaBindings_InvokePlayerClick(int playerId, int button) {
    if (!luaRunning || !serverWorld.players || playerId < 0 ||
        playerId >= WORLD_MAX_PLAYERS || button < 0 || button > 1) return;
    Player *player = serverWorld.players[playerId];
    if (!player || player->disconnected) return;
    int count = arrlen(luaPlayerClickCallbacks);
    for (int i = 0; i < count; i++) {
        Lua_GetRawI(Lua_GetRegistryIndex(), luaPlayerClickCallbacks[i]);
        LuaBindings_PushPlayer(player);
        Lua_PushString(button == 0 ? "left" : "right");
        Lua_CallFunc(2, 0);
    }
}

int LuaBindings_BroadcastMessage(void) {
    const char *message = Lua_GetString(1);
    ServerWorld_SendMessage(message);
    return 0;
}

static int LuaBindings_DefineTexture(void) {
    char name[65];
    Lua_CopyString(1,name,sizeof(name));
    char path[1024];
    Lua_CopyString(2,path,sizeof(path));
    if(!ServerTextures_Define(name,path)) return Lua_Error("texture registration failed: invalid PNG, limits exceeded, reserved name, or incompatible dimensions");
    return 0;
}
static int LuaBindings_SetTerrainTexture(void) {
    int id=ServerTextures_Find(Lua_GetString(1));
    if(!ServerTextures_SetTerrain(id)) return Lua_Error("terrain texture must be a defined 256x256 PNG or 'terrain'");
    return 0;
}
static const struct LuaMethod midlessLib[] = {
    {"define_player_inventory", LuaInventory_Define},
    {"define_player_inventory_screen", LuaInventory_DefineScreen},
    {"define_recipe", Crafting_Register},
    {"define_texture", LuaBindings_DefineTexture},
    {"define_hud_bar", ServerHudBars_Define},
    {"remove_hud_bar", ServerHudBars_Remove},
    {"define_player_metadata", LuaMetadata_DefinePlayer},
    {"register_on_player_metadata_change", LuaMetadata_RegisterPlayerChange},
    {"register_on_hp_change", LuaMetadata_RegisterHPChange},
    {"register_on_dig_time", LuaDigging_Register},
    {"set_breaking_texture", ServerTextures_SetBreaking},
    {"set_terrain_texture", LuaBindings_SetTerrainTexture},
    {"define_entity", LuaEntities_Register},
    {"spawn_entity", LuaEntities_Spawn},
    {"get_player_by_id", LuaBindings_GetPlayerById},
    {"get_player_by_name", LuaBindings_GetPlayerByName},
    {"get_players", LuaBindings_ListPlayers},
    {"get_block", LuaMetadata_GetBlock},
    {"set_block", LuaBindings_SetBlock},
    {"set_blocks", LuaBindings_SetBlocks},
    {"define_block", LuaBindings_DefineBlock},
    {"define_item", ServerItems_Define},
    {"define_entity_model", LuaBindings_DefineEntityModel},
    {"remove_entity_model", LuaBindings_RemoveEntityModel},
    {"set_entity_model", LuaBindings_SetEntityModel},
    {"register_on_ready", LuaBindings_RegisterReady},
    {"register_on_step", LuaBindings_RegisterStep},
    {"register_on_player_message", LuaBindings_RegisterChatMessage},
    {"register_on_player_click", LuaBindings_RegisterPlayerClick},
    {"register_on_player_join", LuaBindings_RegisterPlayerJoin},
    {"register_on_player_land", LuaBindings_RegisterPlayerLand},
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
    LuaDigging_Init();
    LuaItemActions_Init();
    Crafting_Reset();
    for (int i = 0; i < 256; i++) blockInteractions[i] = LUA_NOREF;
    LuaInventory_Init();
    LuaVector_Init();
    LuaMetadata_Init();
    LuaEntities_Init();
    luaReadyInvoked = false;
    LuaBindings_DefineBlockConstants();
    LuaBindings_DefineModelConstants();
    Lua_DefineObjectType(LUA_PLAYER_TYPE, playerLib);
    Lua_DefineLib("midless", midlessLib);
    LuaWorldgen_Init();
}

void LuaBindings_Shutdown(void) {
    ServerHudBars_Reset();
    LuaDigging_Shutdown();
    LuaItemActions_Shutdown();
    LuaInventory_Shutdown();
    Crafting_Reset();
    for (int i = 0; i < 256; i++) luaL_unref(L, LUA_REGISTRYINDEX, blockInteractions[i]);
    for (int i = 0; i < arrlen(luaPlayerClickCallbacks); i++)
        Lua_Unref(Lua_GetRegistryIndex(), luaPlayerClickCallbacks[i]);
    arrfree(luaPlayerClickCallbacks);
    luaPlayerClickCallbacks = NULL;
    LuaEntities_Shutdown();
    LuaMetadata_Shutdown();
    arrfree(luaJoinCallbacks);
    luaJoinCallbacks = NULL;
    arrfree(luaLeaveCallbacks);
    luaLeaveCallbacks = NULL;
    for (int i = 0; i < arrlen(luaLandCallbacks); i++) Lua_Unref(Lua_GetRegistryIndex(), luaLandCallbacks[i]);
    arrfree(luaLandCallbacks);
    luaLandCallbacks = NULL;
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

