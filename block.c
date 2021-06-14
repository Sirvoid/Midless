#include "block.h"
#include "raylib.h"

Block Block_definition[256];

Block* Block_Define(int ID, char name[], int topTex, int bottomTex, int sideTex) {
    Block *block = &Block_definition[ID];
    TextCopy(block->name, name);
    
    Block_SetTexture(block, BlockFace_Top, topTex);
    Block_SetTexture(block, BlockFace_Bottom, bottomTex);
    Block_SetTexture(block, BlockFace_Left, sideTex);
    Block_SetTexture(block, BlockFace_Right, sideTex);
    Block_SetTexture(block, BlockFace_Front, sideTex);
    Block_SetTexture(block, BlockFace_Back, sideTex);
    
    return block;
}

void Block_SetTexture(Block *block, BlockFace face, int texIndex) {
    block->textures[(int)face] = texIndex;
}

int Block_GetTexture(Block *block, BlockFace face) {
    return block->textures[(int)face];
}