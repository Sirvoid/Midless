#ifndef G_BLOCKFACEHELPER_H
#define G_BLOCKFACEHELPER_H

#include "raylib.h" 
#include "block.h"

//Reset memory counters.
void BFH_ResetIndexes();

//Add a block face to a given mesh.
void BFH_AddFace(Mesh *mesh, BlockFace face, Vector3 pos, int blockID);

//Get facing direction of a block face.
Vector3 BFH_GetDirection(BlockFace face);

#endif