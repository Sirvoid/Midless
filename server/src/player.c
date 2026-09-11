#include "lighting.h"
#include "blockstates.h"
/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include "raylib.h"
#include "raymath.h"
#include "player.h"
#include "world/world.h"
#include "world/chunk/chunk.h"
#include "networkhandler.h"
#include "packet.h"
#include "world/worldgen.h"
#include "stb_ds.h"

static uint64_t nextConnectionId;

bool ServerPlayer_FindSpawnPoint(Vector3 *position) {
    // Search actual chunks so saved terrain changes and generated structures count.
    int top = worldgen.maxY, bottom = worldgen.minY;
    for (int i = 0; i < worldgen.structureCount; i++)
        top = fmaxf(top, worldgen.maxY + 1 + worldgen.structures[i].maxDY);
    for (int i = 0; i < worldgen.featureCount; i++)
        top = fmaxf(top, worldgen.maxY + worldgen.features[i].paddingMax[1]);
    // Include player-built blocks above the generator's normal height range.
    for (int i = 0; i < hmlen(serverWorld.chunks); i++) {
        Chunk *chunk = serverWorld.chunks[i].value;
        if (chunk->position.x != 0 || chunk->position.z != 0) continue;
        top = fmaxf(top, chunk->blockPosition.y + CHUNK_SIZE_Y - 1);
        bottom = fminf(bottom, chunk->blockPosition.y);
    }
    FilePathList files = LoadDirectoryFiles("world");
    for (int i = 0; i < files.count; i++) {
        const char *name = GetFileName(files.paths[i]);
        int x, y, z, length = 0;
        if (sscanf(name, "%d.%d.%d.dat%n", &x, &y, &z, &length) != 3 ||
            !length || name[length] || x || z || y < -62500 || y > 62500) continue;
        top = fmaxf(top, y * CHUNK_SIZE_Y + CHUNK_SIZE_Y - 1);
        bottom = fminf(bottom, y * CHUNK_SIZE_Y);
    }
    UnloadDirectoryFiles(files);
    for (int y = top; y >= bottom; y--) {
        Vector3 cell = {0, y, 0};
        Vector3 chunkPosition = {0, floorf(y / (float)CHUNK_SIZE_Y), 0};
        if (!ServerWorld_RequestChunk(chunkPosition)) return false;
        BlockShape shape = ServerBlockStates_Shape(ServerWorld_GetBlock(cell), cell);
        float height = -INFINITY;
        if (shape.liquid) height = shape.bounds.max.y;
        if (shape.solid) for (int i = 0; i < shape.collisionCount; i++) {
            BoundingBox box = shape.collision[i];
            if (box.min.x <= 0.5f && box.max.x >= 0.5f && box.min.z <= 0.5f && box.max.z >= 0.5f)
                height = fmaxf(height, box.max.y);
        }
        if (isfinite(height)) {
            *position = (Vector3){0.5f, height + 1.0f / 32, 0.5f};
            return true;
        }
    }
    TraceLog(LOG_ERROR, "Cannot find a spawn surface at (0, 0)");
    return false;
}

Player *ServerPlayer_Create(void *peer, bool isWeb) {
    Player *player = MemAlloc(sizeof(*player));
    if (player == NULL) return NULL;

    *player = (Player){0};
    player->spawnPoint = (Vector3){0, 80, 0};
    Inventory_Init(&player->inventory);
    player->entityId = -1;
    player->connectionId = ++nextConnectionId;
    player->peer = peer;
    player->drawDistance = 3;
    player->isWeb = isWeb;
    return player;
}

void ServerPlayer_Destroy(Player *player) {
    if (player == NULL) return;
    for (int i=0;i<player->metadataCount;i++) Metadata_Free(&player->metadata[i].value);
    MemFree(player->name);
    MemFree(player);
}

void ServerPlayer_DefineBlock(Player *player, int id, const BlockDefinition *definition) {
    if (!player || player->disconnected) return;
    for(int state=0;state<ServerBlockStates_Count(id);state++) {
        const BlockDefinition *d=state?ServerBlockStates_Definition(id,state):definition;
        unsigned char *packet = ServerPacket_CreateDefineBlock(id | (state<<8), d);
        if(packet) ServerNetwork_Send(player, packet);
    }
}

void ServerPlayer_RemoveBlockDefinition(Player *player, int id) {
    if (!player || player->disconnected) return;
    unsigned char *packet = ServerPacket_CreateRemoveBlockDefinition(id);
    if (!packet) return;
    ServerNetwork_Send(player, packet);
}

void ServerPlayer_Teleport(Player *player, Vector3 position) {
    if (player->entityId < 0) return;
    Entity *entity = &serverWorld.entities[player->entityId];
    ServerWorld_TeleportEntity(player->entityId, position, entity->rotation);
    ServerPlayer_ResetMovement(player);
    Entity localEntity = *entity;
    localEntity.id = USHRT_MAX;
    Vector3 localPosition = {position.x - 0.5f, position.y, position.z - 0.5f};
    ServerNetwork_Send(player, ServerPacket_CreateTeleportEntity(&localEntity, localPosition, entity->rotation));
}

void ServerPlayer_LoadChunks(Player* player) {

    Entity entity = serverWorld.entities[player->entityId];
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
        ServerLighting_Send(chunk,player);
        MemFree(compressedChunk);

        if (GetTime() >= loadDeadline) return;
    }
}

void ServerPlayer_SendMessage(Player *player, const char *message) {
    if (!player || player->disconnected) return;
    int length = TextLength(message);
    int parts = length > 0 ? (length + 63) / 64 : 1;
    for (int i = 0; i < parts; i++) {
        const char *part = TextSubtext(message, i * 64, 64);
        ServerNetwork_Send(player, i == 0 ? ServerPacket_CreateMessage(part)
                                         : ServerPacket_CreateMessageContinuation(part));
    }
}
