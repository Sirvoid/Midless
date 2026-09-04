/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_BLOCK_H
#define MIDLESS_CLIENT_BLOCK_H

#include "raylib.h"

typedef enum BlockFace{
    BLOCK_FACE_LEFT,
	BLOCK_FACE_RIGHT,
	BLOCK_FACE_TOP,
	BLOCK_FACE_BOTTOM,
	BLOCK_FACE_FRONT,
	BLOCK_FACE_BACK
} BlockFace;

typedef enum BlockModelType{
    BLOCK_MODEL_GAS,
    BLOCK_MODEL_SOLID,
    BLOCK_MODEL_SPRITE
} BlockModelType;

typedef enum BlockLightType {
    BLOCK_LIGHT_NONE,
    BLOCK_LIGHT_EMIT
} BlockLightType;

typedef enum BlockRenderType{
    BLOCK_RENDER_OPAQUE,
    BLOCK_RENDER_TRANSPARENT,
    BLOCK_RENDER_TRANSLUCENT
} BlockRenderType;

typedef enum BlockColliderType{
    BLOCK_COLLIDER_NONE,
    BLOCK_COLLIDER_SOLID,
    BLOCK_COLLIDER_LIQUID
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
    bool fullCube;
    bool fastOpaqueCube;
    unsigned char lightPassFaces;
} Block;

extern Block blockDefinitions[256];

const Block *Block_GetDefinition(int id);

//Define All Blocks
void Block_BuildDefinition(void);

//Define a block.
Block* Block_Define(int id, char name[], int topTexture, int bottomTexture, int sideTexture);

//Set texture for a block's face.
void Block_SetTexture(Block *block, BlockFace face, int textureIndex);

//Get texture of a block's face.
int Block_GetTexture(Block *block, BlockFace face);

//Verify if a block is full size
bool Block_IsFullSize(Block *block);

#endif
