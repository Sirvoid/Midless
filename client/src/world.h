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
//Load singleplayer world.
void World_LoadSingleplayer(void);
//Build Chunks mesh in queue
void World_UpdateChunks(void);
//Load & Unload Chunks around players.
void World_LoadChunks(bool loadEdges);
//Read Queue to generate chunks.
void *World_ReadChunksQueues(void *state);
//Queue a chunk to build it.
void World_QueueChunk(Chunk *chunk);
//Add a chunk.
void World_AddChunk(Vector3 position);
//Unload the world.
void World_Unload(void);
//Reload chunks.
void World_Reload(void);
//Draw the world.
void World_Draw(Vector3 camPosition);
//Apply terrain texture to the world.
void World_ApplyTexture(Texture2D texture);
//Apply a shader to the world.
void World_ApplyShader(Shader shader);
//Set block at a given position and reload affected meshes.
void World_SetBlock(Vector3 blockPos, int blockID, bool immediate);
//Get block ID at a given position.
int World_GetBlock(Vector3 blockPos);
//Get a Chunk at a given position.
Chunk* World_GetChunkAt(Vector3 pos);
//Teleport an Entity in the world
void World_TeleportEntity(int ID, Vector3 position, Vector3 rotation);
//Add an Entity to the world
void World_AddEntity(int ID, int type, Vector3 position, Vector3 rotation);
//Remove an Entity from the world
void World_RemoveEntity(int ID);

#endif