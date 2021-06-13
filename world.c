#include "world.h"
#include "raylib.h" 
#include "chunk.h"
#include <stddef.h>

World world;

void World_Init() {
    world.mat = LoadMaterialDefault();
    for(int i = 0; i < WORLD_SIZE; i++) {
        Chunk* chunk = &world.chunks[i];
        Vector3 pos = World_ChunkIndexToPos(i);
        Chunk_Init(chunk, pos);
        Chunk_BuildMesh(chunk);
    }
}

void World_Unload() {
    for(int i = 0; i < WORLD_SIZE; i++) {
        Chunk_Unload(&world.chunks[i]);
    }
}

void World_ApplyTexture(Texture2D texture) {
    SetMaterialTexture(&world.mat, MATERIAL_MAP_DIFFUSE, texture);
}

void World_Draw() {
    for(int i = 0; i < WORLD_SIZE; i++) {
        Chunk* chunk = &world.chunks[i];
        Matrix matrix = (Matrix) { 1, 0, 0, 0, 
                                   0, 1, 0, 0,
                                   0, 0, 1, 0,
                                   0, 0, 0, 1 };
        DrawMesh(*chunk->mesh, world.mat, matrix);
    }
}

int World_GetBlock(Vector3 blockPos) {
    
    //Get Chunk
    Vector3 chunkPos = (Vector3) { (int)blockPos.x / CHUNK_SIZE_X, (int)blockPos.y / CHUNK_SIZE_Y, (int)blockPos.z / CHUNK_SIZE_Z };
    Chunk* chunk = World_GetChunkAt(chunkPos);
    
    if(chunk == NULL) {
        return 0;
    }
    
    //Get Block
    Vector3 blockPosInChunk = (Vector3) { 
                                    blockPos.x - chunkPos.x * CHUNK_SIZE_X, 
                                    blockPos.y - chunkPos.y * CHUNK_SIZE_Y, 
                                    blockPos.z - chunkPos.z * CHUNK_SIZE_Z 
                               };

    return Chunk_GetBlock(chunk, blockPosInChunk);
}

void World_SetBlock(Vector3 blockPos, int blockID) {
    //Get Chunk
    Vector3 chunkPos = (Vector3) { (int)blockPos.x / CHUNK_SIZE_X, (int)blockPos.y / CHUNK_SIZE_Y, (int)blockPos.z / CHUNK_SIZE_Z };
    Chunk* chunk = World_GetChunkAt(chunkPos);
    
    if(chunk == NULL) {
        return;
    }
    
    //Set Block
    Vector3 blockPosInChunk = (Vector3) { 
                                    blockPos.x - chunkPos.x * CHUNK_SIZE_X, 
                                    blockPos.y - chunkPos.y * CHUNK_SIZE_Y, 
                                    blockPos.z - chunkPos.z * CHUNK_SIZE_Z 
                               };
    Chunk_SetBlock(chunk, blockPosInChunk, blockID);
}

Chunk* World_GetChunkAt(Vector3 pos) {
    int index = World_ChunkPosToIndex(pos);
    
    if(World_IsValidChunkPos(pos)) {
        return &world.chunks[index];
    }
    
    return NULL;
}

int World_IsValidChunkPos(Vector3 chunkPos) {
    return chunkPos.x >= 0 && chunkPos.x < WORLD_SIZE_X && chunkPos.y >= 0 && chunkPos.y < WORLD_SIZE_Y && chunkPos.z >= 0 && chunkPos.z < WORLD_SIZE_Z;
}

int World_IsValidBlockPos(Vector3 blockPos) {
    return blockPos.x >= 0 && blockPos.x < WORLD_BLOCK_SIZE_X && blockPos.y >= 0 && blockPos.y < WORLD_BLOCK_SIZE_Y && blockPos.z >= 0 && blockPos.z < WORLD_BLOCK_SIZE_Z;
}

Vector3 World_ChunkIndexToPos(int chunkIndex) {
    int x = (int)(chunkIndex % WORLD_SIZE_X);
	int y = (int)(chunkIndex / (WORLD_SIZE_X * WORLD_SIZE_Z));
	int z = (int)((int)(chunkIndex / WORLD_SIZE_X) % WORLD_SIZE_Z);
    return (Vector3){x, y, z};
}

int World_ChunkPosToIndex(Vector3 chunkPos) {
    return (int)(((int)chunkPos.y * WORLD_SIZE_Z + (int)chunkPos.z) * WORLD_SIZE_X + (int)chunkPos.x);
}

Vector3 World_BlockIndexToPos(int blockIndex) {
    int x = (int)(blockIndex % WORLD_BLOCK_SIZE_X);
	int y = (int)(blockIndex / (WORLD_BLOCK_SIZE_X * WORLD_BLOCK_SIZE_Z));
	int z = (int)((int)(blockIndex / WORLD_BLOCK_SIZE_X) % WORLD_BLOCK_SIZE_Z);
    return (Vector3){x, y, z};
}

int World_BlockPosToIndex(Vector3 blockPos) {
    return (int)(((int)blockPos.y * WORLD_BLOCK_SIZE_Z + (int)blockPos.z) * WORLD_BLOCK_SIZE_X + (int)blockPos.x);
}