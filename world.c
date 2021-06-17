#include <stdio.h>
#include <stddef.h>
#include "world.h"
#include "raylib.h"
#include "raymath.h"
#include "chunk.h"
#include "chunkmesh.h"

World world;

#define WORLD_RENDER_DISTANCE 256

void World_Init(void) {
    world.mat = LoadMaterialDefault();
    
    double startTime = GetTime();
    
    //Create Chunks
    for(int i = 0; i < WORLD_SIZE; i++) {
        Vector3 pos = World_ChunkIndexToPos(i);
        Chunk_Init(&world.chunks[i], pos);
    }
    
    //Refresh meshes
    for(int i = 0; i < WORLD_SIZE; i++) {
        Chunk_BuildMesh(&world.chunks[i]);
    }
    
    printf("World loaded in %f seconds.\n", GetTime() - startTime);
}

void World_Unload(void) {
    for(int i = 0; i < WORLD_SIZE; i++) {
        Chunk_Unload(&world.chunks[i]);
    }
}

void World_ApplyTexture(Texture2D texture) {
    SetMaterialTexture(&world.mat, MATERIAL_MAP_DIFFUSE, texture);
}

void World_ApplyShader(Shader shader) {
    world.mat.shader = shader;
}

void World_Draw(Vector3 camPosition) {
    for(int i = 0; i < WORLD_SIZE; i++) {
        Chunk* chunk = &world.chunks[i];
         
        if(Vector3Distance(chunk->blockPosition, camPosition) > WORLD_RENDER_DISTANCE) continue;
        
        Matrix matrix = (Matrix) { 1, 0, 0, chunk->blockPosition.x,
                                   0, 1, 0, chunk->blockPosition.y,
                                   0, 0, 1, chunk->blockPosition.z,
                                   0, 0, 0, 1 };
        
        ChunkMesh_Draw(*chunk->mesh, world.mat, matrix);
    }
}

int World_GetBlock(Vector3 blockPos) {
    
    //Get Chunk
    Vector3 chunkPos = (Vector3) { (int)blockPos.x / CHUNK_SIZE_X, (int)blockPos.y / CHUNK_SIZE_Y, (int)blockPos.z / CHUNK_SIZE_Z };
    Chunk* chunk = World_GetChunkAt(chunkPos);
    
    if(chunk == NULL) return 0;
    
    //Get Block
    Vector3 blockPosInChunk = (Vector3) { 
                                (int)blockPos.x - chunkPos.x * CHUNK_SIZE_X, 
                                (int)blockPos.y - chunkPos.y * CHUNK_SIZE_Y, 
                                (int)blockPos.z - chunkPos.z * CHUNK_SIZE_Z 
                               };

    return Chunk_GetBlock(chunk, blockPosInChunk);
}

void World_SetBlock(Vector3 blockPos, int blockID) {
    
    //Get Chunk
    Vector3 chunkPos = (Vector3) { (int)blockPos.x / CHUNK_SIZE_X, (int)blockPos.y / CHUNK_SIZE_Y, (int)blockPos.z / CHUNK_SIZE_Z };
    Chunk* chunk = World_GetChunkAt(chunkPos);
    
    if(chunk == NULL) return;
    
    //Set Block
    Vector3 blockPosInChunk = (Vector3) { 
                                (int)blockPos.x - chunkPos.x * CHUNK_SIZE_X, 
                                (int)blockPos.y - chunkPos.y * CHUNK_SIZE_Y, 
                                (int)blockPos.z - chunkPos.z * CHUNK_SIZE_Z 
                               };
    
    Chunk_SetBlock(chunk, blockPosInChunk, blockID);
    
    //Refresh mesh of neighbour chunks.
    Chunk_RefreshBorderingChunks(chunk, blockPosInChunk);
    
    //Refresh current chunk.
    Chunk_BuildMesh(chunk);
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
	int z = (int)(chunkIndex / WORLD_SIZE_X) % WORLD_SIZE_Z;
    return (Vector3){x, y, z};
}

int World_ChunkPosToIndex(Vector3 chunkPos) {
    return (int)(((int)chunkPos.y * WORLD_SIZE_Z + (int)chunkPos.z) * WORLD_SIZE_X + (int)chunkPos.x);
}

Vector3 World_BlockIndexToPos(int blockIndex) {
    int x = (int)(blockIndex % WORLD_BLOCK_SIZE_X);
	int y = (int)(blockIndex / (WORLD_BLOCK_SIZE_X * WORLD_BLOCK_SIZE_Z));
	int z = (int)(blockIndex / WORLD_BLOCK_SIZE_X) % WORLD_BLOCK_SIZE_Z;
    return (Vector3){x, y, z};
}

int World_BlockPosToIndex(Vector3 blockPos) {
    return ((int)blockPos.y * WORLD_BLOCK_SIZE_Z + (int)blockPos.z) * WORLD_BLOCK_SIZE_X + (int)blockPos.x;
}