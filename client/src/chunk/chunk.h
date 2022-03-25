/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_CHUNK_H
#define G_CHUNK_H

#include "raylib.h"
#include "chunkMesh.h"

#define CHUNK_SIZE_X 16
#define CHUNK_SIZE_Y 16
#define CHUNK_SIZE_Z 16
#define CHUNK_SIZE_XZ (CHUNK_SIZE_X * CHUNK_SIZE_Z)
#define CHUNK_SIZE_VEC3 CLITERAL(Vector3){ CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z }
#define CHUNK_SIZE (CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z)

typedef struct Chunk{
    ChunkMesh *mesh;
    ChunkMesh *meshTransparent;
    int data[CHUNK_SIZE];
    int lightData[CHUNK_SIZE];
    int sunlightData[CHUNK_SIZE];
    int step;
    Vector3 position; //Position of the chunk in chunk unit
    Vector3 blockPosition; //Position of the chunk in block unit
    struct Chunk *nextChunk;
    struct Chunk *previousChunk;
    struct Chunk *neighbours[26];

    //Loading/Generation flags
    bool isBuilding;
    bool isBuilt;
    bool hasStartedGenerating;
    bool isMapGenerated;
    bool isLightGenerated;
    bool fromFile;
} Chunk;

typedef struct QueuedChunk {
    Chunk *chunk;
    struct QueuedChunk *next;
    int state;
} QueuedChunk;

typedef struct LightNode{
    int index;
    Chunk *chunk;
    struct LightNode *next;
} LightNode;

typedef struct LightDelNode{
    int index;
    int val;
    Chunk *chunk;
    struct LightDelNode *next;
} LightDelNode;

//Initialize a chunk.
void Chunk_Init(Chunk *chunk, Vector3 pos);
//Unload a chunk.
void Chunk_Unload(Chunk *chunk);

void Chunk_Generate(Chunk *chunk);
void Chunk_SaveFile(Chunk *chunk);
bool Chunk_LoadFile(Chunk *chunk);

void Chunk_UpdateNeighbours(Chunk* chunk, bool leaveNeighbourhood);
void Chunk_RefreshBorderingChunks(Chunk *chunk, bool sidesOnly);

//Set a block in a chunk and refresh mesh.
void Chunk_SetBlock(Chunk *chunk, Vector3 pos, int blockID);

//Get a block ID in a chunk.
int Chunk_GetBlock(Chunk *chunk, Vector3 pos);

//Check if block position is valid in chunk.
int Chunk_IsValidPos(Vector3 pos);

//Convert block position to index.
int Chunk_PosToIndex(Vector3 pos);

//convert index to block position.
Vector3 Chunk_IndexToPos(int index);

//Chunk Queues
QueuedChunk *Chunk_AddToQueue(QueuedChunk *queue, Chunk* chunk);
QueuedChunk *Chunk_PopFromQueue(QueuedChunk *queue);

#endif