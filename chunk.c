/**
 * Copyright (c) 2021 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include "raylib.h" 
#include "raymath.h"
#include "chunk.h"
#include "world.h"
#include "blockfacehelper.h"
#include "block.h"
#include "chunkmesh.h"

int Chunk_triangleCounter = 0;
int Chunk_triangleCounterTransparent = 0;

void Chunk_Init(Chunk *chunk, Vector3 pos) {
    chunk->position = pos;
    chunk->index = World_ChunkPosToIndex(pos);
    chunk->blockPosition = Vector3Multiply(chunk->position, CHUNK_SIZE_VEC3);
    chunk->mesh = (ChunkMesh*)MemAlloc(sizeof(ChunkMesh));
    chunk->meshTransparent = (ChunkMesh*)MemAlloc(sizeof(ChunkMesh));
}

void Chunk_Unload(Chunk *chunk) {
    ChunkMesh_Unload(*chunk->mesh, true);
    ChunkMesh_Unload(*chunk->meshTransparent, true);
}

void Chunk_AllocateMeshData(ChunkMesh *mesh, int triangleCount) {

    mesh->vertexCount = triangleCount * 3;
    mesh->triangleCount = triangleCount;

    int vertXchar = mesh->vertexCount * sizeof(unsigned char);

    mesh->vertices = (unsigned char*)MemAlloc(vertXchar * 3);
    mesh->texcoords = (unsigned char*)MemAlloc(vertXchar * 2);
    mesh->colors = (unsigned char*)MemAlloc(vertXchar);
}

void Chunk_ReAllocateMeshData(ChunkMesh *mesh, int triangleCount) {
    mesh->vertexCount = triangleCount * 3;
    mesh->triangleCount = triangleCount;
    
    int vertXchar = mesh->vertexCount * sizeof(unsigned char);
    
    mesh->vertices = (unsigned char*)MemRealloc(mesh->vertices, vertXchar * 3);
    mesh->texcoords = (unsigned char*)MemRealloc(mesh->texcoords, vertXchar * 2);
    mesh->colors = (unsigned char*)MemRealloc(mesh->colors, vertXchar);
}

void Chunk_BuildMesh(Chunk *chunk) {

    chunk->loaded = 1;
    
    Chunk_AllocateMeshData(chunk->mesh, 2 * 6 * CHUNK_SIZE);
    Chunk_AllocateMeshData(chunk->meshTransparent, 2 * 6 * CHUNK_SIZE);
    
    BFH_ResetIndexes();
    Chunk_triangleCounter = 0;
    Chunk_triangleCounterTransparent = 0;
    
    for(int i = 0; i < CHUNK_SIZE; i++) {
        Vector3 pos = Chunk_IndexToPos(i);
        Vector3 worldPos = Vector3Add(pos, chunk->blockPosition);
        
        int blockID = World_GetBlock(worldPos);
        Block blockDef = Block_definition[blockID];

        bool translucent = blockDef.renderType == BlockRenderType_Translucent;
        bool transparent = blockDef.renderType == BlockRenderType_Transparent;
        bool sprite = blockDef.modelType == BlockModelType_Sprite;
        
        if(translucent || transparent || sprite) {
            Chunk_AddCube(chunk, chunk->meshTransparent, pos, worldPos, blockDef);
        } else {
            Chunk_AddCube(chunk, chunk->mesh, pos, worldPos, blockDef);
        }
        
    }
    
    Chunk_ReAllocateMeshData(chunk->mesh, Chunk_triangleCounter);
    Chunk_ReAllocateMeshData(chunk->meshTransparent, Chunk_triangleCounterTransparent);
    
    world.chunksToRebuild[chunk->index] = 2;
    
}

void Chunk_AddCube(Chunk *chunk, ChunkMesh *mesh, Vector3 pos, Vector3 worldPos, Block blockDef) {
    
    if(blockDef.modelType == BlockModelType_Gas) return;
    
    int numberFaces = 6;
    if(blockDef.modelType == BlockModelType_Sprite) numberFaces = 4;
    
    for(int i = 0; i < numberFaces; i++) {
        Chunk_AddFace(chunk, mesh, pos, worldPos, (BlockFace)i, blockDef);
    }
}

void Chunk_AddFace(Chunk *chunk, ChunkMesh *mesh, Vector3 pos, Vector3 worldPos, BlockFace face, Block blockDef) {
    Vector3 faceDir = BFH_GetDirection(face);
    Vector3 nextPos = Vector3Add(worldPos, faceDir);
    
    int nextBlockID = World_GetBlock(nextPos);
    Block nextDef = Block_definition[nextBlockID];
    
    //Tests
    bool sprite = blockDef.modelType == BlockModelType_Sprite;
    
    bool opaque = blockDef.renderType == BlockRenderType_Opaque;
    bool translucent = blockDef.renderType == BlockRenderType_Translucent;
    bool transparent = blockDef.renderType == BlockRenderType_Transparent;
    bool fullSize = blockDef.minBB.x == 0 && blockDef.minBB.y == 0 && blockDef.minBB.z == 0 && blockDef.maxBB.x == 16 && blockDef.maxBB.y == 16 && blockDef.maxBB.z == 16;
    
    bool nextTranslucent = nextDef.renderType == BlockRenderType_Translucent;
    bool nextTransparent = nextDef.renderType == BlockRenderType_Transparent;
    bool nextSprite = nextDef.modelType == BlockModelType_Sprite;
    bool nextFullSize = nextDef.minBB.x == 0 && nextDef.minBB.y == 0 && nextDef.minBB.z == 0 && nextDef.maxBB.x == 16 && nextDef.maxBB.y == 16 && nextDef.maxBB.z == 16;

    bool bTest = true;
    if(opaque) bTest = (nextDef.modelType == BlockModelType_Gas || nextTranslucent || nextTransparent || nextSprite || !nextFullSize || !fullSize);
    else if(translucent) bTest = (nextDef.modelType == BlockModelType_Gas  || nextTransparent || !nextFullSize || !fullSize);
    else if(transparent) {
        Vector3 prevPos = Vector3Subtract(worldPos, faceDir);
        int behindBlockID = World_GetBlock(prevPos);
        Block behindDef = Block_definition[behindBlockID];
        bTest = ((behindDef.modelType == BlockModelType_Gas && nextTransparent) || nextDef.modelType == BlockModelType_Gas  || nextTranslucent || nextSprite || !nextFullSize || !fullSize);
    }
    
    //Get Face's Light Level
    int lightLevel = 0;
    if(sprite || transparent) {
        lightLevel = World_GetLightLevel(worldPos);
    } else {
        lightLevel = World_GetLightLevel(nextPos);
    }
    
    //Build face
    if(bTest || sprite) {
        if(translucent || transparent || sprite) { 
            Chunk_triangleCounterTransparent += 2;
            BFH_AddFace(mesh, face, pos, blockDef, 1, lightLevel);
        } else {
            Chunk_triangleCounter += 2;
            BFH_AddFace(mesh, face, pos, blockDef, 0, lightLevel);
        }
            
    }
}

void Chunk_RefreshBorderingChunks(Chunk *chunk, Vector3 blockPos) {
    
    int c = 0;
    Vector3 directions[27];

    for(int x = -1; x <= 1; x++) {
        for(int y = -1; y <= 1; y++) {
            for(int z = -1; z <= 1; z++) {
                if(x == 0 && y == 0 && z == 0) continue;
                Vector3 chunkBlockPos = (Vector3){(x + 0.5f) * CHUNK_SIZE_X, (y + 0.5f) * CHUNK_SIZE_Y, (z + 0.5f) * CHUNK_SIZE_Z};
                if(Vector3Distance(chunkBlockPos, blockPos) <= CHUNK_SIZE_X) directions[c++] = (Vector3){x, y, z};
            }
        }
    }

    for(int i = 0; i < c; i++) {
        Chunk *borderingChunk = World_GetChunkAt(Vector3Add(chunk->position, directions[i]));
        if(borderingChunk == NULL) continue;
        World_QueueChunk(borderingChunk, false);
    }
    
}

Vector3 Chunk_IndexToPos(int index) {
    int x = (int)(index % CHUNK_SIZE_X);
	int y = (int)(index / CHUNK_SIZE_XZ);
	int z = (int)(index / CHUNK_SIZE_X) % CHUNK_SIZE_Z;
    return (Vector3){x, y, z};
}
