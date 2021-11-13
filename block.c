#include "block.h"
#include "raylib.h"

Block Block_definition[256];

void Block_BuildDefinition(void) {

    for(int i = 0; i < 256; i++) {
        Block_Define(i, "invalid", 0, 0, 0);
        Block_definition[i].colliderType = BlockColliderType_None;
    }

    Block_Define(0, "air", 0, 0, 0);
    Block_definition[0].modelType = BlockModelType_Gas;
    Block_definition[0].renderType = BlockRenderType_Transparent;
    Block_definition[0].colliderType = BlockColliderType_None;

    Block_Define(1, "stone", 1, 1, 1);
    Block_Define(2, "dirt", 2, 2, 2);
    Block_Define(3, "grass", 0, 2, 3);
    Block_Define(4, "wood", 4, 4, 4);
    
    Block_Define(5, "water", 14, 14, 14);
    Block_definition[5].renderType = BlockRenderType_Translucent;
    Block_definition[5].colliderType = BlockColliderType_Liquid;
    
    Block_Define(6, "sand", 11, 11, 11);
    Block_Define(7, "iron_ore", 6, 6, 6);
    Block_Define(8, "coal_ore", 7, 7, 7);
    Block_Define(9, "gold_ore", 5, 5, 5);
    Block_Define(10, "log", 9, 9, 8);
    Block_Define(11, "leaves", 10, 10, 10);
    Block_definition[11].renderType = BlockRenderType_Transparent;
    
    Block_Define(12, "rose", 12, 12, 12);
    Block_definition[12].modelType = BlockModelType_Sprite;
    Block_definition[12].renderType = BlockRenderType_Transparent;
    Block_definition[12].colliderType = BlockColliderType_None;
    
    Block_Define(13, "dandelion", 13, 13, 13);
    Block_definition[13].modelType = BlockModelType_Sprite;
    Block_definition[13].renderType = BlockRenderType_Transparent;
    Block_definition[13].colliderType = BlockColliderType_None;
    
    Block_Define(14, "glass", 17, 17, 17);
    Block_definition[14].renderType = BlockRenderType_Transparent;

    Block_Define(15, "fire", 16, 16, 16);
    Block_definition[15].renderType = BlockRenderType_Transparent;
    Block_definition[15].modelType = BlockModelType_Sprite;
    Block_definition[15].colliderType = BlockColliderType_None;
    Block_definition[15].lightType = BlockLightType_Emit;

    Block_Define(16, "lava", 15, 15, 15);
    Block_definition[16].colliderType = BlockColliderType_None;
    Block_definition[16].lightType = BlockLightType_Emit;

    Block_Define(17, "stone_slab", 1, 1, 1);
    Block_definition[17].maxBB = (Vector3) {16, 8, 16};

    Block_Define(18, "wood_slab", 4, 4, 4);
    Block_definition[18].maxBB = (Vector3) {16, 8, 16};
}

Block* Block_Define(int ID, char name[], int topTex, int bottomTex, int sideTex) {
    Block *block = &Block_definition[ID];
    TextCopy(block->name, name);
    
    block->modelType = BlockModelType_Solid;
    block->renderType = BlockRenderType_Opaque;
    block->colliderType = BlockColliderType_Solid;
    block->lightType = BlockLightType_None;
    block->minBB = (Vector3) {0, 0, 0};
    block->maxBB = (Vector3) {16, 16, 16};

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