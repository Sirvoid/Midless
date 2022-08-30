/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_BLOCKFACEHELPER_H
#define G_BLOCKFACEHELPER_H

#include <raylib.h>
#include "block.h"
#include "../chunk/chunk.h"
#include "../chunk/chunkmesh.h"

//Reset memory counters.
void BlockMesh_ResetIndexes(void);

//Add a block face to a given mesh.
void BlockMesh_AddFace(unsigned char *vertices, unsigned short *indices, unsigned short *texcoords, unsigned char *colors, BlockFace face, Vector3 pos, Block b, int translucent, int light, int sunlight);

//Get facing direction of a block face.
Vector3 BlockMesh_GetDirection(BlockFace face);

void BlockMesh_GetFacesPosition(Block b, Vector3 *facesPosition);

#endif