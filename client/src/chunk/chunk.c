/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "raylib.h"
#include "raymath.h"
#include "chunk.h"
#include "chunklightning.h"
#include "chunkmeshgeneration.h"
#include "chunkmesh.h"
#include "world.h"
#include "worldgenerator.h"
#include "networkhandler.h"
#include "block.h"

void Chunk_Init(Chunk *chunk, Vector3 pos) {
    chunk->position = pos;
    chunk->blockPosition = Vector3Multiply(chunk->position, CHUNK_SIZE_VEC3);
    chunk->fromFile = false;
    chunk->isBuilt = false;
    chunk->isBuilding = false;
    chunk->isMapGenerated = false;
    chunk->isLightGenerated = false;
    chunk->hasStartedGenerating = false;
    chunk->hasTransparency = false;
    chunk->onlyAir = true;
    chunk->beingDeleted = false;
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

void Chunk_SaveFile(Chunk *chunk) {
    if (Network_connectedToServer) return;
    
    const char* fileName = TextFormat("world/%i.%i.%i.dat", (int)chunk->position.x, (int)chunk->position.y, (int)chunk->position.z);
    int newLength;
    unsigned short* compressed = Chunk_Compress(chunk, CHUNK_SIZE, &newLength);
    SaveFileData(fileName, compressed, newLength * 2);
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
    int newLength = 0;

    for (int i = 0; i < currentLength; i+=2) {
        for (int j = 0; j < compressed[i + 1]; j++) {
            chunk->data[newLength] = compressed[i];
            newLength += 1;
        } 
    }
    
}

unsigned short* Chunk_Compress(Chunk *chunk, int currentLength, int *newLength) {
    
    //BlockID:UShort, Amount:UShort, ...
    
    unsigned short *compressed = MemAlloc(currentLength * 2 * 2);
    
    int oldID = chunk->data[0];
    int bCount = 1;
    int len = 0;
    for (int i = 1; i <= currentLength; i++) {
        
        int curID = 0;
        if (i != currentLength) curID = chunk->data[i];
        
        if (oldID != curID || bCount >= USHRT_MAX || i == currentLength) {
            compressed[len++] = (unsigned short)oldID;
            compressed[len++] = (unsigned short)bCount;

            bCount = 0;
            oldID = curID;
        }
        
        bCount++;
        
    }
    
    *newLength = len;
    
    compressed = MemRealloc(compressed, *newLength * 2);
    return compressed;
}

void Chunk_Unload(Chunk *chunk) {

    if (chunk->modified) Chunk_SaveFile(chunk);

    if (chunk->isBuilt) {
        ChunkMesh_Unload(&chunk->mesh);
        ChunkMesh_Unload(&chunk->meshTransparent);
    }

    Chunk_UpdateNeighbours(chunk, true);
    MemFree(chunk);
}


void Chunk_Generate(Chunk *chunk) {
    if (!chunk->isMapGenerated) {
        if (!chunk->fromFile && !Network_connectedToServer) {
            //Map Generation
            for (int i = CHUNK_SIZE - 1; i >= 0; i--) {
                Vector3 npos = Vector3Add(Chunk_IndexToPos(i), chunk->blockPosition);
                chunk->data[i] = WorldGenerator_Generate(chunk, npos, i);
            }
        }

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

        Block blockDef = Block_GetDefinition(blockID);
        if (blockDef.lightType == BlockLightType_Emit) {
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
                    if (!top->isBuilt || top->isBuilding) return true;
                    top = top->neighbours[2];
                }
            } else {
                if (!chunk->neighbours[i]->isBuilt || chunk->neighbours[i]->isBuilding) return true;
            }
        }
    }
    return false;
}

bool Chunk_IsValidPos(Vector3 pos) {
    return pos.x >= 0 && pos.x < CHUNK_SIZE_X && pos.y >= 0 && pos.y < CHUNK_SIZE_Y && pos.z >= 0 && pos.z < CHUNK_SIZE_Z;
}

Vector3 Chunk_IndexToPos(int index) {
    int x = (index % CHUNK_SIZE_X);
	int y = (index / CHUNK_SIZE_XZ);
	int z = (index / CHUNK_SIZE_X) % CHUNK_SIZE_Z;
    return (Vector3){x, y, z};
}

int Chunk_PosToIndex(Vector3 pos) {
    return ((int)pos.y * CHUNK_SIZE_Z + (int)pos.z) * CHUNK_SIZE_X + (int)pos.x;
}

long int Chunk_GetPackedPos(Vector3 pos) {
    return (long)((int)(pos.x)&4095)<<20 | (long)((int)(pos.z)&4095)<<8 | (long)((int)(pos.y)&255);
}

