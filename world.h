#ifndef G_WORLD_H
#define G_WORLD_H

#include "chunk.h"
#include "raylib.h"

#define WORLD_SIZE_X 4
#define WORLD_SIZE_Y 4
#define WORLD_SIZE_Z 4
#define WORLD_SIZE (WORLD_SIZE_X * WORLD_SIZE_Y * WORLD_SIZE_Z)

typedef struct {
    Chunk chunks[WORLD_SIZE];
    Material mat;
} World;

void World_Init(World *world);
Vector3 World_ChunkIndexToPos(int chunkIndex);
int World_ChunkPosToIndex(Vector3 chunkPos);
void World_Unload(World *world);
void World_Draw(World *world);
void World_ApplyTexture(World *world, Texture2D texture);

#endif