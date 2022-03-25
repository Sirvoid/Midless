/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_WORLD_H
#define G_WORLD_H

#include <raylib.h>
#include "chunk/chunk.h"
#include "entity/entity.h"

#define WORLD_MAX_ENTITIES 1028

typedef struct World{
    Entity *entities;
    Chunk *chunks;
    QueuedChunk *generateChunksQueue;
    QueuedChunk *buildChunksQueue;
    Material mat;
    int drawDistance;
    bool loadChunks;
} World;

extern World world;

//Initialize the world.
void World_Init(void);
void World_LoadSingleplayer(void);
void World_LoadChunks();
void *World_ReadChunksQueues(void *state);
void World_QueueChunk(Chunk *chunk);
//Unload the world.
void World_Unload(void);
//Draw the world.
void World_Draw(Vector3 camPosition);
//Apply terrain texture to the world.
void World_ApplyTexture(Texture2D texture);
void World_ApplyShader(Shader shader);
//Set block at a given position and reload affected meshes.
void World_SetBlock(Vector3 blockPos, int blockID, bool immediate);

//Get block ID at a given position.
int World_GetBlock(Vector3 blockPos);

Chunk* World_GetChunkAt(Vector3 pos);

//World entities management
void World_TeleportEntity(int ID, Vector3 position, Vector3 rotation);
void World_AddEntity(int ID, int type, Vector3 position, Vector3 rotation);
void World_RemoveEntity(int ID);

#endif