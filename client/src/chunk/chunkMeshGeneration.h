/**
 * Copyright (c) 2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */


#ifndef G_CHUNKMESHGEN_H
#define G_CHUNKMESHGEN_H

#include "chunk.h"
#include "../block/block.h"

//Allocate memory for a mesh.
void Chunk_AllocateMeshData(ChunkMesh *mesh, int triangleCount);
//Reallocate memory for a mesh.
void Chunk_ReAllocateMeshData(ChunkMesh *mesh, int triangleCount);

//Build/Refresh a chunk's mesh.
void Chunk_BuildMesh(Chunk *chunk);
void Chunk_AddCube(Chunk *chunk, ChunkMesh *mesh, Vector3 pos, Block blockDef);
void Chunk_AddFace(Chunk *chunk, ChunkMesh *mesh, Vector3 pos, BlockFace face, Block blockDef);
bool Chunk_TestOpaque(Block blockDef, Block nextDef);
bool Chunk_TestTranslucent(Block blockDef, Block nextDef);

#endif