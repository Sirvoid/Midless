/**
 * Copyright (c) 2021 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "luablocks.h"
#include "luaengine.h"
#include "luavalues.h"
#include "luaplayers.h"
#include "luaitems.h"
#include "luadigging.h"
#include "luaitemactions.h"
#include "luametadata.h"
#include "luablockstates.h"
#include "../world/world.h"
#include "../networkhandler.h"
#include "../packet.h"
#include "stb_ds.h"

extern lua_State *L;
static int blockInteractions[256];
static int *luaBlockUpdateCallbacks;
#define LUA_BLOCK_BATCH_SIZE 4096
#define LUA_MAX_BLOCK_UPDATES 1000000

int LuaBlocks_RegisterBlockUpdate(void) {
    int callback = Lua_RefFunction(1);
    arrput(luaBlockUpdateCallbacks, callback);
    return 0;
}

void ScriptHooks_BlockUpdate(Vector3 position, unsigned short blockId,
                             unsigned short previousBlockId) {
    if (luaRunning == 0)
        return;
    for (int i = 0; i < arrlen(luaBlockUpdateCallbacks); i++) {
        Lua_GetRawI(Lua_GetRegistryIndex(), luaBlockUpdateCallbacks[i]);
        LuaValues_PushPosition(position);
        LuaItems_PushId(L, blockId);
        LuaItems_PushId(L, previousBlockId);
        Lua_CallFunc(3, 0);
    }
}

int LuaBlocks_SetBlock(void) {
    Vector3 position = LuaValues_ReadPosition(1, true);
    int blockId = LuaItems_Id(L, 2, true, false);
    if (!ServerWorld_IsBlockDefined(blockId))
        return luaL_error(L, "block is not defined");
    ServerWorld_SetBlock(position, blockId, true, false, true);
    return 0;
}

static ServerBlockUpdate ReadBlockUpdate(int table) {
    Lua_CheckTable(table);

    Lua_PushField(table, "pos");
    Vector3 position = LuaValues_ReadPosition(-1, true);
    Lua_Pop();

    Lua_PushField(table, "blockId");
    int blockId = LuaItems_Id(L, -1, true, false);
    Lua_Pop();
    if (!ServerWorld_IsBlockDefined(blockId))
        Lua_Error("blockId is not defined");

    return (ServerBlockUpdate){.position = position, .blockId = (unsigned char)blockId};
}

int LuaBlocks_SetBlocks(void) {
    Lua_CheckTable(1);
    bool callCallbacks = Lua_GetTop() < 2 || Lua_GetBoolean(2);
    int count = Lua_TableLength(1);
    if (count < 0 || count > LUA_MAX_BLOCK_UPDATES)
        return Lua_Error("set_blocks accepts at most 1000000 updates");

    // Validate the complete input before changing any blocks.
    for (int i = 0; i < count; i++) {
        Lua_GetRawI(1, i + 1);
        ReadBlockUpdate(-1);
        Lua_Pop();
    }

    ServerBlockUpdate *updates = count > 0 ? MemAlloc(sizeof(*updates) * count) : NULL;
    if (count > 0 && updates == NULL)
        return Lua_Error("could not allocate block update batch");

    int changedCount = 0;
    for (int i = 0; i < count; i++) {
        Lua_GetRawI(1, i + 1);
        ServerBlockUpdate update = ReadBlockUpdate(-1);
        Lua_Pop();

        int previousBlockId = ServerWorld_GetBlock(update.position);
        if (previousBlockId == update.blockId)
            continue;
        ServerWorld_SetBlock(update.position, update.blockId, false, false, callCallbacks);
        if (ServerWorld_GetBlock(update.position) == update.blockId)
            updates[changedCount++] = update;
    }

    for (int offset = 0; offset < changedCount; offset += LUA_BLOCK_BATCH_SIZE) {
        int remaining = changedCount - offset;
        unsigned short batchCount =
            (unsigned short)(remaining < LUA_BLOCK_BATCH_SIZE ? remaining : LUA_BLOCK_BATCH_SIZE);
        for (int playerId = 0; playerId < WORLD_MAX_PLAYERS; playerId++) {
            Player *player = serverWorld.players[playerId];
            if (player == NULL)
                continue;
            ServerNetwork_Send(player, ServerPacket_CreateBlockBatch(updates + offset, batchCount));
        }
    }

    MemFree(updates);
    return 0;
}

static void ReadBounds(int table, const char *name, uint8_t values[3]) {
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

static void ReadBlockTable(BlockDefinition *d) {
    Lua_PushField(2, "name");
    Lua_CopyString(-1, d->name, sizeof(d->name));
    Lua_Pop();
    d->modelType =
        LuaValues_IntField(2, "model", BLOCK_MODEL_SOLID, BLOCK_MODEL_GAS, BLOCK_MODEL_SPRITE);
    d->renderType = LuaValues_IntField(2, "render", BLOCK_RENDER_OPAQUE, BLOCK_RENDER_OPAQUE,
                                       BLOCK_RENDER_TRANSLUCENT);
    d->colliderType = LuaValues_IntField(2, "collider", BLOCK_COLLIDER_SOLID, BLOCK_COLLIDER_NONE,
                                         BLOCK_COLLIDER_LIQUID);
    d->lightType =
        LuaValues_IntField(2, "light", BLOCK_LIGHT_NONE, BLOCK_LIGHT_NONE, BLOCK_LIGHT_EMIT);
    for (int i = 0; i < 3; i++)
        d->max[i] = 16;
    if (Lua_PushField(2, "bounds")) {
        Lua_CheckTable(-1);
        int bounds = Lua_GetTop();
        ReadBounds(bounds, "min", d->min);
        ReadBounds(bounds, "max", d->max);
    }
    Lua_Pop();

    // Require textures, with each face resolved from all -> sides -> face.
    Lua_PushField(2, "textures");
    Lua_CheckTable(-1);
    int textures = Lua_GetTop();
    int all = LuaValues_IntField(textures, "all", -1, 0, 255);
    int sides = LuaValues_IntField(textures, "sides", all, 0, 255);
    const char *faces[] = {"left", "right", "top", "bottom", "front", "back"};
    for (int i = 0; i < 6; i++) {
        int fallback = (i == 2 || i == 3) ? all : sides;
        int texture = LuaValues_IntField(textures, faces[i], fallback, 0, 255);
        if (texture < 0)
            Lua_Error(
                "textures must specify all faces, using all, sides, or individual face names");
        d->textures[i] = texture;
    }
    Lua_Pop();
}

int LuaBlocks_DefineBlock(void) {
    int blockId = LuaItems_Declare(L, true);
    if (!blockId || serverWorld.hasBlockDefinition[blockId])
        return luaL_error(L, "block already defined");
    BlockDefinition definition = {0};
    Lua_CheckTable(2);
    ReadBlockTable(&definition);
    if (!BlockDefinition_Validate(blockId, &definition)) {
        return Lua_Error("invalid block definition");
    }
    if (!serverWorld.players) {
        return Lua_Error("midless.define_block must run after world initialization.");
    }
    lua_getfield(L, 2, "on_interact");
    if (!lua_isnil(L, -1))
        luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_pop(L, 1);
    LuaDigging_Define(blockId, 2, true);
    LuaItemActions_Define(blockId, 2, true);
    LuaMetadata_DefineBlock(blockId, 2);
    LuaBlockStates_Define(L, blockId, 2, &definition);
    lua_getfield(L, 2, "item_metadata");
    if (!lua_isnil(L, -1)) {
        lua_newtable(L);
        lua_pushvalue(L, -2);
        lua_setfield(L, -2, "metadata");
        LuaMetadata_DefineItem(blockId, lua_gettop(L));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    luaL_unref(L, LUA_REGISTRYINDEX, blockInteractions[blockId]);
    lua_getfield(L, 2, "on_interact");
    blockInteractions[blockId] = luaL_ref(L, LUA_REGISTRYINDEX);
    ServerWorld_DefineBlock(blockId, &definition);
    return 0;
}

bool ScriptHooks_InteractBlock(Player *player, Vector3 position, int blockId) {
    if (!luaRunning || blockId < 1 || blockId > 255 || !serverWorld.hasBlockDefinition[blockId] ||
        blockInteractions[blockId] < 0)
        return false;
    int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, blockInteractions[blockId]);
    LuaPlayers_Push(player);
    LuaMetadata_PushBlock(L, position);
    if (lua_pcall(L, 2, 0, 0) != LUA_OK)
        TraceLog(LOG_WARNING, "Block on_interact: %s", lua_tostring(L, -1));
    lua_settop(L, top);
    return true; // A registered interaction consumes right-click, including on error.
}

static void LuaBlocks_InitConstants(void) {
    static const LuaConstant models[] = {{"GAS", BLOCK_MODEL_GAS},
                                         {"SOLID", BLOCK_MODEL_SOLID},
                                         {"SPRITE", BLOCK_MODEL_SPRITE},
                                         {NULL, 0}};
    static const LuaConstant renders[] = {{"OPAQUE", BLOCK_RENDER_OPAQUE},
                                          {"TRANSPARENT", BLOCK_RENDER_TRANSPARENT},
                                          {"TRANSLUCENT", BLOCK_RENDER_TRANSLUCENT},
                                          {NULL, 0}};
    static const LuaConstant colliders[] = {{"NONE", BLOCK_COLLIDER_NONE},
                                            {"SOLID", BLOCK_COLLIDER_SOLID},
                                            {"LIQUID", BLOCK_COLLIDER_LIQUID},
                                            {NULL, 0}};
    static const LuaConstant lights[] = {
        {"NONE", BLOCK_LIGHT_NONE}, {"EMIT", BLOCK_LIGHT_EMIT}, {NULL, 0}};
    Lua_MakeTable(4);
    LuaValues_ConstantTable(models);
    Lua_SetField(-2, "model");
    LuaValues_ConstantTable(renders);
    Lua_SetField(-2, "render");
    LuaValues_ConstantTable(colliders);
    Lua_SetField(-2, "collider");
    LuaValues_ConstantTable(lights);
    Lua_SetField(-2, "light");
    Lua_SetGlobal("block");
}

void LuaBlocks_Init(void) {
    for (int i = 0; i < 256; i++)
        blockInteractions[i] = LUA_NOREF;
    LuaBlocks_InitConstants();
}

void LuaBlocks_Shutdown(void) {
    for (int i = 0; i < 256; i++) {
        luaL_unref(L, LUA_REGISTRYINDEX, blockInteractions[i]);
        blockInteractions[i] = LUA_NOREF;
    }
    for (int i = 0; i < arrlen(luaBlockUpdateCallbacks); i++) {
        Lua_Unref(Lua_GetRegistryIndex(), luaBlockUpdateCallbacks[i]);
    }
    arrfree(luaBlockUpdateCallbacks);
    luaBlockUpdateCallbacks = NULL;
}
