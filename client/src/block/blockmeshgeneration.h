/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_BLOCKFACEHELPER_H
#define G_BLOCKFACEHELPER_H

#include "raylib.h"
#include "block.h"
#include "chunk.h"
#include "chunkmesh.h"

//Reset memory counters.
void BlockMesh_ResetIndexes(void);
void BlockMesh_BuildTemplates(void);

//Add a block face to a given mesh.
void BlockMesh_AddFace(unsigned char *vertices, unsigned short *indices, unsigned short *texcoords, unsigned char *colors, BlockFace face, int x, int y, int z, const Block *block, int translucent, int light, int sunlight);

//Get facing direction of a block face.
Vector3 BlockMesh_GetDirection(BlockFace face);

#endif