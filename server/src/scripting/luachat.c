/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "luachat.h"
#include "luaengine.h"
#include "luaplayers.h"
#include "../world/world.h"
#include "stb_ds.h"

static int *luaChatMessageCallbacks;

int LuaChat_RegisterMessage(lua_State *state) {
    (void)state;
    int callback = Lua_RefFunction(1);
    arrput(luaChatMessageCallbacks, callback);
    return 0;
}

bool ScriptHooks_ChatMessage(int playerId, const char *message) {
    if (luaRunning == 0)
        return false;
    if (!serverWorld.players || playerId < 0 || playerId >= WORLD_MAX_PLAYERS)
        return false;
    Player *player = serverWorld.players[playerId];
    if (!player || player->disconnected)
        return false;
    int count = arrlen(luaChatMessageCallbacks);
    for (int i = 0; i < count; i++) {
        Lua_GetRawI(Lua_GetRegistryIndex(), luaChatMessageCallbacks[i]);
        LuaPlayers_Push(player);
        Lua_PushString(message);
        if (Lua_CallFuncHandled(2))
            return true;
    }
    return false;
}

int LuaChat_Broadcast(lua_State *state) {
    (void)state;
    const char *message = Lua_GetString(1);
    ServerWorld_SendMessage(message);
    return 0;
}

int LuaChat_SendPlayerMessage(lua_State *state) {
    (void)state;
    Player *player = LuaPlayers_Check();
    const char *message = Lua_GetString(2);
    if (LuaPlayers_IsLeaving(player))
        return Lua_Error("player is leaving");
    ServerPlayer_SendMessage(player, message);
    return 0;
}

void LuaChat_Shutdown(void) {
    for (int i = 0; i < arrlen(luaChatMessageCallbacks); i++) {
        Lua_Unref(Lua_GetRegistryIndex(), luaChatMessageCallbacks[i]);
    }
    arrfree(luaChatMessageCallbacks);
    luaChatMessageCallbacks = NULL;
}
