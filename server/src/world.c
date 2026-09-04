/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#define __clang__ true
#if !defined(MIDLESS_STB_DS_EXTERNAL)
    #define STB_DS_IMPLEMENTATION
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>
#include <limits.h>
#include <pthread.h>
#include "raylib.h"
#include "raymath.h"
#include "stb_ds.h"
#include "world.h"
#include "chunk/chunk.h"
#include "networkhandler.h"
#include "packet.h"
#include "entity.h"
#include "worldgenerator.h"
#include "luabindings.h"
#include "utils.h"

typedef struct PendingWorldBlock {
    Vector3 position;
    unsigned short blockId;
} PendingWorldBlock;

typedef struct GeneratedBlockUpdate {
    Chunk *chunk;
    ServerBlockUpdate update;
} GeneratedBlockUpdate;

World serverWorld;
static long long serverWorldLastUpdateMilliseconds;
static long long serverWorldLastTimeSyncMilliseconds;

typedef struct ChunkLoadResult {
    Vector3 position;
    Chunk *chunk;
    unsigned short *compressedData;
    int compressedLength;
} ChunkLoadResult;

static Vector3 *serverChunkLoadRequests;
static ChunkLoadResult *serverChunkLoadResults;
static pthread_mutex_t serverChunkLoaderMutex;
static pthread_cond_t serverChunkLoaderCondition;
static pthread_t serverChunkLoaderThread;
static bool serverChunkLoaderRunning;
static bool serverChunkLoaderStarted;
static bool serverChunkLoaderBusy;
static Vector3 serverChunkLoaderPosition;

