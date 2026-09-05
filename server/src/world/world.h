/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SERVER_WORLD_H
#define MIDLESS_SERVER_WORLD_H

#define WORLD_MAX_ENTITIES 1028
#define WORLD_MAX_PLAYERS 256

#include "raylib.h"
#include "../player.h"
#include "../entity.h"
#include "chunk/chunk.h"
#include "worldtime.h"

typedef struct World{
    BlockDefinition blockDefinitions[256];
    bool hasBlockDefinition[256];
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
void ServerWorld_RemoveChunk(Chunk *currentChunk);
Chunk* ServerWorld_GetChunkAt(Vector3 position);
Chunk* ServerWorld_RequestChunk(Vector3 position);
bool ServerWorld_QueueChunk(Vector3 position);

void ServerWorld_AddPlayer(void *player);
void ServerWorld_RemovePlayer(void *player);

void ServerWorld_TeleportEntity(int id, Vector3 position, Vector3 rotation);
void ServerWorld_AddEntity(int id, int type, int model, Vector3 position);
void ServerWorld_RemoveEntity(int id);

void ServerWorld_Send(void *player);
void ServerWorld_SendMessage(const char* message);
//Broadcast and take ownership.
void ServerWorld_Broadcast(unsigned char* packet);
void ServerWorld_BroadcastExcluding(unsigned char* packet, int excludedPlayerId);
bool ServerWorld_IsBlockDefined(int id);
void ServerWorld_DefineBlock(int id, const BlockDefinition *definition);
void ServerWorld_SendBlockDefinitions(Player *player);
void ServerWorld_RemoveBlockDefinition(int id);

int ServerWorld_GetBlock(Vector3 blockPos);
void ServerWorld_SetBlockFast(Vector3 blockPos, int blockId);
void ServerWorld_SetBlock(Vector3 blockPos, int blockId, bool broadcast);

#endif
