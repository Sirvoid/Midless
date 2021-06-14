#ifndef G_BLOCK_H
#define G_BLOCK_H

typedef enum BlockFace{
	BlockFace_Left,
	BlockFace_Right,
	BlockFace_Top,
	BlockFace_Bottom,
	BlockFace_Front,
	BlockFace_Back
} BlockFace;

typedef struct Block {
    char name[16];
    int textures[6];
} Block;

extern Block Block_definition[256];

Block* Block_Define(int ID, char name[], int topTex, int bottomTex, int sideTex);
void Block_SetTexture(Block *block, BlockFace face, int texIndex);

int Block_GetTexture(Block *block, BlockFace face);

#endif