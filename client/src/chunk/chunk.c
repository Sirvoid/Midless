/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include "raylib.h"
#include "raymath.h"
#include "chunk.h"
#include "chunklightning.h"
#include "chunkmeshgeneration.h"
#include "chunkmesh.h"
#include "world.h"
#include "networkhandler.h"
#include "block.h"

static void Chunk_Init(Chunk *chunk, Vector3 pos) {
    chunk->mesh = (ChunkMesh){0};
    chunk->meshTransparent = (ChunkMesh){0};
    chunk->position = pos;
    chunk->blockPosition = Vector3Multiply(chunk->position, CHUNK_SIZE_VEC3);
    chunk->fromFile = false;
    chunk->isBuilt = false;
    chunk->isBlockDataReady = false;
    chunk->isLightGenerated = false;
    chunk->isGenerating = false;
    chunk->hasTransparency = false;
    chunk->onlyAir = true;
    chunk->modified = false;
    chunk->incompleteLightFaces = 0;
    chunk->incompleteSunlightFaces = 0;
    chunk->isLightDirty = false;

    memset(chunk->lightData, 0, sizeof(chunk->lightData));
    memset(chunk->sunlightData, 0, sizeof(chunk->sunlightData));
    memset(chunk->skyMask, 0, sizeof(chunk->skyMask));
 
    if (Chunk_LoadFile(chunk)) {
        chunk->fromFile = true;
    }

    Chunk_UpdateNeighbours(chunk, false);
}

Chunk *Chunk_Create(Vector3 pos) {
    Chunk *chunk = MemAlloc(sizeof(*chunk));
    if (chunk == NULL) return NULL;

    Chunk_Init(chunk, pos);
    return chunk;
}

void Chunk_SaveFile(Chunk *chunk) {
    if (networkConnectedToServer) return;
    
    const char* fileName = TextFormat("world/%i.%i.%i.dat", (int)chunk->position.x, (int)chunk->position.y, (int)chunk->position.z);
    int compressedLength;
    unsigned short* compressed = Chunk_CreateCompressedData(chunk, &compressedLength);
    SaveFileData(fileName, compressed, compressedLength * 2);
    MemFree(compressed);
}

bool Chunk_LoadFile(Chunk *chunk) {
    if (networkConnectedToServer) return false;

    const char* fileName = TextFormat("world/%i.%i.%i.dat", (int)chunk->position.x, (int)chunk->position.y, (int)chunk->position.z);
    if (FileExists(fileName)) {
        unsigned int length = 0;
        unsigned char *saveFile = LoadFileData(fileName, &length);
        Chunk_Decompress(chunk, (unsigned short*)saveFile, length / 2);
        UnloadFileData(saveFile);
        return true;
    }
    return false;
}

void Chunk_Decompress(Chunk *chunk, unsigned short *compressed, int compressedLength) {
    ChunkData_Decompress(chunk->data, compressed, compressedLength);
}

unsigned short* Chunk_CreateCompressedData(Chunk *chunk, int *compressedLength) {
    return ChunkData_CreateCompressed(chunk->data, compressedLength);
}

void Chunk_Unload(Chunk *chunk) {
    if (chunk == NULL) return;

    ChunkMesh_Unload(&chunk->mesh);
    ChunkMesh_Unload(&chunk->meshTransparent);
    chunk->isBuilt = false;
}

void Chunk_Destroy(Chunk *chunk) {
    if (chunk == NULL) return;
    MemFree(chunk);
}


void Chunk_Generate(Chunk *chunk) {
    if (chunk == NULL || chunk->isLightGenerated) return;

    chunk->isBlockDataReady = true;

    // Initial sunlight calculated from the top down.
    Chunk *topChunk = chunk->neighbours[BLOCK_FACE_TOP];
    if (topChunk != NULL && !topChunk->isLightGenerated) {
        Chunk_Generate(topChunk);
    }

    Chunk_DoSunlight(chunk);
    Chunk_DoLightSources(chunk);
    chunk->isLightGenerated = true;
    Chunk_ReconcileLighting(chunk);
}



