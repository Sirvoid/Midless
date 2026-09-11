#include "../serverinventory.h"
#include "../lighting.h"
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#define __clang__ true
#include "raylib.h"
#include "raymath.h"
#include "stb_ds.h"
#include "chunkmanager.h"
#include "world.h"
#include "entitypersistence.h"
#include "chunk/chunk.h"
#include "../networkhandler.h"
#include "../packet.h"
#include "../scripting/luabindings.h"

typedef struct PendingWorldBlock {
    Vector3 position;
    unsigned short blockId;
} PendingWorldBlock;

typedef struct GeneratedBlockUpdate {
    Chunk *chunk;
    ServerBlockUpdate update;
} GeneratedBlockUpdate;

typedef struct ChunkLoadResult {
    Vector3 position;
    Chunk *chunk;
} ChunkLoadResult;

static Vector3 *loadRequests;
static ChunkLoadResult *loadResults;
static pthread_mutex_t loaderMutex;
static pthread_cond_t loaderCondition;
static pthread_t loaderThread;
static bool loaderRunning;
static bool loaderStarted;
static bool loaderBusy;
static Vector3 loaderPosition;

static bool PositionInLoadRadius(Player *player, Vector3 chunkPosition) {
    Entity entity = serverWorld.entities[player->entityId];
    Vector3 playerChunkPosition = {
        floorf(entity.position.x / CHUNK_SIZE_X),
        floorf(entity.position.y / CHUNK_SIZE_Y),
        floorf(entity.position.z / CHUNK_SIZE_Z)
    };
    Vector3 offset = Vector3Subtract(chunkPosition, playerChunkPosition);
    int loadingHeight = fmin(player->drawDistance, 4);
    float loadingRadius = player->drawDistance + 3;
    return fabsf(offset.y) <= loadingHeight &&
        Vector3LengthSqr(offset) < loadingRadius * loadingRadius;
}

static void WriteGeneratedBlock(Chunk *chunk, Vector3 blockPosition, int blockId) {
    Vector3 localPosition = {
        floorf(blockPosition.x) - chunk->blockPosition.x,
        floorf(blockPosition.y) - chunk->blockPosition.y,
        floorf(blockPosition.z) - chunk->blockPosition.z
    };
    if (!ServerChunk_IsValidPos(localPosition)) return;
    ServerChunk_SetBlock(chunk, localPosition, blockId);

    ServerLighting_Changed(chunk);
    if (arrlen(chunk->players) > 0) {
        arrput(serverWorld.generatedBlockUpdates, ((GeneratedBlockUpdate){
            .chunk = chunk,
            .update = {.position = blockPosition, .blockId = (unsigned char)blockId}
        }));
    }
}

static bool ApplyPendingBlocks(Chunk *chunk) {
    bool applied = false;
    for (int i = 0; i < arrlen(serverWorld.pendingBlocks);) {
        PendingWorldBlock pending = serverWorld.pendingBlocks[i];
        Vector3 pendingChunkPosition = {
            floorf(pending.position.x / CHUNK_SIZE_X),
            floorf(pending.position.y / CHUNK_SIZE_Y),
            floorf(pending.position.z / CHUNK_SIZE_Z)
        };
        if (!Vector3Equals(pendingChunkPosition, chunk->position)) {
            i++;
            continue;
        }
        WriteGeneratedBlock(chunk, pending.position, pending.blockId);
        arrdel(serverWorld.pendingBlocks, i);
        applied = true;
    }
    return applied;
}

static void FlushGeneratedBlockUpdates(void) {
    while (arrlen(serverWorld.generatedBlockUpdates) > 0) {
        Chunk *chunk = serverWorld.generatedBlockUpdates[0].chunk;
        ServerBlockUpdate *updates = NULL;
        GeneratedBlockUpdate *remaining = NULL;
        for (int i = 0; i < arrlen(serverWorld.generatedBlockUpdates); i++) {
            if (serverWorld.generatedBlockUpdates[i].chunk != chunk) {
                arrput(remaining, serverWorld.generatedBlockUpdates[i]);
            } else {
                arrput(updates, serverWorld.generatedBlockUpdates[i].update);
            }
        }
        arrfree(serverWorld.generatedBlockUpdates);
        serverWorld.generatedBlockUpdates = remaining;
        for (int i = 0; i < arrlen(chunk->players); i++) {
            ServerNetwork_Send(chunk->players[i], ServerPacket_CreateBlockBatch(
                updates, (unsigned short)arrlen(updates)));
        }
        arrfree(updates);
    }
}

