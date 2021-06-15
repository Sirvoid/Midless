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

//Definiton of every blocks.
extern Block Block_definition[256];

//Define a block.
Block* Block_Define(int ID, char name[], int topTex, int bottomTex, int sideTex);

//Set texture for a block's face.
void Block_SetTexture(Block *block, BlockFace face, int texIndex);

//Get texture of a block's face.
int Block_GetTexture(Block *block, BlockFace face);

#endif