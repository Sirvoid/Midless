/**
 * Copyright (c) 2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */


#ifndef MIDLESS_CLIENT_CHUNK_MESH_GENERATION_H
#define MIDLESS_CLIENT_CHUNK_MESH_GENERATION_H

#include "chunk.h"
#include "block.h"

void ChunkMeshGeneration_Init(void);
void ChunkMeshGeneration_Shutdown(void);
//Build/Refresh a chunk's mesh.
void ChunkMeshGeneration_Build(Chunk *chunk);
bool ChunkMeshGeneration_IsOpaqueFaceVisible(const Block *blockDef, const Block *nextDef);
bool ChunkMeshGeneration_IsTranslucentFaceVisible(const Block *blockDef, const Block *nextDef);

#endif
