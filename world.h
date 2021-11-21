/**
 * Copyright (c) 2021 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_WORLD_H
#define G_WORLD_H

#include "chunk.h"
#include "entity.h"
#include "raylib.h"

typedef struct World{
    Vector3 size;
    unsigned char *data;
    unsigned char *lightData;
    Chunk *chunks;
    unsigned char *chunksToRebuild;
    Entity *entities;
    Material mat;
    int loaded; //Number of chunks loaded
    int drawDistance;
} World;

extern World world;

//Initialize the world.
void World_Init(void);
void World_LoadSingleplayer(void);

unsigned char *World_Decompress(unsigned char *data, int currentLength, int *newLength); //Decompress world sent by the server.

//Loading
void World_StartLoading(void); //Start loading the world.
void World_LoadChunks(void); //Main thread load chunks updated by the chunk thread.

//Save/Load
void World_SaveFile(const char *fileName);
bool World_LoadFile(const char *fileName);
//Unload the world.
void World_Unload(void);
//Load world from data (World Size needs to be updated before calling this!)
void World_Load(unsigned char *worldData);
//Draw the world.
void World_Draw(Vector3 camPosition);
//Apply terrain texture to the world.
void World_ApplyTexture(Texture2D texture);
void World_ApplyShader(Shader shader);

//Set block at a given position and reload affected meshes.
void World_SetBlock(Vector3 blockPos, int blockID);
//Get block ID at a given position.
int World_GetBlock(Vector3 blockPos);

//World entities management
void World_TeleportEntity(int ID, Vector3 position);
void World_AddEntity(int ID, int type, Vector3 position);
void World_RemoveEntity(int ID);

//Lightning
void World_UpdateLightMap(Vector3 pos);
int World_GetLightLevel(Vector3 blockPos);
void World_AddLight(Vector3 pos, int intensity, bool isSunLight);
void World_PropagateLight(Vector3 pos, Vector3 *directions, int intensity, bool isSunLight);
void World_BuildLightMap(void);

//Chunk Queue management for multithreading
void World_QueueChunk(Chunk *chunk, bool updateLight); //Add chunk to queue.
void *World_ReadChunksQueue(void *state); //Update mesh of chunks in queue.

//Converters and other utilities
int World_GetFlatSize(void);
int World_IsValidChunkPos(Vector3 chunkPos);
int World_IsValidBlockPos(Vector3 blockPos);
int World_ChunkPosToIndex(Vector3 chunkPos);
int World_BlockPosToIndex(Vector3 blockPos);

Vector3 World_ChunkIndexToPos(int chunkIndex);
Vector3 World_BlockIndexToPos(int blockIndex);

Chunk* World_GetChunkAt(Vector3 pos);

#endif