/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_CHUNK_H
#define G_CHUNK_H

#include "raylib.h"

#define CHUNK_SIZE_X 16
#define CHUNK_SIZE_Y 16
#define CHUNK_SIZE_Z 16
#define CHUNK_SIZE_XZ (CHUNK_SIZE_X * CHUNK_SIZE_Z)
#define CHUNK_SIZE_VEC3 CLITERAL(Vector3){ CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z }
#define CHUNK_SIZE (CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z)

typedef struct Chunk{
    unsigned short data[CHUNK_SIZE];
    Vector3 position; //Position of the chunk in chunk unit
    Vector3 blockPosition; //Position of the chunk in block unit
    bool fromFile;
} Chunk;

void Chunk_Init(Chunk *chunk, Vector3 pos);
void Chunk_Unload(Chunk *chunk);

unsigned short* Chunk_Compress(Chunk *chunk, int currentLength, int *newLength);
void Chunk_SaveFile(Chunk *chunk);
bool Chunk_LoadFile(Chunk *chunk);
void Chunk_Generate(Chunk *chunk);

void Chunk_SetBlock(Chunk *chunk, Vector3 pos, int blockID);
int Chunk_GetBlock(Chunk *chunk, Vector3 pos);

bool Chunk_IsValidPos(Vector3 pos);
Vector3 Chunk_IndexToPos(int index);
int Chunk_PosToIndex(Vector3 pos);

#endif