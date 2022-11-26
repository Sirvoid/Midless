/**
 * Copyright (c) 2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */


#ifndef G_CHUNKMESHGEN_H
#define G_CHUNKMESHGEN_H

#include "chunk.h"
#include "block.h"

void Chunk_MeshGenerationInit(void);
//Build/Refresh a chunk's mesh.
void Chunk_BuildMesh(Chunk *chunk);
void Chunk_AddCube(Chunk *chunk, ChunkMesh *mesh, Vector3 pos, Block blockDef);
void Chunk_AddFace(Chunk *chunk, ChunkMesh *mesh, Vector3 pos, BlockFace face, Block blockDef);
bool Chunk_TestOpaque(Block blockDef, Block nextDef);
bool Chunk_TestTranslucent(Block blockDef, Block nextDef);

#endif