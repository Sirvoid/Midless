/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#define __clang__ true
#if !defined(ISLEFORGE_STB_DS_EXTERNAL)
    #define STB_DS_IMPLEMENTATION
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "stb_ds.h"
#include "world.h"
#include "chunk/chunk.h"
#include "networkhandler.h"
#include "packet.h"
#include "entity.h"
#include "worldgenerator.h"
#include "luadefinition.h"
#include "utils.h"

typedef struct PendingWorldBlock {
    Vector3 position;
    unsigned short blockID;
} PendingWorldBlock;

typedef struct GeneratedBlockUpdate {
    Chunk *chunk;
    ServerBlockUpdate update;
} GeneratedBlockUpdate;

World serverWorld;
static long long serverWorldLastUpdateMilliseconds;
static long long serverWorldLastTimeSyncMilliseconds;

static void ServerWorld_WriteGeneratedBlock(Chunk *chunk, Vector3 blockPos, int blockID) {
    Vector3 localPos = {
        floor(blockPos.x) - chunk->blockPosition.x,
        floor(blockPos.y) - chunk->blockPosition.y,
        floor(blockPos.z) - chunk->blockPosition.z
    };
    if (!ServerChunk_IsValidPos(localPos)) return;

    chunk->data[ServerChunk_PosToIndex(localPos)] = blockID;

    if (arrlen(chunk->players) > 0) {
        arrput(serverWorld.generatedBlockUpdates, ((GeneratedBlockUpdate) {
            .chunk = chunk,
            .update = {.position = blockPos, .blockID = (unsigned char)blockID}
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

static void ServerWorld_ApplyPendingBlocks(Chunk *chunk) {
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

        ServerWorld_WriteGeneratedBlock(chunk, pending.position, pending.blockID);
        arrdel(serverWorld.pendingBlocks, i);
    }
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
}

void ServerWorld_Shutdown(void)
{
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
            if(player->disconnected) {
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

void ServerWorld_RemoveChunk(Chunk *curChunk) {
    long int p = ServerChunk_GetPackedPos(curChunk->position);

    int index = hmgeti(serverWorld.chunks, p);
    if(index >= 0) {
        hmdel(serverWorld.chunks, p);
        if (curChunk->modified) ServerChunk_SaveFile(curChunk);
        ServerChunk_Destroy(curChunk);
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

void ServerWorld_AddPlayer(void *player) {
    
    Player* p = (Player*)player;
    
    for(int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        if(serverWorld.players[i]) continue;
        serverWorld.players[i] = p;
        serverWorld.players[i]->id = i;
        ServerWorld_AddEntity(i, 1, (Vector3) {0, 80, 0});
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

void ServerWorld_TeleportEntity(int ID, Vector3 position, Vector3 rotation) {
    if(serverWorld.entities[ID].type == 0) return;
    serverWorld.entities[ID].position = position;
    serverWorld.entities[ID].rotation = rotation;
    ServerWorld_BroadcastExcluding(ServerPacket_CreateTeleportEntity(&serverWorld.entities[ID], position, rotation), ID);
}

void ServerWorld_AddEntity(int ID, int type, Vector3 position) {
    serverWorld.entities[ID].ID = ID;
    serverWorld.entities[ID].type = type;
    serverWorld.entities[ID].position = position;
    ServerWorld_BroadcastExcluding(ServerPacket_CreateSpawnEntity(&serverWorld.entities[ID]), ID);
}

void ServerWorld_RemoveEntity(int ID) {
    serverWorld.entities[ID].type = 0;
    ServerWorld_BroadcastExcluding(ServerPacket_CreateDespawnEntity(&serverWorld.entities[ID]), ID);
}

void ServerWorld_SendMessage(const char* message) {
    int parts = TextLength(message) / 64;
    
    for(int i = 0; i <= parts; i++) {
        const char *messageChunk = TextSubtext(message, i * 64, 64);
        ServerWorld_Broadcast(ServerPacket_CreateMessage(messageChunk));
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

void ServerWorld_BroadcastExcluding(unsigned char* packet, int excludedPlayerID) {
    for(int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        if(!serverWorld.players[i]) continue;
        if(serverWorld.players[i]->id == excludedPlayerID) continue;

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

void ServerWorld_SetBlockFast(Vector3 blockPos, int blockID) {
    Vector3 chunkPos = {
        floor(blockPos.x / CHUNK_SIZE_X),
        floor(blockPos.y / CHUNK_SIZE_Y),
        floor(blockPos.z / CHUNK_SIZE_Z)
    };
    Chunk *chunk = ServerWorld_GetChunkAt(chunkPos);
    if (chunk != NULL) {
        ServerWorld_WriteGeneratedBlock(chunk, blockPos, blockID);
        return;
    }

    arrput(serverWorld.pendingBlocks, ((PendingWorldBlock) {
        .position = {
            floor(blockPos.x),
            floor(blockPos.y),
            floor(blockPos.z)
        },
        .blockID = (unsigned short)blockID
    }));
}

void ServerWorld_SetBlock(Vector3 blockPos, int blockID, bool broadcast) {
    
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

    if(previousBlock == blockID) return;

    ServerChunk_SetBlock(chunk, blockPosInChunk, blockID);

    if(broadcast) {
        ServerWorld_Broadcast(ServerPacket_CreateSetBlock(blockID, blockPos));
    }

    LD_OnBlockUpdateCall(blockPos, blockID, previousBlock);
}
