/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <limits.h>
#include <stddef.h>
#include <string.h>
#include "world.h"
#include "../player.h"
#include "../droppeditems.h"
#include "../serverinventory.h"
#include "../networkhandler.h"
#include "../packet.h"
#include "scripthooks.h"
#include "entityregistry.h"

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

    for (int i = 0; i < serverWorld.maxPlayers; i++) {
        if (serverWorld.players[i] != NULL) continue;
        Vector3 position = newPlayer->hasSavedPosition ? newPlayer->savedPosition : newPlayer->spawnPoint;
        int entityId = ServerWorld_AddEntity(1, 0, position, i);
        if (entityId < 0) return;
        newPlayer->entityId = entityId;
        strcpy(serverWorld.entities[entityId].texture,newPlayer->texture);
        TextColor_Escape(serverWorld.entities[entityId].nametag.text, newPlayer->name);
        serverWorld.players[i] = newPlayer;
        newPlayer->id = i;
        ServerPlayer_ResetMovement(newPlayer);

        Entity localEntity = serverWorld.entities[newPlayer->entityId];
        localEntity.id = USHRT_MAX;
        ServerNetwork_Send(player, ServerPacket_CreateSpawnEntity(&localEntity));
        break;
    }

    if (newPlayer->entityId < 0) return;
    ServerEntities_Send(newPlayer);

    char name[PACKET_STRING_SIZE * 2 + 1];
    TextColor_Escape(name, newPlayer->name);
    ServerWorld_SendMessage(TextFormat("%s joined the game!", name));
}

void ServerWorld_RemovePlayer(void *player) {
    Player *removedPlayer = player;
    ServerWorld_RemovePlayerFromChunks(removedPlayer);
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        if (serverWorld.players[i] != removedPlayer) continue;
        if (!removedPlayer->leaveInvoked) {
            ScriptHooks_PlayerLeave(i);
            removedPlayer->leaveInvoked = true;
        }
        // Keep the live inventory for retry if the save fails.
        if (!ServerInventory_Save(removedPlayer)) return;
        ServerDrops_ForgetPlayer(i);
        serverWorld.players[i] = NULL;
        ServerWorld_RemoveEntity(removedPlayer->entityId);
        break;
    }
    ServerPlayer_Destroy(removedPlayer);
}