void Chunk_SetBlock(Chunk *chunk, Vector3 pos, int blockId) {
    if (Chunk_IsValidPos(pos)) {
        int index = Chunk_PosToIndex(pos);
        if (chunk->data[index] == blockId) return;

        chunk->data[index] = blockId;
        chunk->modified = true;

        const Block *blockDef = Block_GetDefinition(blockId);

        Chunk_RemoveSunlight(chunk, pos);

        if (blockDef->lightType == BLOCK_LIGHT_EMIT) {
            Chunk_AddLightSource(chunk,pos, 15, false);
        } else {
            Chunk_RemoveLightSource(chunk,pos);
        }

    }
}

int Chunk_GetBlock(Chunk *chunk, Vector3 pos) {
    if (Chunk_IsValidPos(pos)) {
        return chunk->data[Chunk_PosToIndex(pos)];
    }
    return 0;
}

void Chunk_UpdateNeighbours(Chunk* chunk, bool leaveNeighbourhood) {

    Vector3 directions[26] = {
        {-1, 0, 0},
        {1, 0, 0},
        {0, 1, 0},
        {0, -1, 0},
        {0, 0, 1},
        {0, 0, -1},
        {-1, -1, -1},
        {1, 1, 1},
        {-1, -1, 0},
        {1, 1, 0},
        {-1, -1, 1},
        {1, 1, -1},
        {-1, 0, -1},
        {1, 0, 1},
        {-1, 0, 1},
        {1, 0, -1},
        {-1, 1, -1},
        {1, -1, 1},
        {-1, 1, 0},
        {1, -1, 0},
        {-1, 1, 1},
        {1, -1, -1},
        {0, -1, -1},
        {0, 1, 1},
        {0, -1, 1},
        {0, 1, -1}
    };

    if (leaveNeighbourhood) {
        for (int i = 0; i < 26; i++) {
            Chunk *neighbour = chunk->neighbours[i];

            if (neighbour != NULL) {
                int j = i;
                if(i % 2 == 0) { 
                    j = i + 1;
                } else {
                    j = i - 1;
                }

                neighbour->neighbours[j] = NULL;
                if (i < 6) {
                    neighbour->incompleteLightFaces |= (unsigned char)(1u << j);
                    neighbour->incompleteSunlightFaces |= (unsigned char)(1u << j);
                }
            }
        }
    } else {
        for (int i = 0; i < 26; i++) {
            Chunk *borderingChunk = World_GetChunkAt(Vector3Add(chunk->position, directions[i]));

            if (borderingChunk != NULL) {
                int j = i;
                if (i % 2 == 0) { 
                    j = i + 1;
                } else {
                    j = i - 1;
                }

                borderingChunk->neighbours[j] = chunk;
                chunk->neighbours[i] = borderingChunk;
            } else {
                chunk->neighbours[i] = NULL;
            }

            
        }
    }

}

bool Chunk_AreNeighbourGenerated(Chunk* chunk) {
    int i = 0;
    for (i = 0; i < 6; i++) {
        if (chunk->neighbours[i] != NULL) {
            if (chunk->neighbours[i]->isLightGenerated == false) return false;
        }
    }
    return true;
}

bool Chunk_IsValidPos(Vector3 pos) {
    return ChunkData_IsValidPosition((int)pos.x, (int)pos.y, (int)pos.z);
}

Vector3 Chunk_IndexToPos(int index) {
    int x, y, z;
    ChunkData_IndexToPosition(index, &x, &y, &z);
    return (Vector3){x, y, z};
}

int Chunk_PosToIndex(Vector3 pos) {
    return ChunkData_PositionToIndex((int)pos.x, (int)pos.y, (int)pos.z);
}

long int Chunk_GetPackedPos(Vector3 pos) {
    return ChunkData_PackPosition((int)pos.x, (int)pos.y, (int)pos.z);
}
