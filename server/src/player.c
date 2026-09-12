#include "lighting.h"
#include "world/chunkmanager.h"
#include "blockstates.h"
/**
 * Copyright (c) 2022 Sirvoid
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
    arrfree(player->chunkRequests);
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
    // Detach the old view immediately; saving its chunks remains budgeted.
    ServerWorld_RemovePlayerFromChunks(player);
    player->chunkRequestDistance = -1;
    player->pendingChunkCount = 0;
    player->lightingChunkCount = 0;
    player->chunkRetryTime = 0;
    ServerChunkManager_CancelUnusedRequests();
    unsigned char *reset = MemAlloc(RESET_CHUNKS_PACKET_SIZE);
    if (reset) { reset[0] = PACKET_RESET_CHUNKS; ServerNetwork_Send(player, reset); }
    ServerPlayer_LoadChunks(player);
    Entity localEntity = *entity;
    localEntity.id = USHRT_MAX;
    Vector3 localPosition = {position.x - 0.5f, position.y, position.z - 0.5f};
    ServerNetwork_Send(player, ServerPacket_CreateTeleportEntity(&localEntity, localPosition, entity->rotation));
}

static int CompareChunkRequests(const void *a, const void *b) {
    const ChunkRequest *first = a, *second = b;
    return (first->distanceSquared > second->distanceSquared) -
           (first->distanceSquared < second->distanceSquared);
}

static void PrepareChunkRequests(Player *player, Vector3 center) {
    arrsetlen(player->chunkRequests, 0);
    player->chunkRequestCursor = 0;
    player->pendingChunkCount = 0;
    player->lightingChunkCount = 0;
    player->chunkRequestCenter = center;
    player->chunkRequestDistance = player->drawDistance;
    int height = player->drawDistance < 4 ? player->drawDistance : 4;
    int radius = player->drawDistance + 3;
    for (int y = -height; y <= height; y++) {
        for (int x = -player->drawDistance; x <= player->drawDistance; x++) {
            for (int z = -player->drawDistance; z <= player->drawDistance; z++) {
                int distance = x*x + y*y + z*z;
                if (distance >= radius*radius) continue;
                Vector3 position = {center.x + x, center.y + y, center.z + z};
                Chunk *chunk = ServerWorld_GetChunkAt(position);
                if (chunk && ServerChunk_PlayerInChunk(chunk, player)) continue;
                arrput(player->chunkRequests, ((ChunkRequest){position, distance}));
            }
        }
    }
    if (arrlen(player->chunkRequests) > 1)
        qsort(player->chunkRequests, arrlen(player->chunkRequests), sizeof(ChunkRequest), CompareChunkRequests);
}

void ServerPlayer_LoadChunks(Player *player) {
    Vector3 position = serverWorld.entities[player->entityId].position;
    Vector3 center = {floorf(position.x / 16), floorf(position.y / 16), floorf(position.z / 16)};
    if (player->chunkRequestDistance != player->drawDistance ||
        !Vector3Equals(center, player->chunkRequestCenter)) PrepareChunkRequests(player, center);

    double deadline = GetTime() + 0.002;
    // Generated chunks move to a separate lighting queue, freeing loader slots.
    for (int i = 0; i < player->pendingChunkCount;) {
        Chunk *chunk = ServerWorld_GetChunkAt(player->pendingChunks[i]);
        if (!chunk) {
            if (GetTime() >= player->chunkRetryTime) ServerWorld_QueueChunk(player->pendingChunks[i]);
            i++;
            continue;
        }
        if (player->lightingChunkCount == PLAYER_LIGHT_REQUESTS) break;
        ServerLighting_Prioritize(chunk);
        player->lightingChunks[player->lightingChunkCount++] = player->pendingChunks[i];
        player->pendingChunkCount--;
        memmove(player->pendingChunks + i, player->pendingChunks + i + 1,
            (player->pendingChunkCount - i) * sizeof(Vector3));
    }
    for (int i = 0; i < player->lightingChunkCount;) {
        Chunk *chunk = ServerWorld_GetChunkAt(player->lightingChunks[i]);
        if (!chunk) {
            if (GetTime() >= player->chunkRetryTime) ServerWorld_QueueChunk(player->lightingChunks[i]);
            i++;
            continue;
        }
        if (!ServerLighting_IsReady(chunk)) { i++; continue; }
        if (!ServerChunk_PlayerInChunk(chunk, player)) {
            int length = 0;
            unsigned short *data = ServerChunk_CreateCompressedData(chunk, &length);
            if (!data) { i++; continue; }
            ServerChunk_AddPlayer(chunk, player);
            ServerNetwork_Send(player, ServerPacket_CreateLoadChunk(data, length, chunk->position, chunk->skyMask));
            ServerLighting_Send(chunk, player);
            MemFree(data);
        }
        player->lightingChunkCount--;
        memmove(player->lightingChunks + i, player->lightingChunks + i + 1,
            (player->lightingChunkCount - i) * sizeof(Vector3));
        if (GetTime() >= deadline) break;
    }
    if (GetTime() >= player->chunkRetryTime) player->chunkRetryTime = GetTime() + 2.0;

    while (player->lightingChunkCount < PLAYER_LIGHT_REQUESTS &&
           player->pendingChunkCount < PLAYER_CHUNK_REQUESTS &&
           player->chunkRequestCursor < arrlen(player->chunkRequests) && GetTime() < deadline) {
        Vector3 next = player->chunkRequests[player->chunkRequestCursor].position;
        Chunk *chunk = ServerWorld_GetChunkAt(next);
        if (chunk && ServerChunk_PlayerInChunk(chunk, player)) { player->chunkRequestCursor++; continue; }
        if (!ServerWorld_QueueChunk(next)) break;
        player->pendingChunks[player->pendingChunkCount++] = next;
        player->chunkRequestCursor++;
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
