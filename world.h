#ifndef G_WORLD_H
#define G_WORLD_H

#include "chunk.h"
#include "raylib.h"

#define WORLD_SIZE_X 8
#define WORLD_SIZE_Y 8
#define WORLD_SIZE_Z 8
#define WORLD_SIZE (WORLD_SIZE_X * WORLD_SIZE_Y * WORLD_SIZE_Z)

#define WORLD_BLOCK_SIZE_X (WORLD_SIZE_X * CHUNK_SIZE_X)
#define WORLD_BLOCK_SIZE_Y (WORLD_SIZE_Y * CHUNK_SIZE_Y)
#define WORLD_BLOCK_SIZE_Z (WORLD_SIZE_Z * CHUNK_SIZE_Z)

typedef struct World{
    Chunk chunks[WORLD_SIZE];
    Material mat;
} World;

//Initialize the world.
void World_Init();
//Unload the world.
void World_Unload();
//Draw the world.
void World_Draw();
//Apply terrain texture to the world.
void World_ApplyTexture(Texture2D texture);
//Set block at a given position and reload affected meshes.
void World_SetBlock(Vector3 blockPos, int blockID);

//Get block ID at a given position.
int World_GetBlock(Vector3 blockPos);

int World_IsValidChunkPos(Vector3 chunkPos);
int World_IsValidBlockPos(Vector3 blockPos);
int World_ChunkPosToIndex(Vector3 chunkPos);
int World_BlockPosToIndex(Vector3 blockPos);

Vector3 World_ChunkIndexToPos(int chunkIndex);
Vector3 World_BlockIndexToPos(int blockIndex);

Chunk* World_GetChunkAt(Vector3 pos);

#endif