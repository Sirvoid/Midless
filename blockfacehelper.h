#ifndef G_BLOCKFACEHELPER_H
#define G_BLOCKFACEHELPER_H

#include "raylib.h" 
#include "block.h"
#include "chunk.h"
#include "chunkmesh.h"

//Reset memory counters.
void BFH_ResetIndexes(void);

//Add a block face to a given mesh.
void BFH_AddFace(ChunkMesh *mesh, BlockFace face, Vector3 pos, Block blockDef, int translucent, int light);

//Get facing direction of a block face.
Vector3 BFH_GetDirection(BlockFace face);

void BFH_GetFacesPosition(Block b, Vector3 *facesPosition);

#endif