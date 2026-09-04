/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SERVER_CHUNK_H
#define MIDLESS_SERVER_CHUNK_H

#include "raylib.h"
#include "../../player.h"
#include "chunkdata.h"

#define CHUNK_SIZE_VEC3 CLITERAL(Vector3){ CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z }

typedef struct Chunk{
    unsigned short data[CHUNK_SIZE];
    unsigned char skyMask[CHUNK_SKY_MASK_SIZE];
    Vector3 position; //Position of the chunk in chunk unit
    Vector3 blockPosition; //Position of the chunk in block unit
    bool fromFile;
    bool modified;
    Player* *players;
} Chunk;

//Allocate and initialize a chunk.
Chunk *ServerChunk_Create(Vector3 pos);
//Unload the chunk.
void ServerChunk_Destroy(Chunk *chunk);

void ServerChunk_Decompress(Chunk *chunk, unsigned short *compressed, int compressedLength);
//Create compressed chunk data.
unsigned short* ServerChunk_CreateCompressedData(Chunk *chunk, int *compressedLength);
void ServerChunk_SaveFile(Chunk *chunk);
bool ServerChunk_LoadFile(Chunk *chunk);
void ServerChunk_Generate(Chunk *chunk);

bool ServerChunk_PlayerInChunk(Chunk* chunk, Player* player);
void ServerChunk_AddPlayer(Chunk* chunk, Player* player);
void ServerChunk_RemovePlayer(Chunk* chunk, int index);

void ServerChunk_SetBlock(Chunk *chunk, Vector3 pos, int blockId);
int ServerChunk_GetBlock(Chunk *chunk, Vector3 pos);

bool ServerChunk_IsValidPos(Vector3 pos);
Vector3 ServerChunk_IndexToPos(int index);
int ServerChunk_PosToIndex(Vector3 pos);
long int ServerChunk_GetPackedPos(Vector3 pos);

#endif
