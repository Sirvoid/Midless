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
#include "streamprofile.h"
#include "chunksave.h"
#include "platform.h"

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
    double generationSeconds;
} ChunkLoadResult;

static Vector3 *loadRequests;
static ChunkLoadResult *loadResults;
static pthread_mutex_t loaderMutex;
static pthread_cond_t loaderCondition;
#define CHUNK_LOAD_WORKERS 6
#define CHUNK_LOAD_RESULTS 32

typedef struct ChunkLoader {
    pthread_t thread;
    bool started, busy;
    Vector3 position;
} ChunkLoader;

static ChunkLoader loaders[CHUNK_LOAD_WORKERS];
static bool loaderRunning;
static bool loaderStarted;

static unsigned int unloadCursor;
static bool backgroundSaving;

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

// Called with loaderMutex held. Reserve room for jobs already being generated.
static bool ResultQueueFull(void) {
    int count = arrlen(loadResults);
    for (int i = 0; i < CHUNK_LOAD_WORKERS; i++) {
        if (loaders[i].busy) count++;
    }
    return count >= CHUNK_LOAD_RESULTS;
}

static void *ChunkLoaderRun(void *data) {
    ChunkLoader *loader = data;
    while (true) {
        pthread_mutex_lock(&loaderMutex);
        while (loaderRunning && (arrlen(loadRequests) == 0 || ResultQueueFull())) {
            pthread_cond_wait(&loaderCondition, &loaderMutex);
        }
        if (!loaderRunning) {
            pthread_mutex_unlock(&loaderMutex);
            return NULL;
        }
        Vector3 position = loadRequests[0];
        arrdel(loadRequests, 0);
        loader->busy = true;
        loader->position = position;
        pthread_mutex_unlock(&loaderMutex);

        double start = GetTime();
        Chunk *chunk = ServerChunk_Create(position);
        if (chunk != NULL) ServerChunk_Generate(chunk);
        ChunkLoadResult result = {.position = position, .chunk = chunk,
                                 .generationSeconds = GetTime() - start};

        pthread_mutex_lock(&loaderMutex);
        arrput(loadResults, result);
        loader->busy = false;
        pthread_mutex_unlock(&loaderMutex);
    }
}

static void ProcessLoadedChunks(void) {
    static StreamProfile profile;
    pthread_mutex_lock(&loaderMutex);
    ChunkLoadResult *results = NULL;
    int count = arrlen(loadResults);
    if (count > 8) count = 8;
    for (int i = 0; i < count; i++) arrput(results, loadResults[i]);
    if (count) arrdeln(loadResults, 0, count);
    pthread_cond_broadcast(&loaderCondition);
    pthread_mutex_unlock(&loaderMutex);

    for (int i = 0; i < arrlen(results); i++) {
        ChunkLoadResult *result = &results[i];
        StreamProfile_Add(&profile, "generation/load", result->generationSeconds);
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


    }
    arrfree(results);
}

void ServerChunkManager_Init(void) {
    serverWorld.chunks = NULL;
    unloadCursor = 0;
    backgroundSaving = ChunkSave_Init();
    if (!backgroundSaving) TraceLog(LOG_WARNING, "Disk worker unavailable; using synchronous chunk saves");
    serverWorld.pendingBlocks = NULL;
    serverWorld.generatedBlockUpdates = NULL;
    pthread_mutex_init(&loaderMutex, NULL);
    pthread_cond_init(&loaderCondition, NULL);
    loaderRunning = true;
    loaderStarted = false;
    int workerCount = Platform_ProcessorCount() - 2;
    if (workerCount < 1) workerCount = 1;
    if (workerCount > CHUNK_LOAD_WORKERS) workerCount = CHUNK_LOAD_WORKERS;
    for (int i = 0; i < CHUNK_LOAD_WORKERS; i++) {
        loaders[i] = (ChunkLoader){0};
        if (i >= workerCount) continue;
        loaders[i].started = pthread_create(&loaders[i].thread, NULL, ChunkLoaderRun, &loaders[i]) == 0;
        loaderStarted |= loaders[i].started;
    }
    if (!loaderStarted) loaderRunning = false;
}

static void FinishShutdownSave(Chunk *chunk, bool success, const BinaryWriter *snapshot) {
    chunk->savedOnShutdown = success;
}

