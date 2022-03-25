/**
 * Copyright (c) 2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <raylib.h>
#include <raymath.h>
#include "chunkMeshGeneration.h"
#include "chunkLightning.h"
#include "../block/blockMeshGeneration.h"

int Chunk_triangleCounter = 0;
int Chunk_triangleCounterTransparent = 0;

void Chunk_AllocateMeshData(ChunkMesh *mesh, int triangleCount) {

    mesh->vertexCount = triangleCount * 2;
    mesh->triangleCount = triangleCount;

    mesh->vertices = (unsigned char*)MemAlloc(mesh->vertexCount * 3);
    mesh->texcoords = (unsigned char*)MemAlloc(mesh->vertexCount * 2);
    mesh->colors = (unsigned char*)MemAlloc(mesh->vertexCount);
    mesh->indices = (unsigned short*)MemAlloc(mesh->triangleCount * 3 * sizeof(unsigned short));
}

void Chunk_ReAllocateMeshData(ChunkMesh *mesh, int triangleCount) {
    mesh->vertexCount = triangleCount * 2;
    mesh->triangleCount = triangleCount;
    
    mesh->vertices = (unsigned char*)MemRealloc(mesh->vertices, mesh->vertexCount * 3);
    mesh->texcoords = (unsigned char*)MemRealloc(mesh->texcoords, mesh->vertexCount * 2);
    mesh->colors = (unsigned char*)MemRealloc(mesh->colors, mesh->vertexCount);
    mesh->indices = (unsigned short*)MemRealloc(mesh->indices ,mesh->triangleCount * 3 * sizeof(unsigned short));
}


void Chunk_BuildMesh(Chunk *chunk) {

    if(chunk->isBuilt == true) {
        ChunkMesh_Unload(chunk->mesh);
        ChunkMesh_Unload(chunk->meshTransparent);
    }

    Chunk_AllocateMeshData(chunk->mesh, 2 * 6 * CHUNK_SIZE);
    Chunk_AllocateMeshData(chunk->meshTransparent, 2 * 6 * CHUNK_SIZE);
    
    BlockMesh_ResetIndexes();
    Chunk_triangleCounter = 0;
    Chunk_triangleCounterTransparent = 0;
    
    for(int i = 0; i < CHUNK_SIZE; i++) {
        int blockID = chunk->data[i];
        Block blockDef = Block_GetDefinition(blockID);
        if(blockDef.modelType == BlockModelType_Gas) continue;

        Vector3 pos = Chunk_IndexToPos(i);
        bool translucent = blockDef.renderType == BlockRenderType_Translucent;

        if(translucent) {
            Chunk_AddCube(chunk, chunk->meshTransparent, pos, blockDef);
        } else {
            Chunk_AddCube(chunk, chunk->mesh, pos, blockDef);
        }
    }
    
    Chunk_ReAllocateMeshData(chunk->mesh, Chunk_triangleCounter);
    Chunk_ReAllocateMeshData(chunk->meshTransparent, Chunk_triangleCounterTransparent);

    ChunkMesh_Upload(chunk->mesh);
    ChunkMesh_Upload(chunk->meshTransparent);

    chunk->isBuilt = true;
}

void Chunk_AddCube(Chunk *chunk, ChunkMesh *mesh, Vector3 pos, Block blockDef) {
    
    int numberFaces = 6;
    if(blockDef.modelType == BlockModelType_Sprite) numberFaces = 4;
    
    for(int i = 0; i < numberFaces; i++) {
        Chunk_AddFace(chunk, mesh, pos, (BlockFace)i, blockDef);
    }
}

void Chunk_AddFace(Chunk *chunk, ChunkMesh *mesh, Vector3 pos, BlockFace face, Block blockDef) {
    Vector3 faceDir = BlockMesh_GetDirection(face);
    Vector3 nextPos = Vector3Add(pos, faceDir);
    Chunk *nextChunk = chunk;

    int nextBlockID = 0;
    if(!Chunk_IsValidPos(nextPos)) {
        Chunk *neighbour = chunk->neighbours[(int)face];
        if(neighbour == NULL) return;
        nextPos.x = ((int)nextPos.x % CHUNK_SIZE_X + CHUNK_SIZE_X) % CHUNK_SIZE_X;
        nextPos.y = ((int)nextPos.y % CHUNK_SIZE_Y + CHUNK_SIZE_Y) % CHUNK_SIZE_Y;
        nextPos.z = ((int)nextPos.z % CHUNK_SIZE_Z + CHUNK_SIZE_Z) % CHUNK_SIZE_Z;
        nextChunk = neighbour;
        nextBlockID = neighbour->data[Chunk_PosToIndex(nextPos)];
    } else {
        nextBlockID = chunk->data[Chunk_PosToIndex(nextPos)];
    }

    Block nextDef = Block_GetDefinition(nextBlockID);

    //Tests
    bool sprite = blockDef.modelType == BlockModelType_Sprite;

    BlockRenderType renderType = BlockRenderType_Opaque;
    if(blockDef.renderType == BlockRenderType_Translucent) {
        renderType = BlockRenderType_Translucent;
    } else if(blockDef.renderType == BlockRenderType_Transparent) {
        renderType = BlockRenderType_Transparent;
    }
    
    bool bTest = true;
    if(renderType == BlockRenderType_Opaque) bTest = Chunk_TestOpaque(blockDef, nextDef);
    else if(renderType == BlockRenderType_Translucent) bTest = Chunk_TestTranslucent(blockDef, nextDef);
    
    //Get Face's Light Level
    int lightLevel = 0;
    if(sprite || renderType == BlockRenderType_Transparent) {
        lightLevel = Chunk_GetLight(chunk, pos);
    } else {
        lightLevel = Chunk_GetLight(nextChunk, nextPos);
    }
    
    //Build face
    if(bTest || sprite) {
        if(renderType == BlockRenderType_Translucent) { 
            Chunk_triangleCounterTransparent += 2;
            BlockMesh_AddFace(mesh, face, pos, blockDef, 1, lightLevel);
        } else {
            Chunk_triangleCounter += 2;
            BlockMesh_AddFace(mesh, face, pos, blockDef, 0, lightLevel);
        }
            
    }
}

bool Chunk_TestOpaque(Block blockDef, Block nextDef) {
    if(nextDef.modelType == BlockModelType_Gas) return true;
    if(nextDef.renderType == BlockRenderType_Translucent) return true;
    if(nextDef.renderType == BlockRenderType_Transparent) return true;
    if(nextDef.modelType == BlockModelType_Sprite) return true;
    if(!(blockDef.minBB.x == 0 && blockDef.minBB.y == 0 && blockDef.minBB.z == 0 && blockDef.maxBB.x == 16 && blockDef.maxBB.y == 16 && blockDef.maxBB.z == 16)) return true;
    if(!(nextDef.minBB.x == 0 && nextDef.minBB.y == 0 && nextDef.minBB.z == 0 && nextDef.maxBB.x == 16 && nextDef.maxBB.y == 16 && nextDef.maxBB.z == 16)) return true;
    
    return false;
}

bool Chunk_TestTranslucent(Block blockDef, Block nextDef) {
    if(nextDef.modelType == BlockModelType_Gas) return true;
    if(nextDef.renderType == BlockRenderType_Transparent) return true;
    if(!(blockDef.minBB.x == 0 && blockDef.minBB.y == 0 && blockDef.minBB.z == 0 && blockDef.maxBB.x == 16 && blockDef.maxBB.y == 16 && blockDef.maxBB.z == 16)) return true;
    if(!(nextDef.minBB.x == 0 && nextDef.minBB.y == 0 && nextDef.minBB.z == 0 && nextDef.maxBB.x == 16 && nextDef.maxBB.y == 16 && nextDef.maxBB.z == 16)) return true;

    return false;
}