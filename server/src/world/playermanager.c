#include <limits.h>
#include <stddef.h>
#include "world.h"
#include "../player.h"
#include "../networkhandler.h"
#include "../packet.h"
#include "../scripting/luabindings.h"

void ServerPlayerManager_Update(void) {
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *player = serverWorld.players[i];
        if (player == NULL) continue;
        if (ServerNetwork_PlayerReadyForRemoval(player)) {
            ServerWorld_RemovePlayer(player);
            continue;
        }
        ServerPlayer_LoadChunks(player);
    }
}

void ServerPlayerManager_Shutdown(void) {
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        if (serverWorld.players[i] != NULL) ServerWorld_RemovePlayer(serverWorld.players[i]);
    }
}

void ServerWorld_AddPlayer(void *player) {
    Player *newPlayer = player;

    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        if (serverWorld.players[i] != NULL) continue;
        serverWorld.players[i] = newPlayer;
        newPlayer->id = i;
        ServerWorld_AddEntity(i, 1, 0, (Vector3){0, 80, 0});

        Entity localEntity = serverWorld.entities[i];
        localEntity.id = USHRT_MAX;
        ServerNetwork_Send(player, ServerPacket_CreateSpawnEntity(&localEntity));
        break;
    }

    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        if (serverWorld.players[i] == NULL || i == newPlayer->id) continue;
        ServerNetwork_Send(player, ServerPacket_CreateSpawnEntity(&serverWorld.entities[i]));
    }

    ServerWorld_SendMessage(TextFormat("%s joined the game!", newPlayer->name));
}

void ServerWorld_RemovePlayer(void *player) {
    Player *removedPlayer = player;
    ServerWorld_RemovePlayerFromChunks(removedPlayer);
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        if (serverWorld.players[i] != removedPlayer) continue;
        LuaBindings_InvokePlayerLeave(i);
        serverWorld.players[i] = NULL;
        ServerWorld_RemoveEntity(i);
        break;
    }
    ServerPlayer_Destroy(removedPlayer);
}
