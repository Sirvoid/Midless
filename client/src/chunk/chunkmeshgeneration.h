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

#include "blockmeshgeneration.h"

#define MESH_SNAPSHOT_CELLS (CHUNK_SIZE + 6 * CHUNK_SIZE_XZ)

typedef struct MeshCell {
    unsigned short block;
    unsigned char light, sky;
} MeshCell;

typedef struct MeshSnapshot {
    MeshCell cells[MESH_SNAPSHOT_CELLS];
    bool neighbors[6];
} MeshSnapshot;

typedef struct MeshBank {
    unsigned char *vertices, *colors;
    unsigned short *indices, *texcoords;
    int vertexCount, capacity;
} MeshBank;

typedef struct MeshBuffers {
    MeshBank banks[2];
    bool onlyAir, valid;
} MeshBuffers;

MeshBuffers *ChunkMeshGeneration_CreateBuffers(void);
void ChunkMeshGeneration_FreeBuffers(MeshBuffers *buffers);
void ChunkMeshGeneration_Compute(MeshBuffers *buffers, MeshSnapshot *chunk,
    const Block *definitions, const BlockMeshTemplate *templates);
void ChunkMeshGeneration_Upload(MeshBuffers *buffers, Chunk *chunk);
bool ChunkMeshGeneration_IsOpaqueFaceVisible(const Block *blockDef, const Block *nextDef);
bool ChunkMeshGeneration_IsTranslucentFaceVisible(const Block *blockDef, const Block *nextDef);

#endif
