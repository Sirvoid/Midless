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
    unsigned short data[CHUNK_SIZE];
    unsigned char lightData[CHUNK_SIZE];
    unsigned char sunlightData[CHUNK_SIZE];
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
//Generate a chunk's map & lightning.
void Chunk_Generate(Chunk *chunk);
//Save a chunk to a file.
void Chunk_SaveFile(Chunk *chunk);
//Load a chunk from a file.
bool Chunk_LoadFile(Chunk *chunk);
//Get a neighbour from a direction
Chunk* Chunk_GetNeighbour(Chunk* chunk, Vector3 dir);
//Update a chunk's neighbour list.
void Chunk_UpdateNeighbours(Chunk* chunk, bool leaveNeighbourhood);
//Refresh the mesh of the chunk's neighbours.
void Chunk_RefreshBorderingChunks(Chunk *chunk, bool sidesOnly);
//Check if the chunk's neighbours are generated.
bool Chunk_AreNeighbourGenerated(Chunk* chunk);
//Check if the chunk's neighbours mesh is building.
bool Chunk_AreNeighbourBuilding(Chunk* chunk);
//Set a block in a chunk and refresh mesh.
void Chunk_SetBlock(Chunk *chunk, Vector3 pos, int blockID);
//Get a block ID in a chunk.
int Chunk_GetBlock(Chunk *chunk, Vector3 pos);
//Check if block position is valid in chunk.
bool Chunk_IsValidPos(Vector3 pos);
//Convert block position to index.
int Chunk_PosToIndex(Vector3 pos);
//convert index to block position.
Vector3 Chunk_IndexToPos(int index);
//Add a chunk to a queue.
QueuedChunk *Chunk_AddToQueue(QueuedChunk *queue, Chunk* chunk);
QueuedChunk *Chunk_InsertToQueue(QueuedChunk *queue, QueuedChunk* previous, Chunk* chunk);
//Remove a chunk from a queue.
QueuedChunk *Chunk_PopFromQueue(QueuedChunk *queue);
QueuedChunk *Chunk_RemoveFromQueue(QueuedChunk *head, QueuedChunk* previous, QueuedChunk* chunk);

#endif