void ServerChunkManager_Shutdown(void) {
    pthread_mutex_lock(&loaderMutex);
    loaderRunning = false;
    pthread_cond_broadcast(&loaderCondition);
    pthread_mutex_unlock(&loaderMutex);
    for (int i = 0; i < CHUNK_LOAD_WORKERS; i++)
        if (loaders[i].started) pthread_join(loaders[i].thread, NULL);
    // Complete older snapshots first, then capture the final stopped world.
    ChunkSave_Flush(NULL);
    if (backgroundSaving) {
        for (int i = 0; i < hmlen(serverWorld.chunks); i++) {
            Chunk *chunk = serverWorld.chunks[i].value;
            InventoryWindow_UnloadChunk(chunk->position);
            if (!ChunkSave_Queue(chunk)) {
                ChunkSave_Flush(FinishShutdownSave);
                // An oversized snapshot or allocation failure uses the synchronous fallback.
                ChunkSave_Queue(chunk);
            }
        }
        ChunkSave_Flush(FinishShutdownSave);
    }
    ChunkSave_Shutdown();

    for (int i = 0; i < arrlen(loadResults); i++) {
        ServerChunk_Destroy(loadResults[i].chunk);
    }
    arrfree(loadResults);
    arrfree(loadRequests);
    loadResults = NULL;
    loadRequests = NULL;
    loaderStarted = false;
    memset(loaders, 0, sizeof(loaders));
    pthread_cond_destroy(&loaderCondition);
    pthread_mutex_destroy(&loaderMutex);

    for (int i = hmlen(serverWorld.chunks) - 1; i >= 0; i--) {
        Chunk *chunk = serverWorld.chunks[i].value;
        long int key = ServerChunk_GetPackedPos(chunk->position);
        // Every chunk is leaving. Do not queue lighting work for its neighbors.
        InventoryWindow_UnloadChunk(chunk->position);
        if (!chunk->savedOnShutdown) EntityPersistence_Save(chunk);
        EntityPersistence_Unload(chunk);
        (void)hmdel(serverWorld.chunks, key);
        ServerChunk_Destroy(chunk);
    }
    hmfree(serverWorld.chunks);
    serverWorld.chunks = NULL;
    arrfree(serverWorld.pendingBlocks);
    serverWorld.pendingBlocks = NULL;
    arrfree(serverWorld.generatedBlockUpdates);
    serverWorld.generatedBlockUpdates = NULL;
}

void ServerChunkManager_CancelUnusedRequests(void) {
    pthread_mutex_lock(&loaderMutex);
    for (int i = arrlen(loadRequests) - 1; i >= 0; i--) {
        bool wanted = false;
        for (int p = 0; p < WORLD_MAX_PLAYERS; p++) {
            Player *player = serverWorld.players[p];
            if (player && !player->disconnected && PositionInLoadRadius(player, loadRequests[i])) {
                wanted = true;
                break;
            }
        }
        if (!wanted) arrdel(loadRequests, i);
    }
    pthread_mutex_unlock(&loaderMutex);
}

static bool ChunkWanted(Chunk *chunk) {
    if (arrlen(chunk->players)) return true;
    for (int i = 0; serverWorld.players && i < WORLD_MAX_PLAYERS; i++) {
        Player *player = serverWorld.players[i];
        if (player && !player->disconnected && PositionInLoadRadius(player, chunk->position)) return true;
    }
    return false;
}

static void FinishChunkSave(Chunk *chunk, bool success, const BinaryWriter *saved) {
    if (!success || ChunkWanted(chunk)) return;
    // The live chunk remains available during writing. Never unload edits or
    // entity changes that happened after the snapshot, even if a player left again.
    BinaryWriter current = {0};
    bool unchanged = EntityPersistence_Encode(chunk, &current) && current.size == saved->size &&
                     memcmp(current.data, saved->data, current.size) == 0;
    free(current.data);
    if (!unchanged) return;
    EntityPersistence_Unload(chunk);
    (void)hmdel(serverWorld.chunks, ServerChunk_GetPackedPos(chunk->position));
    ServerLighting_Removed(chunk->position);
    ServerChunk_Destroy(chunk);
}

void ServerChunkManager_Update(void) {
    ServerChunkManager_CancelUnusedRequests();
    ProcessLoadedChunks();
    FlushGeneratedBlockUpdates();
    bool playersConnected = false;
    for (int i = 0; serverWorld.players && i < WORLD_MAX_PLAYERS; i++) {
        Player *player = serverWorld.players[i];
        if (player && !player->disconnected) { playersConnected = true; break; }
    }
    double workSeconds = playersConnected ? 0.002 : 0.008;
    ChunkSave_PollUntil(FinishChunkSave, GetTime() + workSeconds);
    // Keep the bounded disk queue supplied instead of limiting throughput to
    // two chunks per server tick. Idle servers can spend more time draining it.
    double unloadDeadline = GetTime() + workSeconds;
    int checked = 0, removed = 0;
    int count = hmlen(serverWorld.chunks);
    while (checked < count && checked < 256 && removed < CHUNK_SAVE_JOBS) {
        int remaining = hmlen(serverWorld.chunks);
        if (!remaining) break;
        unloadCursor %= remaining;
        Chunk *chunk = serverWorld.chunks[unloadCursor++].value;
        checked++;
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
        if (arrlen(chunk->players) == 0) {
            bool wanted = false;
            for (int p = 0; p < WORLD_MAX_PLAYERS; p++) {
                Player *player = serverWorld.players[p];
                if (player && !player->disconnected && PositionInLoadRadius(player, chunk->position)) {
                    wanted = true;
                    break;
                }
            }
            if (!wanted) {
                if (chunk->savePending) continue;
                if (backgroundSaving) {
                    InventoryWindow_UnloadChunk(chunk->position);
                    if (!ChunkSave_Queue(chunk)) break;
                } else ServerWorld_RemoveChunk(chunk);
                removed++;
            }
        }
        if (GetTime() >= unloadDeadline) break;
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
    if (chunk->savePending) return;
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
    for (int i = 0; i < CHUNK_LOAD_WORKERS; i++) {
        if (loaders[i].busy && Vector3Equals(loaders[i].position, position)) {
            pthread_mutex_unlock(&loaderMutex);
            return true;
        }
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
    if (arrlen(loadRequests) >= 128) {
        pthread_mutex_unlock(&loaderMutex);
        return false;
    }
    arrput(loadRequests, position);
    pthread_cond_broadcast(&loaderCondition);
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
