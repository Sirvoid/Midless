/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "player.h"
#include "world/world.h"
#include "world/chunk/chunk.h"
#include "networkhandler.h"
#include "packet.h"

Player *ServerPlayer_Create(void *peer, bool isWeb) {
    Player *player = MemAlloc(sizeof(*player));
    if (player == NULL) return NULL;

    *player = (Player){0};
    player->peer = peer;
    player->drawDistance = 3;
    player->isWeb = isWeb;
    return player;
}

void ServerPlayer_Destroy(Player *player) {
    if (player == NULL) return;
    MemFree(player->name);
    MemFree(player);
}

void ServerPlayer_DefineBlock(Player *player, int id, const BlockDefinition *definition) {
    if (!player || player->disconnected) return;
    unsigned char *packet = ServerPacket_CreateDefineBlock(id, definition);
    if (!packet) return;
    ServerNetwork_Send(player, packet);
}

void ServerPlayer_RemoveBlockDefinition(Player *player, int id) {
    if (!player || player->disconnected) return;
    unsigned char *packet = ServerPacket_CreateRemoveBlockDefinition(id);
    if (!packet) return;
    ServerNetwork_Send(player, packet);
}

void ServerPlayer_UpdatePositionRotation(Player* player, Vector3 position, Vector3 rotation) {
    ServerWorld_TeleportEntity(player->id, position, rotation);
}

void ServerPlayer_LoadChunks(Player* player) {

    Entity entity = serverWorld.entities[player->id];
    double loadDeadline = GetTime() + 0.008;

    if (player->chunkRequestPending) return;

    Vector3 playerChunkPos = (Vector3) {(int)floor(entity.position.x / CHUNK_SIZE_X), (int)floor(entity.position.y / CHUNK_SIZE_Y), (int)floor(entity.position.z / CHUNK_SIZE_Z)};

    int loadingHeight = fmin(player->drawDistance, 4);
    while (true) {
        bool foundChunk = false;
        float closestDistanceSquared = INFINITY;
        Vector3 closestPosition = {0};

        for (int y = -loadingHeight; y <= loadingHeight; y++) {
            for (int x = -player->drawDistance; x <= player->drawDistance; x++) {
                for (int z = -player->drawDistance; z <= player->drawDistance; z++) {
                    float distanceSquared = (float)(x*x + y*y + z*z);
                    float loadingRadius = player->drawDistance + 3;
                    if (distanceSquared >= loadingRadius * loadingRadius ||
                        distanceSquared >= closestDistanceSquared) continue;

                    Vector3 chunkPos = {
                        playerChunkPos.x + x,
                        playerChunkPos.y + y,
                        playerChunkPos.z + z
                    };
                    Chunk *chunk = ServerWorld_GetChunkAt(chunkPos);
                    if (chunk != NULL && ServerChunk_PlayerInChunk(chunk, player)) continue;

                    foundChunk = true;
                    closestDistanceSquared = distanceSquared;
                    closestPosition = chunkPos;
                }
            }
        }

        if (!foundChunk) return;

        Chunk *chunk = ServerWorld_GetChunkAt(closestPosition);
        if (chunk == NULL) {
            if (ServerWorld_QueueChunk(closestPosition)) {
                player->chunkRequestPending = true;
                player->pendingChunkPosition = closestPosition;
            }
            return;
        }
        ServerChunk_AddPlayer(chunk, player);

        int compressedLength = 0;
        unsigned short *compressedChunk = ServerChunk_CreateCompressedData(chunk, &compressedLength);
        ServerNetwork_Send(player, ServerPacket_CreateLoadChunk(
            compressedChunk, compressedLength, closestPosition, chunk->skyMask));
        MemFree(compressedChunk);

        if (GetTime() >= loadDeadline) return;
    }
}