static bool ServerWorld_PositionInLoadRadius(Player *player, Vector3 chunkPosition) {
    Entity entity = serverWorld.entities[player->id];
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

static void *ServerWorld_ChunkLoaderRun(void *unused) {
    (void)unused;
    while (true) {
        pthread_mutex_lock(&serverChunkLoaderMutex);
        while (serverChunkLoaderRunning && arrlen(serverChunkLoadRequests) == 0) {
            pthread_cond_wait(&serverChunkLoaderCondition, &serverChunkLoaderMutex);
        }
        if (!serverChunkLoaderRunning) {
            pthread_mutex_unlock(&serverChunkLoaderMutex);
            return NULL;
        }
        Vector3 position = serverChunkLoadRequests[0];
        arrdel(serverChunkLoadRequests, 0);
        serverChunkLoaderBusy = true;
        serverChunkLoaderPosition = position;
        pthread_mutex_unlock(&serverChunkLoaderMutex);

        Chunk *chunk = ServerChunk_Create(position);
        if (chunk != NULL) ServerChunk_Generate(chunk);

        ChunkLoadResult result = {.position = position, .chunk = chunk};
        if (chunk != NULL) {
            result.compressedData = ServerChunk_CreateCompressedData(
                chunk, &result.compressedLength);
        }

        pthread_mutex_lock(&serverChunkLoaderMutex);
        arrput(serverChunkLoadResults, result);
        serverChunkLoaderBusy = false;
        pthread_mutex_unlock(&serverChunkLoaderMutex);
    }
}

static void ServerWorld_StartChunkLoader(void) {
    pthread_mutex_init(&serverChunkLoaderMutex, NULL);
    pthread_cond_init(&serverChunkLoaderCondition, NULL);
    serverChunkLoaderRunning = true;
    serverChunkLoaderStarted = pthread_create(
        &serverChunkLoaderThread, NULL, ServerWorld_ChunkLoaderRun, NULL) == 0;
    if (!serverChunkLoaderStarted) serverChunkLoaderRunning = false;
}

static void ServerWorld_StopChunkLoader(void) {
    pthread_mutex_lock(&serverChunkLoaderMutex);
    serverChunkLoaderRunning = false;
    pthread_cond_signal(&serverChunkLoaderCondition);
    pthread_mutex_unlock(&serverChunkLoaderMutex);
    if (serverChunkLoaderStarted) pthread_join(serverChunkLoaderThread, NULL);

    for (int i = 0; i < arrlen(serverChunkLoadResults); i++) {
        ServerChunk_Destroy(serverChunkLoadResults[i].chunk);
        MemFree(serverChunkLoadResults[i].compressedData);
    }
    arrfree(serverChunkLoadResults);
    arrfree(serverChunkLoadRequests);
    serverChunkLoadResults = NULL;
    serverChunkLoadRequests = NULL;
    serverChunkLoaderStarted = false;
    serverChunkLoaderBusy = false;
    pthread_cond_destroy(&serverChunkLoaderCondition);
    pthread_mutex_destroy(&serverChunkLoaderMutex);
}

static void ServerWorld_WriteGeneratedBlock(Chunk *chunk, Vector3 blockPos, int blockId) {
    Vector3 localPos = {
        floor(blockPos.x) - chunk->blockPosition.x,
        floor(blockPos.y) - chunk->blockPosition.y,
        floor(blockPos.z) - chunk->blockPosition.z
    };
    if (!ServerChunk_IsValidPos(localPos)) return;

    chunk->data[ServerChunk_PosToIndex(localPos)] = blockId;

    if (arrlen(chunk->players) > 0) {
        arrput(serverWorld.generatedBlockUpdates, ((GeneratedBlockUpdate) {
            .chunk = chunk,
            .update = {.position = blockPos, .blockId = (unsigned char)blockId}
        }));
    }
}

static void ServerWorld_FlushGeneratedBlockUpdates(void) {
    while (arrlen(serverWorld.generatedBlockUpdates) > 0) {
        Chunk *chunk = serverWorld.generatedBlockUpdates[0].chunk;
        ServerBlockUpdate *updates = NULL;
        GeneratedBlockUpdate *remaining = NULL;

        for (int i = 0; i < arrlen(serverWorld.generatedBlockUpdates); i++) {
            if (serverWorld.generatedBlockUpdates[i].chunk != chunk) {
                arrput(remaining, serverWorld.generatedBlockUpdates[i]);
                continue;
            }
            arrput(updates, serverWorld.generatedBlockUpdates[i].update);
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

static bool ServerWorld_ApplyPendingBlocks(Chunk *chunk) {
    bool applied = false;
    for (int i = 0; i < arrlen(serverWorld.pendingBlocks);) {
        PendingWorldBlock pending = serverWorld.pendingBlocks[i];
        Vector3 pendingChunkPos = {
            floor(pending.position.x / CHUNK_SIZE_X),
            floor(pending.position.y / CHUNK_SIZE_Y),
            floor(pending.position.z / CHUNK_SIZE_Z)
        };
        if (!Vector3Equals(pendingChunkPos, chunk->position)) {
            i++;
            continue;
        }

        ServerWorld_WriteGeneratedBlock(chunk, pending.position, pending.blockId);
        arrdel(serverWorld.pendingBlocks, i);
        applied = true;
    }
    return applied;
}

static void ServerWorld_ProcessLoadedChunks(void) {
    pthread_mutex_lock(&serverChunkLoaderMutex);
    ChunkLoadResult *results = serverChunkLoadResults;
    serverChunkLoadResults = NULL;
    pthread_mutex_unlock(&serverChunkLoaderMutex);

    for (int i = 0; i < arrlen(results); i++) {
        ChunkLoadResult *result = &results[i];
        Chunk *chunk = result->chunk;
        if (chunk != NULL && ServerWorld_GetChunkAt(result->position) == NULL) {
            hmput(serverWorld.chunks, ServerChunk_GetPackedPos(result->position), chunk);
            if (ServerWorld_ApplyPendingBlocks(chunk)) {
                MemFree(result->compressedData);
                result->compressedData = ServerChunk_CreateCompressedData(
                    chunk, &result->compressedLength);
            }
        } else {
            ServerChunk_Destroy(chunk);
            chunk = ServerWorld_GetChunkAt(result->position);
        }

        for (int playerIndex = 0; playerIndex < WORLD_MAX_PLAYERS; playerIndex++) {
            Player *player = serverWorld.players[playerIndex];
            if (player == NULL) continue;
            if (player->chunkRequestPending &&
                Vector3Equals(player->pendingChunkPosition, result->position)) {
                player->chunkRequestPending = false;
            }
            if (chunk == NULL || !ServerWorld_PositionInLoadRadius(player, result->position) ||
                ServerChunk_PlayerInChunk(chunk, player)) continue;

            ServerChunk_AddPlayer(chunk, player);
            unsigned short *compressedData = result->compressedData;
            int compressedLength = result->compressedLength;
            bool temporaryCompression = false;
            if (compressedData == NULL) {
                compressedData = ServerChunk_CreateCompressedData(chunk, &compressedLength);
                temporaryCompression = true;
            }
            ServerNetwork_Send(player, ServerPacket_CreateLoadChunk(
                compressedData, compressedLength, result->position, chunk->skyMask));
            if (temporaryCompression) MemFree(compressedData);
        }
        MemFree(result->compressedData);
    }
    arrfree(results);
}

void ServerWorld_Init(void) {
    serverWorld.players = MemAlloc(sizeof(Player*) * WORLD_MAX_PLAYERS);
    memset(serverWorld.players, 0, sizeof(*serverWorld.players) * WORLD_MAX_PLAYERS);

    serverWorld.entities = MemAlloc(WORLD_MAX_ENTITIES * sizeof(Entity));
    memset(serverWorld.entities, 0, sizeof(*serverWorld.entities) * WORLD_MAX_ENTITIES);

    serverWorld.chunks = NULL;
    serverWorld.pendingBlocks = NULL;
    serverWorld.generatedBlockUpdates = NULL;
    serverWorld.maxDrawDistance = 8;
    serverWorld.time = 0.0f;
    serverWorldLastUpdateMilliseconds = GetTimeMilliseconds();
    serverWorldLastTimeSyncMilliseconds = serverWorldLastUpdateMilliseconds;

    // Create world directory.
    struct stat st = {0};
    if (stat("./world", &st) == -1) {
        #if defined(OS_LINUX) || defined(PLATFORM_WEB)
            mkdir("./world", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        #else
            mkdir("./world");
        #endif
    }
    
    int seed = rand();
        
    if (FileExists("./world/seed.dat")) {
        unsigned int bytesRead = 0;
        unsigned char *data = LoadFileData("./world/seed.dat", &bytesRead);
        seed = (int)(data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]); 
        UnloadFileData(data);
    } else {
        char data[4] = {(char)(seed >> 24), (char)(seed >> 16), (char)(seed >> 8), (char)(seed)};
        SaveFileData("./world/seed.dat", data, 4);
    }

    ServerWorldGenerator_Init(seed);
    ServerWorld_StartChunkLoader();
}

void ServerWorld_Shutdown(void)
{
    ServerWorld_StopChunkLoader();
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        if (serverWorld.players[i] != NULL) {
            ServerWorld_RemovePlayer(serverWorld.players[i]);
        }
    }

    for (int i = hmlen(serverWorld.chunks) - 1; i >= 0; i--) {
        ServerWorld_RemoveChunk(serverWorld.chunks[i].value);
    }

    MemFree(serverWorld.players);
    serverWorld.players = NULL;

    MemFree(serverWorld.entities);
    serverWorld.entities = NULL;

    hmfree(serverWorld.chunks);
    serverWorld.chunks = NULL;

    arrfree(serverWorld.pendingBlocks);
    serverWorld.pendingBlocks = NULL;

    arrfree(serverWorld.generatedBlockUpdates);
    serverWorld.generatedBlockUpdates = NULL;
}

void ServerWorld_Update(void) {
    ServerWorld_ProcessLoadedChunks();
    long long nowMilliseconds = GetTimeMilliseconds();
    long long elapsedMilliseconds = nowMilliseconds - serverWorldLastUpdateMilliseconds;
    serverWorldLastUpdateMilliseconds = nowMilliseconds;

    if (elapsedMilliseconds > 0) {
        serverWorld.time += elapsedMilliseconds / 1000.0f;
        while (serverWorld.time >= WORLD_DAY_LENGTH_SECONDS) {
            serverWorld.time -= WORLD_DAY_LENGTH_SECONDS;
        }
    }

    if (nowMilliseconds - serverWorldLastTimeSyncMilliseconds >= WORLD_TIME_SYNC_INTERVAL_MILLISECONDS) {
        ServerWorld_Broadcast(ServerPacket_CreateWorldTime(serverWorld.time));
        serverWorldLastTimeSyncMilliseconds = nowMilliseconds;
    }

    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *player = serverWorld.players[i];
        if (player != NULL) {
            if(ServerNetwork_PlayerReadyForRemoval(player)) {
                ServerWorld_RemovePlayer(player);
                continue;
            }
            ServerPlayer_LoadChunks(player);
        }
    }

    ServerWorld_FlushGeneratedBlockUpdates();

    for (int i = 0; i < hmlen(serverWorld.chunks); i++) {
        Chunk *chunk = serverWorld.chunks[i].value;

        for (int j = arrlen(chunk->players) - 1; j >= 0; j--) {
            Player *player = chunk->players[j];
            
            Entity entity = serverWorld.entities[player->id];
            Vector3 playerChunkPos = (Vector3) {(int)floor(entity.position.x / CHUNK_SIZE_X), (int)floor(entity.position.y / CHUNK_SIZE_Y), (int)floor(entity.position.z / CHUNK_SIZE_Z)};
            if (Vector3Distance(chunk->position, playerChunkPos) >= player->drawDistance + 3) {
                ServerNetwork_Send(player, ServerPacket_CreateUnloadChunk(chunk->position));
                ServerChunk_RemovePlayer(chunk, j);
            }
        }

        if (arrlen(chunk->players) == 0) {
            ServerWorld_RemoveChunk(chunk);
        }
    }
}

void ServerWorld_RemovePlayerFromChunks(Player *playerToRemove) {
    for(int i = 0; i < hmlen(serverWorld.chunks); i++) {
        Chunk *chunk = serverWorld.chunks[i].value;

        for(int j = arrlen(chunk->players) - 1; j >= 0; j--) {
            Player *player = chunk->players[j];

            if(player != playerToRemove) continue;
            ServerChunk_RemovePlayer(chunk, j);
        }

    }
}

Chunk* ServerWorld_AddChunk(Vector3 position) {

    long int p = ServerChunk_GetPackedPos(position);
    int index = hmgeti(serverWorld.chunks, p);
    Chunk *chunk;
    if(index == -1) {
        chunk = ServerChunk_Create(position);
        if (chunk == NULL) return NULL;

        // The server world takes ownership of the chunk.
        hmput(serverWorld.chunks, p, chunk);
        ServerChunk_Generate(chunk);
        ServerWorld_ApplyPendingBlocks(chunk);
    } else {
        chunk = serverWorld.chunks[index].value;
    }

    return chunk;
}

void ServerWorld_RemoveChunk(Chunk *currentChunk) {
    long int p = ServerChunk_GetPackedPos(currentChunk->position);

    int index = hmgeti(serverWorld.chunks, p);
    if(index >= 0) {
        hmdel(serverWorld.chunks, p);
        if (currentChunk->modified) ServerChunk_SaveFile(currentChunk);
        ServerChunk_Destroy(currentChunk);
    }
    
}

Chunk* ServerWorld_GetChunkAt(Vector3 position) {
    long int p = ServerChunk_GetPackedPos(position);
    int index = hmgeti(serverWorld.chunks, p);
    if(index >= 0) {
        return serverWorld.chunks[index].value;
    }
    
    return NULL;
}

Chunk* ServerWorld_RequestChunk(Vector3 position) {
    return ServerWorld_AddChunk(position);
}

bool ServerWorld_QueueChunk(Vector3 position) {
    if (!serverChunkLoaderStarted) return false;
    if (ServerWorld_GetChunkAt(position) != NULL) return true;

    pthread_mutex_lock(&serverChunkLoaderMutex);
    if (serverChunkLoaderBusy && Vector3Equals(serverChunkLoaderPosition, position)) {
        pthread_mutex_unlock(&serverChunkLoaderMutex);
        return true;
    }
    for (int i = 0; i < arrlen(serverChunkLoadRequests); i++) {
        if (Vector3Equals(serverChunkLoadRequests[i], position)) {
            pthread_mutex_unlock(&serverChunkLoaderMutex);
            return true;
        }
    }
    for (int i = 0; i < arrlen(serverChunkLoadResults); i++) {
        if (Vector3Equals(serverChunkLoadResults[i].position, position)) {
            pthread_mutex_unlock(&serverChunkLoaderMutex);
            return true;
        }
    }
    arrput(serverChunkLoadRequests, position);
    pthread_cond_signal(&serverChunkLoaderCondition);
    pthread_mutex_unlock(&serverChunkLoaderMutex);
    return true;
}

void ServerWorld_AddPlayer(void *player) {
    
    Player* p = (Player*)player;
    
    for(int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        if(serverWorld.players[i]) continue;
        serverWorld.players[i] = p;
        serverWorld.players[i]->id = i;
        ServerWorld_AddEntity(i, 1, 0, (Vector3) {0, 80, 0});

        Entity localEntity = serverWorld.entities[i];
        localEntity.id = USHRT_MAX;
        ServerNetwork_Send(player, ServerPacket_CreateSpawnEntity(&localEntity));
        break;
    }
    
    for(int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        if(!serverWorld.players[i] || i == p->id) continue;
        ServerNetwork_Send(player, ServerPacket_CreateSpawnEntity(&serverWorld.entities[i]));
    }
    
    ServerWorld_SendMessage(TextFormat("%s joined the game!", p->name));
}

void ServerWorld_RemovePlayer(void *player) {
    ServerWorld_RemovePlayerFromChunks(player);
    Player* curPlayer = (Player*)player;
    for(int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        if(serverWorld.players[i] == NULL) continue;
        if(serverWorld.players[i] == curPlayer) {
            serverWorld.players[i] = NULL;
            ServerWorld_RemoveEntity(i);
            break;
        }
    }
    ServerPlayer_Destroy(curPlayer);
}

void ServerWorld_TeleportEntity(int id, Vector3 position, Vector3 rotation) {
    if(serverWorld.entities[id].type == 0) return;
    serverWorld.entities[id].position = position;
    serverWorld.entities[id].rotation = rotation;
    ServerWorld_BroadcastExcluding(ServerPacket_CreateTeleportEntity(&serverWorld.entities[id], position, rotation), id);
}

void ServerWorld_AddEntity(int id, int type, int model, Vector3 position) {
    serverWorld.entities[id].id = id;
    serverWorld.entities[id].type = type;
    serverWorld.entities[id].model = (unsigned char)model;
    serverWorld.entities[id].position = position;
    ServerWorld_BroadcastExcluding(ServerPacket_CreateSpawnEntity(&serverWorld.entities[id]), id);
}

void ServerWorld_RemoveEntity(int id) {
    serverWorld.entities[id].type = 0;
    ServerWorld_BroadcastExcluding(ServerPacket_CreateDespawnEntity(&serverWorld.entities[id]), id);
}

void ServerWorld_SendMessage(const char* message) {
    int messageLength = TextLength(message);
    int parts = messageLength > 0 ? (messageLength + 63) / 64 : 1;

    for(int i = 0; i < parts; i++) {
        const char *messageChunk = TextSubtext(message, i * 64, 64);
        ServerWorld_Broadcast(i == 0
            ? ServerPacket_CreateMessage(messageChunk)
            : ServerPacket_CreateMessageContinuation(messageChunk));
    }
    
}

void ServerWorld_Broadcast(unsigned char* packet) {
    for(int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        if(!serverWorld.players[i]) continue;

        int packetLength = ServerPacket_GetLength(packet[0]);
        unsigned char* packetCopy = MemAlloc(packetLength);
        memcpy(packetCopy, packet, packetLength);

        ServerNetwork_Send((void*)serverWorld.players[i], packetCopy);
    }

    MemFree(packet);
    
}

void ServerWorld_BroadcastExcluding(unsigned char* packet, int excludedPlayerId) {
    for(int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        if(!serverWorld.players[i]) continue;
        if(serverWorld.players[i]->id == excludedPlayerId) continue;

        int packetLength = ServerPacket_GetLength(packet[0]);
        unsigned char* packetCopy = MemAlloc(packetLength);
        memcpy(packetCopy, packet, packetLength);

        ServerNetwork_Send((void*)serverWorld.players[i], packetCopy);
    }
    MemFree(packet);
}

int ServerWorld_GetBlock(Vector3 blockPos) {
    //Get Chunk
    Vector3 chunkPos = (Vector3) { floor(blockPos.x / CHUNK_SIZE_X), floor(blockPos.y / CHUNK_SIZE_Y), floor(blockPos.z / CHUNK_SIZE_Z) };
    Chunk* chunk = ServerWorld_GetChunkAt(chunkPos);
    
    if(chunk == NULL) return 0;
    
    //Get Block
    Vector3 blockPosInChunk = (Vector3) { 
                                floor(blockPos.x) - chunk->blockPosition.x,
                                floor(blockPos.y) - chunk->blockPosition.y, 
                                floor(blockPos.z) - chunk->blockPosition.z 
                               };

    return ServerChunk_GetBlock(chunk, blockPosInChunk);
}

void ServerWorld_SetBlockFast(Vector3 blockPos, int blockId) {
    Vector3 chunkPos = {
        floor(blockPos.x / CHUNK_SIZE_X),
        floor(blockPos.y / CHUNK_SIZE_Y),
        floor(blockPos.z / CHUNK_SIZE_Z)
    };
    Chunk *chunk = ServerWorld_GetChunkAt(chunkPos);
    if (chunk != NULL) {
        ServerWorld_WriteGeneratedBlock(chunk, blockPos, blockId);
        return;
    }

    arrput(serverWorld.pendingBlocks, ((PendingWorldBlock) {
        .position = {
            floor(blockPos.x),
            floor(blockPos.y),
            floor(blockPos.z)
        },
        .blockId = (unsigned short)blockId
    }));
}

void ServerWorld_SetBlock(Vector3 blockPos, int blockId, bool broadcast) {
    
    //Get Chunk
    Vector3 chunkPos = (Vector3) { floor(blockPos.x / CHUNK_SIZE_X), floor(blockPos.y / CHUNK_SIZE_Y), floor(blockPos.z / CHUNK_SIZE_Z) };
    Chunk* chunk = ServerWorld_GetChunkAt(chunkPos);
    
    if(chunk == NULL) return;

    //Set Block
    Vector3 blockPosInChunk = (Vector3) { 
                                floor(blockPos.x) - chunkPos.x * CHUNK_SIZE_X, 
                                floor(blockPos.y) - chunkPos.y * CHUNK_SIZE_Y, 
                                floor(blockPos.z) - chunkPos.z * CHUNK_SIZE_Z 
                               };
    
    int previousBlock = ServerChunk_GetBlock(chunk, blockPosInChunk);

    if(previousBlock == blockId) return;

    ServerChunk_SetBlock(chunk, blockPosInChunk, blockId);

    if(broadcast) {
        ServerWorld_Broadcast(ServerPacket_CreateSetBlock(blockId, blockPos));
    }

    LuaBindings_InvokeBlockUpdate(blockPos, blockId, previousBlock);
}
