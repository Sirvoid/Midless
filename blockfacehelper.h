#ifndef G_BLOCKFACEHELPER_H
#define G_BLOCKFACEHELPER_H

#include "raylib.h" 

typedef enum {
	BlockFace_Left,
	BlockFace_Right,
	BlockFace_Top,
	BlockFace_Bottom,
	BlockFace_Front,
	BlockFace_Back
}  BlockFace;

void BFH_ResetIndexes();
void BFH_AddFace(Mesh *mesh, BlockFace face, Vector3 pos, int block_id);

Vector3 BFH_GetDirection(BlockFace face);

#endif