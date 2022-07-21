/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_BLOCK_H
#define G_BLOCK_H

#include "raylib.h"

typedef enum BlockFace{
    BlockFace_Left,
	BlockFace_Right,
	BlockFace_Top,
	BlockFace_Bottom,
	BlockFace_Front,
	BlockFace_Back
} BlockFace;

typedef enum BlockModelType{
    BlockModelType_Gas,
    BlockModelType_Solid,
    BlockModelType_Sprite
} BlockType;

typedef enum BlockLightType {
    BlockLightType_None,
    BlockLightType_Emit
} BlockLightType;

typedef enum BlockRenderType{
    BlockRenderType_Opaque,
    BlockRenderType_Transparent,
    BlockRenderType_Translucent
} BlockRenderType;

typedef enum BlockColliderType{
    BlockColliderType_None,
    BlockColliderType_Solid,
    BlockColliderType_Liquid
} BlockColliderType;

typedef struct Block {
    char name[16];
    int textures[6];
    int modelType;
    int renderType;
    int colliderType;
    int lightType;
    Vector3 minBB; //0-16
    Vector3 maxBB; //0-16
} Block;

Block Block_GetDefinition(int ID);

//Define All Blocks
void Block_BuildDefinition(void);

//Define a block.
Block* Block_Define(int ID, char name[], int topTex, int bottomTex, int sideTex);

//Set texture for a block's face.
void Block_SetTexture(Block *block, BlockFace face, int texIndex);

//Get texture of a block's face.
int Block_GetTexture(Block *block, BlockFace face);

//Verify if a block is full size
bool Block_IsFullSize(Block *block);

#endif