static void *ChunkLoaderRun(void *unused) {
    (void)unused;
    while (true) {
        pthread_mutex_lock(&loaderMutex);
        while (loaderRunning && arrlen(loadRequests) == 0) {
            pthread_cond_wait(&loaderCondition, &loaderMutex);
        }
        if (!loaderRunning) {
            pthread_mutex_unlock(&loaderMutex);
            return NULL;
        }
        Vector3 position = loadRequests[0];
        arrdel(loadRequests, 0);
        loaderBusy = true;
        loaderPosition = position;
        pthread_mutex_unlock(&loaderMutex);

        Chunk *chunk = ServerChunk_Create(position);
        if (chunk != NULL) ServerChunk_Generate(chunk);
        ChunkLoadResult result = {.position = position, .chunk = chunk};

        pthread_mutex_lock(&loaderMutex);
        arrput(loadResults, result);
        loaderBusy = false;
        pthread_mutex_unlock(&loaderMutex);
    }
}

static void ProcessLoadedChunks(void) {
    pthread_mutex_lock(&loaderMutex);
    ChunkLoadResult *results = loadResults;
    loadResults = NULL;
    pthread_mutex_unlock(&loaderMutex);

    for (int i = 0; i < arrlen(results); i++) {
        ChunkLoadResult *result = &results[i];
        Chunk *chunk = result->chunk;
        if (chunk != NULL && ServerWorld_GetChunkAt(result->position) == NULL) {
            hmput(serverWorld.chunks, ServerChunk_GetPackedPos(result->position), chunk);
            if (!EntityPersistence_Activate(chunk)) {
                TraceLog(LOG_ERROR, "Chunk entities could not be restored; leaving save untouched");
                (void)hmdel(serverWorld.chunks, ServerChunk_GetPackedPos(result->position));
                ServerChunk_Destroy(chunk);
                chunk = NULL;
            }
            if (chunk) { ApplyPendingBlocks(chunk); ServerLighting_Changed(chunk); }
        } else {
            ServerChunk_Destroy(chunk);
            chunk = ServerWorld_GetChunkAt(result->position);
        }

        unsigned short *compressedData = NULL;
        int compressedLength = 0;
        for (int playerIndex = 0; playerIndex < WORLD_MAX_PLAYERS; playerIndex++) {
            Player *player = serverWorld.players[playerIndex];
            if (player == NULL) continue;
            if (player->chunkRequestPending &&
                Vector3Equals(player->pendingChunkPosition, result->position)) {
                player->chunkRequestPending = false;
            }
            if (chunk == NULL || !PositionInLoadRadius(player, result->position) ||
                ServerChunk_PlayerInChunk(chunk, player)) continue;

            if (!compressedData) compressedData = ServerChunk_CreateCompressedData(chunk, &compressedLength);
            if (!compressedData) continue;
            ServerChunk_AddPlayer(chunk, player);
            ServerNetwork_Send(player, ServerPacket_CreateLoadChunk(
                compressedData, compressedLength, chunk->position, chunk->skyMask));
            ServerLighting_Send(chunk,player);
        }
        MemFree(compressedData);
    }
    arrfree(results);
}

void ServerChunkManager_Init(void) {
    serverWorld.chunks = NULL;
    serverWorld.pendingBlocks = NULL;
    serverWorld.generatedBlockUpdates = NULL;
    pthread_mutex_init(&loaderMutex, NULL);
    pthread_cond_init(&loaderCondition, NULL);
    loaderRunning = true;
    loaderStarted = pthread_create(&loaderThread, NULL, ChunkLoaderRun, NULL) == 0;
    if (!loaderStarted) loaderRunning = false;
}

