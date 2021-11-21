/**
 * Copyright (c) 2021 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_CHUNK_H
#define G_CHUNK_H

#include "blockfacehelper.h"
#include "raylib.h"
#include "chunkmesh.h"

#define CHUNK_SIZE_X 16
#define CHUNK_SIZE_Y 16
#define CHUNK_SIZE_Z 16
#define CHUNK_SIZE_XZ (CHUNK_SIZE_X * CHUNK_SIZE_Z)
#define CHUNK_SIZE_VEC3 CLITERAL(Vector3){ CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z }
#define CHUNK_SIZE (CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z)

typedef struct Chunk{
    ChunkMesh *mesh;
    ChunkMesh *meshTransparent;
    int loaded; //Chunk loaded once
    int index;
    Vector3 position; //Position of the chunk in chunk unit
    Vector3 blockPosition; //Position of the chunk in block unit
} Chunk;

//Initialize a chunk.
void Chunk_Init(Chunk *chunk, Vector3 pos);
//Allocate memory for a mesh.
void Chunk_AllocateMeshData(ChunkMesh *mesh, int triangleCount);
//Reallocate memory for a mesh.
void Chunk_ReAllocateMeshData(ChunkMesh *mesh, int triangleCount);
//Unload a chunk.
void Chunk_Unload(Chunk *chunk);

//Build/Refresh a chunk's mesh.
void Chunk_BuildMesh(Chunk *chunk);
void Chunk_AddCube(Chunk *chunk, ChunkMesh *mesh, Vector3 pos, Vector3 worldPos, Block blockDef);
void Chunk_AddFace(Chunk *chunk, ChunkMesh *mesh, Vector3 pos, Vector3 worldPos, BlockFace face, Block blockDef);

void Chunk_RefreshBorderingChunks(Chunk *chunk, Vector3 blockPos);

//convert index to block position.
Vector3 Chunk_IndexToPos(int index);

#endif