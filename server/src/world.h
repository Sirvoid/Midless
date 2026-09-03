/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef S_WORLD_H
#define S_WORLD_H

#define WORLD_MAX_ENTITIES 1028
#define WORLD_MAX_PLAYERS 256

#include "raylib.h"
#include "player.h"
#include "entity.h"
#include "chunk/chunk.h"
#include "worldtime.h"

typedef struct World{
    Player** players;
    Entity* entities;
    struct { long int key; Chunk* value; } *chunks;
    struct PendingWorldBlock *pendingBlocks;
    struct GeneratedBlockUpdate *generatedBlockUpdates;
    int maxDrawDistance;
    float time;
} World;

extern World serverWorld;

void ServerWorld_Init(void);
void ServerWorld_Shutdown(void);
void ServerWorld_Update(void);

void ServerWorld_RemovePlayerFromChunks(Player *playerToRemove);

Chunk* ServerWorld_AddChunk(Vector3 position);
void ServerWorld_RemoveChunk(Chunk *curChunk);
Chunk* ServerWorld_GetChunkAt(Vector3 position);
Chunk* ServerWorld_RequestChunk(Vector3 position);

void ServerWorld_AddPlayer(void *player);
void ServerWorld_RemovePlayer(void *player);

void ServerWorld_TeleportEntity(int ID, Vector3 position, Vector3 rotation);
void ServerWorld_AddEntity(int ID, int type, Vector3 position);
void ServerWorld_RemoveEntity(int ID);

void ServerWorld_Send(void *playerPtr);
void ServerWorld_SendMessage(const char* message);
//Broadcast and take ownership.
void ServerWorld_Broadcast(unsigned char* packet);
void ServerWorld_BroadcastExcluding(unsigned char* packet, int excludedPlayerID);

int ServerWorld_GetBlock(Vector3 blockPos);
void ServerWorld_SetBlockFast(Vector3 blockPos, int blockID);
void ServerWorld_SetBlock(Vector3 blockPos, int blockID, bool broadcast);

#endif