void ServerChunkManager_Shutdown(void) {
    pthread_mutex_lock(&loaderMutex);
    loaderRunning = false;
    pthread_cond_signal(&loaderCondition);
    pthread_mutex_unlock(&loaderMutex);
    if (loaderStarted) pthread_join(loaderThread, NULL);

    for (int i = 0; i < arrlen(loadResults); i++) {
        ServerChunk_Destroy(loadResults[i].chunk);
    }
    arrfree(loadResults);
    arrfree(loadRequests);
    loadResults = NULL;
    loadRequests = NULL;
    loaderStarted = false;
    loaderBusy = false;
    pthread_cond_destroy(&loaderCondition);
    pthread_mutex_destroy(&loaderMutex);

    for (int i = hmlen(serverWorld.chunks) - 1; i >= 0; i--) {
        Chunk *chunk = serverWorld.chunks[i].value;
        long int key = ServerChunk_GetPackedPos(chunk->position);
        ServerWorld_RemoveChunk(chunk);
        // A failed write retains the chunk during play. Shutdown still releases
        // its memory after the save error has been reported.
        if (hmgeti(serverWorld.chunks, key) >= 0) {
            (void)hmdel(serverWorld.chunks, key);
            ServerChunk_Destroy(chunk);
        }
    }
    hmfree(serverWorld.chunks);
    serverWorld.chunks = NULL;
    arrfree(serverWorld.pendingBlocks);
    serverWorld.pendingBlocks = NULL;
    arrfree(serverWorld.generatedBlockUpdates);
    serverWorld.generatedBlockUpdates = NULL;
}

void ServerChunkManager_Update(void) {
    ProcessLoadedChunks();
    FlushGeneratedBlockUpdates();
    for (int i = 0; i < hmlen(serverWorld.chunks); i++) {
        Chunk *chunk = serverWorld.chunks[i].value;
        for (int j = arrlen(chunk->players) - 1; j >= 0; j--) {
            Player *player = chunk->players[j];
            Entity entity = serverWorld.entities[player->entityId];
            Vector3 playerChunkPosition = {
                floorf(entity.position.x / CHUNK_SIZE_X),
                floorf(entity.position.y / CHUNK_SIZE_Y),
                floorf(entity.position.z / CHUNK_SIZE_Z)
            };
            if (Vector3Distance(chunk->position, playerChunkPosition) >= player->drawDistance + 3) {
                ServerNetwork_Send(player, ServerPacket_CreateUnloadChunk(chunk->position));
                ServerChunk_RemovePlayer(chunk, j);
            }
        }
        if (arrlen(chunk->players) == 0) ServerWorld_RemoveChunk(chunk);
    }
}

void ServerWorld_RemovePlayerFromChunks(Player *playerToRemove) {
    for (int i = 0; i < hmlen(serverWorld.chunks); i++) {
        Chunk *chunk = serverWorld.chunks[i].value;
        for (int j = arrlen(chunk->players) - 1; j >= 0; j--) {
            if (chunk->players[j] == playerToRemove) ServerChunk_RemovePlayer(chunk, j);
        }
    }
}

Chunk *ServerWorld_AddChunk(Vector3 position) {
    long int packedPosition = ServerChunk_GetPackedPos(position);
    int index = hmgeti(serverWorld.chunks, packedPosition);
    if (index >= 0) return serverWorld.chunks[index].value;

    Chunk *chunk = ServerChunk_Create(position);
    if (chunk == NULL) return NULL;
    hmput(serverWorld.chunks, packedPosition, chunk);
    ServerChunk_Generate(chunk);
    if (!EntityPersistence_Activate(chunk)) {
        (void)hmdel(serverWorld.chunks, packedPosition);
        ServerChunk_Destroy(chunk);
        TraceLog(LOG_ERROR, "Chunk entities could not be restored; leaving save untouched");
        return NULL;
    }
    ApplyPendingBlocks(chunk);
    ServerLighting_Changed(chunk);
    return chunk;
}

void ServerWorld_RemoveChunk(Chunk *chunk) {
    long int packedPosition = ServerChunk_GetPackedPos(chunk->position);
    if (ServerWorld_GetChunkAt(chunk->position) != chunk) return;
    // Closing may create a dropped cursor stack; include it in this save.
    InventoryWindow_UnloadChunk(chunk->position);
    if (!EntityPersistence_Save(chunk)) return;
    EntityPersistence_Unload(chunk);
    (void)hmdel(serverWorld.chunks, packedPosition);
    ServerLighting_Removed(chunk->position);
    ServerChunk_Destroy(chunk);
}

