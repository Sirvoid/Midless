/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_WORLD_H
#define G_WORLD_H

#include "raylib.h"
#include "player.h"
#include "entity.h"

#define CHUNK_SIZE_X 16
#define CHUNK_SIZE_Y 16
#define CHUNK_SIZE_Z 16
#define CHUNK_SIZE_XZ (CHUNK_SIZE_X * CHUNK_SIZE_Z)
#define CHUNK_SIZE_VEC3 CLITERAL(Vector3){ CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z }
#define CHUNK_SIZE (CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z)

typedef struct World{
    Player** players;
    Entity* entities;
} World;

extern World world;

void World_Init(void);
void World_AddPlayer(void *player);
void World_RemovePlayer(void *player);

void World_TeleportEntity(int ID, Vector3 position, Vector3 rotation);
void World_AddEntity(int ID, int type, Vector3 position);
void World_RemoveEntity(int ID);

void World_Send(void *playerPtr);
void World_SendMessage(const char* message);
void World_Broadcast(unsigned char* packet);
void World_BroadcastExcluding(unsigned char* packet, int excludedPlayerID);

int World_GetBlock(Vector3 blockPos);
void World_SetBlock(Vector3 blockPos, int blockID, bool broadcast);

#endif