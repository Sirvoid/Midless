/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_CHUNK_H
#define MIDLESS_CLIENT_CHUNK_H

#include "raylib.h"
#include "chunkmesh.h"
#include "chunkdata.h"

#define CHUNK_SIZE_VEC3 CLITERAL(Vector3){ CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z }

typedef struct Chunk{
    ChunkMesh mesh;
    ChunkMesh meshTransparent;
    unsigned short data[CHUNK_SIZE];
    unsigned char lightData[CHUNK_SIZE];
    unsigned char sunlightData[CHUNK_SIZE];
    unsigned char skyMask[CHUNK_SKY_MASK_SIZE];
    int step;
    Vector3 position; //Position of the chunk in chunk unit
    Vector3 blockPosition; //Position of the chunk in block unit
    struct Chunk *neighbours[26];

    //Loading/Generation flags
    bool isBuilt;
    bool isGenerating;
    bool isMapGenerated;
    bool isLightGenerated;
    bool fromFile;
    bool modified;

    //mesh flags
    bool hasTransparency;
    bool onlyAir;
} Chunk;

typedef struct LightNode{
    int index;
    Chunk *chunk;
    struct LightNode *next;
} LightNode;

typedef struct LightRemovalNode{
    int index;
    int val;
    Chunk *chunk;
    struct LightRemovalNode *next;
} LightRemovalNode;

//Allocate and initialize a chunk.
Chunk *Chunk_Create(Vector3 pos);
//Unload the chunk's external resources.
void Chunk_Unload(Chunk *chunk);
//Unload the chunk.
void Chunk_Destroy(Chunk *chunk);
//Generate a chunk's map & lightning.
void Chunk_Generate(Chunk *chunk);
//Save a chunk to a file.
void Chunk_SaveFile(Chunk *chunk);
//Load a chunk from a file.
bool Chunk_LoadFile(Chunk *chunk);
//Decompress chunk
void Chunk_Decompress(Chunk *chunk, unsigned short *compressed, int compressedLength);
//Create compressed chunk data.
unsigned short* Chunk_CreateCompressedData(Chunk *chunk, int *compressedLength);
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
void Chunk_SetBlock(Chunk *chunk, Vector3 pos, int blockId);
//Get a block id in a chunk.
int Chunk_GetBlock(Chunk *chunk, Vector3 pos);
//Check if block position is valid in chunk.
bool Chunk_IsValidPos(Vector3 pos);
//Convert block position to index.
int Chunk_PosToIndex(Vector3 pos);
//convert index to block position.
Vector3 Chunk_IndexToPos(int index);
//Pack chunk position into a 32 bit number.
long int Chunk_GetPackedPos(Vector3 pos);

#endif
