/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <string.h>
#include "raylib.h" 
#include "raymath.h"
#include "chunk.h"
#include "chunkLightning.h"
#include "chunkMeshGeneration.h"
#include "world.h"
#include "chunkMesh.h"
#include "worldgenerator.h"
#include "networkhandler.h"
#include "../block/block.h"

void Chunk_Init(Chunk *chunk, Vector3 pos) {
    chunk->position = pos;
    chunk->blockPosition = Vector3Multiply(chunk->position, CHUNK_SIZE_VEC3);
    chunk->mesh = MemAlloc(sizeof(ChunkMesh));
    chunk->meshTransparent = MemAlloc(sizeof(ChunkMesh));
    chunk->fromFile = false;
    chunk->isBuilt = false;
    chunk->isBuilding = false;
    chunk->isMapGenerated = false;
    chunk->isLightGenerated = false;
    chunk->hasStartedGenerating = false;
    chunk->mesh->vaoId = 0;
    chunk->meshTransparent->vaoId = 0;

    for(int i = 0; i < CHUNK_SIZE; i++) {
        chunk->lightData[i] = 0;
        chunk->sunlightData[i] = 0;
    }
 
    if(Chunk_LoadFile(chunk)) {
        chunk->fromFile = true;
    }

    Chunk_UpdateNeighbours(chunk, false);
}

void Chunk_SaveFile(Chunk *chunk) {
    if(Network_connectedToServer) return;
    const char* fileName = TextFormat("world/%i.%i.%i.dat", (int)chunk->position.x, (int)chunk->position.y, (int)chunk->position.z);
    SaveFileData(fileName, chunk->data, CHUNK_SIZE * 2);
}

bool Chunk_LoadFile(Chunk *chunk) {
    const char* fileName = TextFormat("world/%i.%i.%i.dat", (int)chunk->position.x, (int)chunk->position.y, (int)chunk->position.z);
    if(FileExists(fileName)) {

        unsigned int length = 0;
        unsigned char *saveFile = LoadFileData(fileName, &length);

        memcpy(chunk->data, &saveFile[0], length);

        UnloadFileData(saveFile);
        return true;
    }
    return false;
}

void Chunk_Unload(Chunk *chunk) {

    if(chunk->isBuilt) {
        ChunkMesh_Unload(chunk->mesh);
        ChunkMesh_Unload(chunk->meshTransparent);

        MemFree(chunk->mesh);
        MemFree(chunk->meshTransparent);
    }

    Chunk_UpdateNeighbours(chunk, true);
}


void Chunk_Generate(Chunk *chunk) {
    if(!chunk->isLightGenerated) {
        if(!chunk->fromFile) {
            //Map Generation
            for(int i = CHUNK_SIZE - 1; i >= 0; i--) {
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
    if(Chunk_IsValidPos(pos)) {
        int index = Chunk_PosToIndex(pos);

        chunk->data[index] = blockID;
        Chunk_SaveFile(chunk);

        Block blockDef = Block_GetDefinition(blockID);
        if(blockDef.lightType == BlockLightType_Emit) {
            Chunk_AddLightSource(chunk,pos, 15, false);
        } else {
            Chunk_RemoveLightSource(chunk,pos);
            Chunk_RemoveSunlight(chunk,pos);
        }

    }
}

int Chunk_GetBlock(Chunk *chunk, Vector3 pos) {
    if(Chunk_IsValidPos(pos)) {
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
    for(int i = 0; i < 26; i++) {
        if(directions[i].x == dir.x && directions[i].y == dir.y && directions[i].z == dir.z) {
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

    if(leaveNeighbourhood) {
        for(int i = 0; i < 26; i++) {
            Chunk *neighbour = chunk->neighbours[i];
            if(neighbour != NULL) {
                
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
        for(int i = 0; i < 26; i++) {
            Chunk *borderingChunk = World_GetChunkAt(Vector3Add(chunk->position, directions[i]));

            if(borderingChunk != NULL) {
                int j = i;
                if(i % 2 == 0) { 
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
     if(!sidesOnly) nb = 26;

     for(int i = 0; i < nb; i++) {
        if(chunk->neighbours[i] == NULL) continue;
        if(!chunk->neighbours[i]->isBuilt) continue;
        World_QueueChunk(chunk->neighbours[i]);
     }
}

bool Chunk_AreNeighbourGenerated(Chunk* chunk) {
    int i = 0;
    for(i = 0; i < 6; i++) {
        if(chunk->neighbours[i] != NULL) {
            if(chunk->neighbours[i]->isLightGenerated == false) return false;
        }
    }
    return true;
}

bool Chunk_AreNeighbourBuilding(Chunk* chunk) {
    for(int i = 0; i < 26; i++) {
        if(chunk->neighbours[i] != NULL) {
            if(i == 2) {
                Chunk *top = chunk->neighbours[2];
                while(top != NULL) {
                    if(!top->isBuilt || !top->isBuilding) return true;
                    top = top->neighbours[2];
                }
            } else {
                if(!chunk->neighbours[i]->isBuilt || chunk->neighbours[i]->isBuilding) return true;
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

/*-------------------------------------------------------------------------------------------------------*
*-------------------------------------------Chunk Queue----------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/

QueuedChunk *Chunk_AddToQueue(QueuedChunk *queue, Chunk* chunk) {

    QueuedChunk *head = queue;

    if(queue != NULL) {
        while(queue->next != NULL) {
            queue = queue->next;
        }
        queue->next = MemAlloc(sizeof(QueuedChunk));
        queue = queue->next;
    } else {
        queue = MemAlloc(sizeof(QueuedChunk));
        head = queue;
    }

    queue->chunk = chunk;
    queue->next = NULL;

    return head;
}

QueuedChunk *Chunk_InsertToQueue(QueuedChunk *queue, QueuedChunk* previous, Chunk* chunk) {

    QueuedChunk *queued =  MemAlloc(sizeof(QueuedChunk));
    queued->chunk = chunk;
    queued->next = NULL;

    if(previous == NULL) {
        if(queue != NULL) queued->next = queue;
        return queued;
    } else {
        if(previous->next != NULL) queued->next = previous->next;
        previous->next = queued;
    }

    return queue;
}

QueuedChunk *Chunk_PopFromQueue(QueuedChunk *queue) {
    if(queue == NULL) return NULL;

    QueuedChunk *node = queue->next;
    MemFree(queue);
    return node;
}

QueuedChunk *Chunk_RemoveFromQueue(QueuedChunk *head, QueuedChunk* previous, QueuedChunk* chunk) {
    if(head == NULL) return NULL;

    QueuedChunk *next = chunk->next;
    MemFree(chunk);
    if(chunk == head) return next;
    previous->next = next;
   
    return head;
}
