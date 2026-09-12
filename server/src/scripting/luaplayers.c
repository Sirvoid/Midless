/**
 * Copyright (c) 2021 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "luaplayers.h"
#include "luaengine.h"
#include "luavalues.h"
#include "luachat.h"
#include "luaitems.h"
#include "luainventory.h"
#include "luametadata.h"
#include "luamodels.h"
#include "luadamage.h"
#include "luahudbars.h"
#include "luatextcolors.h"
#include "luaentitytexture.h"
#include "../serverinventory.h"
#include "../world/world.h"
#include "../networkhandler.h"
#include "../packet.h"
#include "stb_ds.h"
#include <math.h>
#include <string.h>

extern lua_State *L;

#define LUA_PLAYER_TYPE "midless.Player"
static int *luaJoinCallbacks, *luaLeaveCallbacks, *luaLandCallbacks;
static Player *luaLeavingPlayer;
typedef struct LuaPlayerHandle {
    int id;
    uint64_t connectionId;
} LuaPlayerHandle;

Entity *LuaPlayers_TestEntity(lua_State *state, int index) {
    LuaPlayerHandle *h = luaL_testudata(state, index, LUA_PLAYER_TYPE);
    Player *p = h && serverWorld.players && h->id >= 0 && h->id < WORLD_MAX_PLAYERS
                    ? serverWorld.players[h->id]
                    : NULL;
    if (!p || p->disconnected || !p->movementReady || p->connectionId != h->connectionId ||
        !serverWorld.entities || p->entityId < 0 || p->entityId >= WORLD_MAX_ENTITIES)
        return NULL;
    Entity *e = &serverWorld.entities[p->entityId];
    return e->active && !e->pendingRemoval && e->ownerPlayerId == p->id ? e : NULL;
}
static int IsPlayerValid(void) {
    lua_pushboolean(L, LuaPlayers_TestEntity(L, 1) != NULL);
    return 1;
}

void LuaPlayers_Push(Player *player) {
    if (!player || (player->disconnected && player != luaLeavingPlayer)) {
        Lua_PushString(NULL);
        return;
    }
    LuaPlayerHandle *handle = Lua_NewObject(LUA_PLAYER_TYPE, sizeof(*handle));
    handle->id = player->id;
    handle->connectionId = player->connectionId;
}

Player *LuaPlayers_Check(void) {
    LuaPlayerHandle *handle = Lua_CheckObject(1, LUA_PLAYER_TYPE);
    Player *player = serverWorld.players ? serverWorld.players[handle->id] : NULL;
    if (!player || (player->disconnected && player != luaLeavingPlayer) ||
        player->connectionId != handle->connectionId) {
        Lua_Error("player is no longer connected");
        return NULL;
    }
    return player;
}

int LuaPlayers_RegisterJoin(void) {
    int callback = Lua_RefFunction(1);
    arrput(luaJoinCallbacks, callback);
    return 0;
}

int LuaPlayers_RegisterLeave(void) {
    int callback = Lua_RefFunction(1);
    arrput(luaLeaveCallbacks, callback);
    return 0;
}

static void InvokePlayerEvent(int playerId, bool leaving) {
    if (!luaRunning || !serverWorld.players || playerId < 0 || playerId >= WORLD_MAX_PLAYERS)
        return;
    Player *player = serverWorld.players[playerId];
    if (!player || (!leaving && player->disconnected))
        return;
    Player *previousLeavingPlayer = luaLeavingPlayer;
    if (leaving)
        luaLeavingPlayer = player;
    int count = leaving ? arrlen(luaLeaveCallbacks) : arrlen(luaJoinCallbacks);
    for (int i = 0; i < count; i++) {
        int callback = leaving ? luaLeaveCallbacks[i] : luaJoinCallbacks[i];
        Lua_GetRawI(Lua_GetRegistryIndex(), callback);
        LuaPlayers_Push(player);
        Lua_CallFunc(1, 0);
    }
    luaLeavingPlayer = previousLeavingPlayer;
}

void ScriptHooks_PlayerJoin(int playerId) {
    InvokePlayerEvent(playerId, false);
}

void ScriptHooks_PlayerLeave(int playerId) {
    InvokePlayerEvent(playerId, true);
}

int LuaPlayers_RegisterLand(void) {
    int callback = Lua_RefFunction(1);
    arrput(luaLandCallbacks, callback);
    return 0;
}

void ScriptHooks_PlayerLand(int playerId, float distance) {
    if (!luaRunning || !serverWorld.players || playerId < 0 || playerId >= WORLD_MAX_PLAYERS)
        return;
    Player *player = serverWorld.players[playerId];
    if (!player || player->disconnected)
        return;
    int count = arrlen(luaLandCallbacks);
    for (int i = 0; i < count; i++) {
        Lua_GetRawI(Lua_GetRegistryIndex(), luaLandCallbacks[i]);
        LuaPlayers_Push(player);
        lua_pushnumber(L, distance);
        Lua_CallFunc(2, 0);
    }
}

int LuaPlayers_GetById(void) {
    int id = Lua_GetIntRange(1, 0, WORLD_MAX_PLAYERS - 1);
    LuaPlayers_Push(serverWorld.players ? serverWorld.players[id] : NULL);
    return 1;
}

int LuaPlayers_GetByName(void) {
    const char *name = Lua_GetString(1);
    if (serverWorld.players) {
        for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
            Player *player = serverWorld.players[i];
            if (player && !player->disconnected && player->name && !strcmp(player->name, name)) {
                LuaPlayers_Push(player);
                return 1;
            }
        }
    }
    Lua_PushString(NULL);
    return 1;
}

static int GetPlayerId(void) {
    Lua_PushInt(LuaPlayers_Check()->id);
    return 1;
}

int LuaPlayers_List(void) {
    Lua_MakeTable(0);
    if (!serverWorld.players)
        return 1;

    int index = 1;
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *player = serverWorld.players[i];
        if (!player || player->disconnected)
            continue;
        LuaPlayers_Push(player);
        Lua_SetRawI(-2, index++);
    }
    return 1;
}

static int GetPlayerName(void) {
    Lua_PushString(LuaPlayers_Check()->name);
    return 1;
}

static Entity *CheckPlayerEntity(void) {
    int id = LuaPlayers_Check()->entityId;
    if (!serverWorld.entities || id < 0 || id >= WORLD_MAX_ENTITIES ||
        !serverWorld.entities[id].active) {
        Lua_Error("player has no entity");
        return NULL;
    }
    return &serverWorld.entities[id];
}

static int GetPlayerPosition(void) {
    Vector3 position = CheckPlayerEntity()->position;
    LuaValues_PushPosition(position);
    return 1;
}

static int GetPlayerEyePosition(void) {
    Vector3 position = CheckPlayerEntity()->position;
    position.y += 1.5f;
    LuaValues_PushPosition(position);
    return 1;
}

static int GetPlayerLookDirection(void) {
    Vector3 rotation = CheckPlayerEntity()->rotation;
    float yaw = rotation.y;
    float pitch = rotation.x;
    float horizontal = cosf(pitch);
    LuaValues_PushPosition((Vector3){sinf(yaw) * horizontal, -sinf(pitch), cosf(yaw) * horizontal});
    return 1;
}

static int TeleportPlayer(void) {
    Player *player = LuaPlayers_Check();
    if (player->disconnected || player == luaLeavingPlayer)
        return Lua_Error("player is leaving");
    int id = player->entityId;
    Vector3 position = LuaValues_ReadPosition(2, false);
    if (!serverWorld.entities || id < 0 || !serverWorld.entities[id].active) {
        return Lua_Error("player is not connected");
    }
    ServerPlayer_Teleport(player, position);
    return 0;
}

static int SetPlayerModel(void) {
    Player *player = LuaPlayers_Check();
    if (player->disconnected || player == luaLeavingPlayer)
        return Lua_Error("player is leaving");
    int modelId = LuaModels_Resolve(2, false);
    if (!ServerWorld_SetEntityModel(player->entityId, modelId))
        return Lua_Error("model is not defined");
    return 0;
}

static int GetPlayerInventory(void) {
    return LuaInventory_Get(L, LuaPlayers_Check());
}
static int ShowPlayerInventory(void) {
    return LuaInventory_Show(L, LuaPlayers_Check());
}
static int ClosePlayerInventory(void) {
    return LuaInventory_Close(L, LuaPlayers_Check());
}
static int GetSelectedStack(void) {
    LuaItems_PushStack(L, *Inventory_GetSelected(&LuaPlayers_Check()->inventory));
    return 1;
}
static int GetSelectedSlot(void) {
    Player *player = LuaPlayers_Check();
    lua_pushinteger(L, Inventory_GetSelected(&player->inventory) - player->inventory.slots + 1);
    return 1;
}
static int SetSelectedStack(void) {
    Player *player = LuaPlayers_Check();
    if (player->disconnected || player == luaLeavingPlayer)
        return Lua_Error("player is leaving");
    ItemStack stack;
    LuaItems_ReadStack(L, 2, &stack);
    if (stack.count && !ServerItems_IsDefined(stack.itemId))
        return Lua_Error("item is not defined");
    *Inventory_GetSelected(&player->inventory) = stack;
    player->inventoryRevision++;
    ServerInventory_UpdateHeldBlock(player);
    ServerInventory_Send(player);
    return 0;
}
static int GetPlayerMetadata(void) {
    return LuaMetadata_Player(L, LuaPlayers_Check(), false, false);
}
static int SetPlayerMetadata(void) {
    return LuaMetadata_Player(L, LuaPlayers_Check(), true, false);
}
static int ResetPlayerMetadata(void) {
    return LuaMetadata_Player(L, LuaPlayers_Check(), true, true);
}
static float CameraKickOption(const char *name, float fallback, float min, float max) {
    lua_getfield(L, 2, name);
    double value = lua_isnil(L, -1) ? fallback : luaL_checknumber(L, -1);
    if (!isfinite(value) || value < min || value > max)
        luaL_error(L, "%s is outside the camera kick range", name);
    lua_pop(L, 1);
    return (float)value;
}

static int CameraKick(void) {
    Player *player = LuaPlayers_Check();
    if (player->disconnected || player == luaLeavingPlayer)
        return Lua_Error("player is leaving");
    luaL_checktype(L, 2, LUA_TTABLE);
    float pitch = CameraKickOption("pitch", 1.5f, -15, 15);
    float roll = CameraKickOption("roll", 3, -15, 15);
    float duration = CameraKickOption("duration", 0.25f, 0.01f, 2);
    unsigned char *packet = ServerPacket_CreateCameraKick(pitch, roll, duration);
    if (!packet)
        return Lua_Error("cannot allocate camera kick packet");
    ServerNetwork_Send(player, packet);
    return 0;
}

static int GetPlayerHP(void) {
    Player *player = LuaPlayers_Check();
    lua_settop(L, 1);
    lua_pushliteral(L, "midless:hp");
    return LuaMetadata_Player(L, player, false, false);
}
static int SetPlayerHP(void) {
    Player *player = LuaPlayers_Check();
    lua_Integer hp = luaL_checkinteger(L, 2);
    if (hp < 0 || hp > 65535)
        return luaL_error(L, "hp must be 0..65535");
    lua_settop(L, 1);
    lua_pushliteral(L, "midless:hp");
    lua_pushinteger(L, hp);
    return LuaMetadata_Player(L, player, true, false);
}
static int DamagePlayer(void) {
    Player *player = LuaPlayers_Check();
    Entity *entity = CheckPlayerEntity();
    lua_Integer amount = luaL_checkinteger(L, 2);
    if (amount < 0 || amount > 65535)
        return luaL_error(L, "damage must be 0..65535");
    if (lua_isnoneornil(L, 3)) {
        lua_settop(L, 2);
        lua_newtable(L);
    }
    luaL_checktype(L, 3, LUA_TTABLE);
    Vector3 impulse = LuaDamage_Impulse(L, 3, entity);
    if (entity->damageBusy || !amount) {
        lua_pushinteger(L, 0);
        return 1;
    }
    lua_pushcfunction(L, (lua_CFunction)GetPlayerHP);
    lua_pushvalue(L, 1);
    lua_call(L, 1, 1);
    lua_Integer hp = lua_tointeger(L, -1);
    lua_pop(L, 1);
    if (hp <= 0) {
        lua_pushinteger(L, 0);
        return 1;
    }
    entity->damageBusy = true;
    amount = LuaDamage_PlayerHooks(entity, 3, amount);
    // Hooks may change health or teleport; reread before committing damage.
    lua_pushcfunction(L, (lua_CFunction)GetPlayerHP);
    lua_pushvalue(L, 1);
    lua_call(L, 1, 1);
    hp = lua_tointeger(L, -1);
    lua_pop(L, 1);
    if (amount > hp)
        amount = hp;
    if (amount > 0) {
        ServerPlayer_ApplyImpulse(player, impulse);
        // HP notifications may respawn the player. Send the impulse first.
        lua_pushcfunction(L, (lua_CFunction)SetPlayerHP);
        lua_pushvalue(L, 1);
        lua_pushinteger(L, hp - amount);
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            entity->damageBusy = false;
            return lua_error(L);
        }
    }
    entity->damageBusy = false;
    lua_pushinteger(L, amount);
    return 1;
}
static int GetPlayerSpawnPoint(void) {
    LuaValues_PushPosition(LuaPlayers_Check()->spawnPoint);
    return 1;
}
static int SetPlayerSpawnPoint(void) {
    Player *player = LuaPlayers_Check();
    Vector3 position = LuaValues_ReadPosition(2, false);
    if (!isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z) ||
        fabsf(position.x) > 1000000 || fabsf(position.y) > 1000000 || fabsf(position.z) > 1000000)
        return Lua_Error("spawn point must be within one million blocks of the origin");
    player->spawnPoint = position;
    return 0;
}
static int SetPlayerNametag(void) {
    return LuaNametag_Set(L, CheckPlayerEntity());
}
static int SetPlayerTexture(void) {
    return LuaEntityTexture_Set(L, CheckPlayerEntity());
}
static int GetPlayerTexture(void) {
    return LuaEntityTexture_Get(L, CheckPlayerEntity());
}
static int SetPlayerHudBar(void) {
    return LuaHudBars_Set(L, LuaPlayers_Check());
}
static int ApplyPlayerImpulse(void) {
    Player *player = LuaPlayers_Check();
    luaL_checktype(L, 2, LUA_TTABLE);
    Vector3 impulse;
    float *components[] = {&impulse.x, &impulse.y, &impulse.z};
    const char *names[] = {"x", "y", "z"};
    for (int i = 0; i < 3; i++) {
        lua_getfield(L, 2, names[i]);
        double value = luaL_checknumber(L, -1);
        if (!isfinite(value) || fabs(value) > 20)
            return luaL_error(
                L, "player impulse components must be finite and within -20 to 20 blocks/s");
        *components[i] = value;
        lua_pop(L, 1);
    }
    if (!ServerPlayer_ApplyImpulse(player, impulse))
        return luaL_error(L, "player is not ready to receive an impulse");
    return 0;
}
static const struct LuaMethod playerLib[] = {{"set_texture", SetPlayerTexture},
                                             {"get_texture", GetPlayerTexture},
                                             {"is_valid", IsPlayerValid},
                                             {"damage", DamagePlayer},
                                             {"apply_impulse", ApplyPlayerImpulse},
                                             {"camera_kick", CameraKick},
                                             {"set_hud_bar", SetPlayerHudBar},
                                             {"set_nametag", SetPlayerNametag},
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
                                             {"get_id", GetPlayerId},
                                             {"set_model", SetPlayerModel},
                                             {"get_name", GetPlayerName},
                                             {"get_position", GetPlayerPosition},
                                             {"get_eye_position", GetPlayerEyePosition},
                                             {"get_look_direction", GetPlayerLookDirection},
                                             {"teleport", TeleportPlayer},
                                             {"send_message", LuaChat_SendPlayerMessage},
                                             {NULL, NULL}};

static int *luaPlayerClickCallbacks;

int LuaPlayers_RegisterClick(void) {
    int callback = Lua_RefFunction(1);
    arrput(luaPlayerClickCallbacks, callback);
    return 0;
}

void ScriptHooks_PlayerClick(int playerId, int button) {
    if (!luaRunning || !serverWorld.players || playerId < 0 || playerId >= WORLD_MAX_PLAYERS ||
        button < 0 || button > 1)
        return;
    Player *player = serverWorld.players[playerId];
    if (!player || player->disconnected)
        return;
    if (button == 0)
        ScriptHooks_QueriesAttack(player);
    int count = arrlen(luaPlayerClickCallbacks);
    for (int i = 0; i < count; i++) {
        Lua_GetRawI(Lua_GetRegistryIndex(), luaPlayerClickCallbacks[i]);
        LuaPlayers_Push(player);
        Lua_PushString(button == 0 ? "left" : "right");
        Lua_CallFunc(2, 0);
    }
}

bool LuaPlayers_IsLeaving(const Player *player) {
    return player->disconnected || player == luaLeavingPlayer;
}

void LuaPlayers_Init(void) {
    luaLeavingPlayer = NULL;
    Lua_DefineObjectType(LUA_PLAYER_TYPE, playerLib);
}

void LuaPlayers_Shutdown(void) {
    for (int i = 0; i < arrlen(luaJoinCallbacks); i++) {
        Lua_Unref(Lua_GetRegistryIndex(), luaJoinCallbacks[i]);
    }
    arrfree(luaJoinCallbacks);
    luaJoinCallbacks = NULL;
    for (int i = 0; i < arrlen(luaLeaveCallbacks); i++) {
        Lua_Unref(Lua_GetRegistryIndex(), luaLeaveCallbacks[i]);
    }
    arrfree(luaLeaveCallbacks);
    luaLeaveCallbacks = NULL;
    for (int i = 0; i < arrlen(luaLandCallbacks); i++) {
        Lua_Unref(Lua_GetRegistryIndex(), luaLandCallbacks[i]);
    }
    arrfree(luaLandCallbacks);
    luaLandCallbacks = NULL;
    for (int i = 0; i < arrlen(luaPlayerClickCallbacks); i++) {
        Lua_Unref(Lua_GetRegistryIndex(), luaPlayerClickCallbacks[i]);
    }
    arrfree(luaPlayerClickCallbacks);
    luaPlayerClickCallbacks = NULL;
    luaLeavingPlayer = NULL;
}
