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

unsigned char *vertices;
unsigned short *indices;
unsigned short *texcoords;
unsigned char *colors;

unsigned char *verticesT;
unsigned short *indicesT;
unsigned short *texcoordsT;
unsigned char *colorsT;

void Chunk_MeshGenerationInit(void) {

    int vertexCount = 2 * 6 * CHUNK_SIZE * 2;
    int triangleCount = 2 * 6 * CHUNK_SIZE;

    vertices = MemAlloc(vertexCount * 3);
    texcoords = MemAlloc(vertexCount * 2 * sizeof(unsigned short));
    colors = MemAlloc(vertexCount);
    indices = MemAlloc(triangleCount * 3 * sizeof(unsigned short));

    verticesT = MemAlloc(vertexCount * 3);
    texcoordsT = MemAlloc(vertexCount * 2 * sizeof(unsigned short));
    colorsT = MemAlloc(vertexCount);
    indicesT = MemAlloc(triangleCount * 3 * sizeof(unsigned short));
}


void Chunk_BuildMesh(Chunk *chunk) {

    if(chunk->isBuilt == true) {
        ChunkMesh_Unload(&chunk->mesh);
        ChunkMesh_Unload(&chunk->meshTransparent);
    }

    BlockMesh_ResetIndexes();
    Chunk_triangleCounter = 0;
    Chunk_triangleCounterTransparent = 0;
    chunk->hasTransparency = false;
    chunk->onlyAir = true;

    for(int i = 0; i < CHUNK_SIZE; i++) {
        int blockID = chunk->data[i];
        Block blockDef = Block_GetDefinition(blockID);
        if(blockDef.modelType == BlockModelType_Gas) continue;
        chunk->onlyAir = false;

        Vector3 pos = Chunk_IndexToPos(i);
        bool translucent = blockDef.renderType == BlockRenderType_Translucent;

        if(translucent) {
            chunk->hasTransparency = true;
            Chunk_AddCube(chunk, &chunk->meshTransparent, pos, blockDef);
        } else {
            Chunk_AddCube(chunk, &chunk->mesh, pos, blockDef);
        }
    }
    
    chunk->mesh.vertexCount = Chunk_triangleCounter * 2;
    chunk->mesh.triangleCount = Chunk_triangleCounter;

    chunk->meshTransparent.vertexCount = Chunk_triangleCounterTransparent * 2;
    chunk->meshTransparent.triangleCount = Chunk_triangleCounterTransparent;

    ChunkMesh_Upload(&chunk->mesh, vertices, indices, texcoords, colors);
    ChunkMesh_Upload(&chunk->meshTransparent, verticesT, indicesT, texcoordsT, colorsT);

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
    int sunLightLevel = 0;
    if(sprite || renderType == BlockRenderType_Transparent) {
        lightLevel = Chunk_GetLight(chunk, pos, false);
        sunLightLevel = Chunk_GetLight(chunk, pos, true);
    } else {
        lightLevel = Chunk_GetLight(nextChunk, nextPos, false);
        sunLightLevel = Chunk_GetLight(nextChunk, nextPos, true);
    }
    
    //Build face
    if(bTest || sprite) {
        if(renderType == BlockRenderType_Translucent) { 
            Chunk_triangleCounterTransparent += 2;
            BlockMesh_AddFace(verticesT, indicesT, texcoordsT, colorsT, face, pos, blockDef, 1, lightLevel, sunLightLevel);
        } else {
            Chunk_triangleCounter += 2;
            BlockMesh_AddFace(vertices, indices, texcoords, colors, face, pos, blockDef, 0, lightLevel, sunLightLevel);
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