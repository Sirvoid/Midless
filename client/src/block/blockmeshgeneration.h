/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_BLOCK_MESH_GENERATION_H
#define MIDLESS_CLIENT_BLOCK_MESH_GENERATION_H

#include "raylib.h"
#include "block.h"
#include "chunk.h"
#include "chunkmesh.h"

typedef struct BlockMeshTemplate {
    unsigned char vertices[6][12];
    unsigned short texcoords[6][8];
} BlockMeshTemplate;

//Reset memory counters.
void BlockMesh_ResetIndexes(void);
void BlockMesh_BuildTemplates(void);
void BlockMesh_BuildTemplate(int id);

const BlockMeshTemplate *BlockMesh_GetTemplate(int blockId);

//Add a block face to a given mesh.
void BlockMesh_AddFace(unsigned char *vertices, unsigned short *indices, unsigned short *texcoords, unsigned char *colors, BlockFace face, int x, int y, int z, const Block *block, int translucent, int light, int sunlight);

//Get facing direction of a block face.
Vector3 BlockMesh_GetDirection(BlockFace face);

#endif