Chunk *ServerWorld_GetChunkAt(Vector3 position) {
    int index = hmgeti(serverWorld.chunks, ServerChunk_GetPackedPos(position));
    return index >= 0 ? serverWorld.chunks[index].value : NULL;
}

Chunk *ServerWorld_RequestChunk(Vector3 position) {
    return ServerWorld_AddChunk(position);
}

bool ServerWorld_QueueChunk(Vector3 position) {
    if (!loaderStarted) return false;
    if (ServerWorld_GetChunkAt(position) != NULL) return true;
    pthread_mutex_lock(&loaderMutex);
    if (loaderBusy && Vector3Equals(loaderPosition, position)) {
        pthread_mutex_unlock(&loaderMutex);
        return true;
    }
    for (int i = 0; i < arrlen(loadRequests); i++) {
        if (Vector3Equals(loadRequests[i], position)) {
            pthread_mutex_unlock(&loaderMutex);
            return true;
        }
    }
    for (int i = 0; i < arrlen(loadResults); i++) {
        if (Vector3Equals(loadResults[i].position, position)) {
            pthread_mutex_unlock(&loaderMutex);
            return true;
        }
    }
    arrput(loadRequests, position);
    pthread_cond_signal(&loaderCondition);
    pthread_mutex_unlock(&loaderMutex);
    return true;
}

int ServerWorld_GetBlock(Vector3 blockPosition) {
    Vector3 chunkPosition = {
        floorf(blockPosition.x / CHUNK_SIZE_X),
        floorf(blockPosition.y / CHUNK_SIZE_Y),
        floorf(blockPosition.z / CHUNK_SIZE_Z)
    };
    Chunk *chunk = ServerWorld_GetChunkAt(chunkPosition);
    if (chunk == NULL) return 0;
    Vector3 localPosition = {
        floorf(blockPosition.x) - chunk->blockPosition.x,
        floorf(blockPosition.y) - chunk->blockPosition.y,
        floorf(blockPosition.z) - chunk->blockPosition.z
    };
    return ServerChunk_GetBlock(chunk, localPosition);
}

void ServerWorld_SetBlockFast(Vector3 blockPosition, int blockId) {
    Vector3 chunkPosition = {
        floorf(blockPosition.x / CHUNK_SIZE_X),
        floorf(blockPosition.y / CHUNK_SIZE_Y),
        floorf(blockPosition.z / CHUNK_SIZE_Z)
    };
    Chunk *chunk = ServerWorld_GetChunkAt(chunkPosition);
    if (chunk != NULL) {
        WriteGeneratedBlock(chunk, blockPosition, blockId);
        return;
    }
    arrput(serverWorld.pendingBlocks, ((PendingWorldBlock){
        .position = {floorf(blockPosition.x), floorf(blockPosition.y), floorf(blockPosition.z)},
        .blockId = (unsigned short)blockId
    }));
}

void ServerWorld_SetBlock(Vector3 blockPosition, int blockId, bool broadcast, bool byPlayer, bool callCallbacks) {
    Vector3 chunkPosition = {
        floorf(blockPosition.x / CHUNK_SIZE_X),
        floorf(blockPosition.y / CHUNK_SIZE_Y),
        floorf(blockPosition.z / CHUNK_SIZE_Z)
    };
    Chunk *chunk = ServerWorld_GetChunkAt(chunkPosition);
    if (chunk == NULL) return;
    Vector3 localPosition = {
        floorf(blockPosition.x) - chunkPosition.x * CHUNK_SIZE_X,
        floorf(blockPosition.y) - chunkPosition.y * CHUNK_SIZE_Y,
        floorf(blockPosition.z) - chunkPosition.z * CHUNK_SIZE_Z
    };
    int previousBlock = ServerChunk_GetBlock(chunk, localPosition);
    ServerInventory_InvalidateDig(blockPosition);
    if (previousBlock == blockId) return;
    InventoryWindow_Invalidate((Vector3){floorf(blockPosition.x), floorf(blockPosition.y), floorf(blockPosition.z)});
    ServerChunk_SetBlock(chunk, localPosition, blockId);
    ServerLighting_Changed(chunk);
    if (broadcast) ServerWorld_Broadcast(ServerPacket_CreateSetBlock(blockId, blockPosition, byPlayer));
    if (callCallbacks) LuaBindings_InvokeBlockUpdate(blockPosition, blockId, previousBlock);
}
