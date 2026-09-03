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
    chunk->isMapGenerated = false;
    chunk->isLightGenerated = false;
    chunk->isGenerating = false;
    chunk->hasTransparency = false;
    chunk->onlyAir = true;
    chunk->modified = false;

    for (int i = 0; i < CHUNK_SIZE; i++) {
        chunk->lightData[i] = 0;
        chunk->sunlightData[i] = 0;
    }
 
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
    if (Network_connectedToServer) return;
    
    const char* fileName = TextFormat("world/%i.%i.%i.dat", (int)chunk->position.x, (int)chunk->position.y, (int)chunk->position.z);
    int newLength;
    unsigned short* compressed = Chunk_CreateCompressedData(chunk, &newLength);
    SaveFileData(fileName, compressed, newLength * 2);
    MemFree(compressed);
}

bool Chunk_LoadFile(Chunk *chunk) {
    if (Network_connectedToServer) return false;

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

void Chunk_Decompress(Chunk *chunk, unsigned short *compressed, int currentLength) {
    ChunkData_Decompress(chunk->data, compressed, currentLength);
}

unsigned short* Chunk_CreateCompressedData(Chunk *chunk, int *newLength) {
    return ChunkData_CreateCompressed(chunk->data, newLength);
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
    if (!chunk->isMapGenerated) {
        chunk->isMapGenerated = true;
        Chunk_DoSunlight(chunk);
        Chunk_DoLightSources(chunk);
        chunk->isLightGenerated = true;
    }
}



void Chunk_SetBlock(Chunk *chunk, Vector3 pos, int blockID) {
    if (Chunk_IsValidPos(pos)) {
        int index = Chunk_PosToIndex(pos);

        chunk->data[index] = blockID;
        chunk->modified = true;

        const Block *blockDef = Block_GetDefinition(blockID);
        if (blockDef->lightType == BlockLightType_Emit) {
            Chunk_AddLightSource(chunk,pos, 15, false);
        } else {
            Chunk_RemoveLightSource(chunk,pos);
            Chunk_RemoveSunlight(chunk,pos);
        }

    }
}

int Chunk_GetBlock(Chunk *chunk, Vector3 pos) {
    if (Chunk_IsValidPos(pos)) {
        return chunk->data[Chunk_PosToIndex(pos)];
    }
    return 0;
}

Chunk* Chunk_GetNeighbour(Chunk* chunk, Vector3 dir) {
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

    int index = 0;
    for (int i = 0; i < 26; i++) {
        if (directions[i].x == dir.x && directions[i].y == dir.y && directions[i].z == dir.z) {
            index = i;
            break;
        }
    }

    return chunk->neighbours[index];
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

void Chunk_RefreshBorderingChunks(Chunk *chunk, bool sidesOnly) {

     int nb = 6;
     if (!sidesOnly) nb = 26;

     for (int i = 0; i < nb; i++) {
        if (chunk->neighbours[i] == NULL) continue;
        if (!chunk->neighbours[i]->isBuilt) continue;
        Chunk_BuildMesh(chunk->neighbours[i]);
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

bool Chunk_AreNeighbourBuilding(Chunk* chunk) {
    for (int i = 0; i < 26; i++) {
        if (chunk->neighbours[i] != NULL) {
            if (i == 2) {
                Chunk *top = chunk->neighbours[2];
                while (top != NULL) {
                    if (!top->isBuilt) return true;
                    top = top->neighbours[2];
                }
            } else {
                if (!chunk->neighbours[i]->isBuilt) return true;
            }
        }
    }
    return false;
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
