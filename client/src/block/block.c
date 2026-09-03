/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "raylib.h"
#include "block.h"
#include "blockmeshgeneration.h"

Block blockDefinitions[256];

void Block_BuildDefinition(void) {

    for (int i = 0; i < 256; i++) {
        Block_Define(i, "invalid", 0, 0, 0);
        blockDefinitions[i].colliderType = BLOCK_COLLIDER_NONE;
    }

    Block_Define(0, "air", 0, 0, 0);
    blockDefinitions[0].modelType = BLOCK_MODEL_GAS;
    blockDefinitions[0].renderType = BLOCK_RENDER_TRANSPARENT;
    blockDefinitions[0].colliderType = BLOCK_COLLIDER_NONE;

    Block_Define(1, "stone", 1, 1, 1);
    Block_Define(2, "dirt", 2, 2, 2);
    Block_Define(3, "grass", 0, 2, 3);
    Block_Define(4, "wood", 4, 4, 4);
    
    Block_Define(5, "water", 14, 14, 14);
    blockDefinitions[5].renderType = BLOCK_RENDER_TRANSLUCENT;
    blockDefinitions[5].colliderType = BLOCK_COLLIDER_LIQUID;
    
    Block_Define(6, "sand", 11, 11, 11);
    Block_Define(7, "iron_ore", 6, 6, 6);
    Block_Define(8, "coal_ore", 7, 7, 7);
    Block_Define(9, "gold_ore", 5, 5, 5);
    Block_Define(10, "log", 9, 9, 8);
    Block_Define(11, "leaves", 10, 10, 10);
    blockDefinitions[11].renderType = BLOCK_RENDER_TRANSPARENT;
    
    Block_Define(12, "rose", 12, 12, 12);
    blockDefinitions[12].modelType = BLOCK_MODEL_SPRITE;
    blockDefinitions[12].renderType = BLOCK_RENDER_TRANSPARENT;
    blockDefinitions[12].colliderType = BLOCK_COLLIDER_NONE;
    blockDefinitions[12].minBB = (Vector3) {4, 0, 4};
    blockDefinitions[12].maxBB = (Vector3) {12, 10, 12};
    
    Block_Define(13, "dandelion", 13, 13, 13);
    blockDefinitions[13].modelType = BLOCK_MODEL_SPRITE;
    blockDefinitions[13].renderType = BLOCK_RENDER_TRANSPARENT;
    blockDefinitions[13].colliderType = BLOCK_COLLIDER_NONE;
    blockDefinitions[13].minBB = (Vector3) {4, 0, 4};
    blockDefinitions[13].maxBB = (Vector3) {12, 10, 12};
    
    Block_Define(14, "glass", 17, 17, 17);
    blockDefinitions[14].renderType = BLOCK_RENDER_TRANSPARENT;

    Block_Define(15, "fire", 16, 16, 16);
    blockDefinitions[15].renderType = BLOCK_RENDER_TRANSPARENT;
    blockDefinitions[15].modelType = BLOCK_MODEL_SPRITE;
    blockDefinitions[15].colliderType = BLOCK_COLLIDER_NONE;
    blockDefinitions[15].lightType = BLOCK_LIGHT_EMIT;

    Block_Define(16, "lava", 15, 15, 15);
    blockDefinitions[16].colliderType = BLOCK_COLLIDER_LIQUID;
    blockDefinitions[16].lightType = BLOCK_LIGHT_EMIT;

    Block_Define(17, "stone_slab", 1, 1, 1);
    blockDefinitions[17].maxBB = (Vector3) {16, 8, 16};

    Block_Define(18, "wood_slab", 4, 4, 4);
    blockDefinitions[18].maxBB = (Vector3) {16, 8, 16};

    for (int i = 0; i < 256; i++) {
        Block *block = &blockDefinitions[i];
        block->fullCube = Block_IsFullSize(block);
        block->fastOpaqueCube = block->fullCube &&
                                block->modelType == BLOCK_MODEL_SOLID &&
                                block->renderType == BLOCK_RENDER_OPAQUE;
    }
    BlockMesh_BuildTemplates();
}

const Block *Block_GetDefinition(int id) {
    if ((unsigned int)id < 256) {
        return &blockDefinitions[id];
    }
    return &blockDefinitions[0];
}

Block* Block_Define(int id, char name[], int topTexture, int bottomTexture, int sideTexture) {
    Block *block = &blockDefinitions[id];
    TextCopy(block->name, name);
    
    block->modelType = BLOCK_MODEL_SOLID;
    block->renderType = BLOCK_RENDER_OPAQUE;
    block->colliderType = BLOCK_COLLIDER_SOLID;
    block->lightType = BLOCK_LIGHT_NONE;
    block->minBB = (Vector3) {0, 0, 0};
    block->maxBB = (Vector3) {16, 16, 16};

    Block_SetTexture(block, BLOCK_FACE_TOP, topTexture);
    Block_SetTexture(block, BLOCK_FACE_BOTTOM, bottomTexture);
    Block_SetTexture(block, BLOCK_FACE_LEFT, sideTexture);
    Block_SetTexture(block, BLOCK_FACE_RIGHT, sideTexture);
    Block_SetTexture(block, BLOCK_FACE_FRONT, sideTexture);
    Block_SetTexture(block, BLOCK_FACE_BACK, sideTexture);
    
    return block;
}

void Block_SetTexture(Block *block, BlockFace face, int textureIndex) {
    block->textures[(int)face] = textureIndex;
}

int Block_GetTexture(Block *block, BlockFace face) {
    return block->textures[(int)face];
}

bool Block_IsFullSize(Block *block) {
    return block->minBB.x == 0 && block->minBB.z == 0 && block->minBB.y == 0 && block->maxBB.x == 16 && block->maxBB.z == 16 && block->maxBB.y == 16;
}