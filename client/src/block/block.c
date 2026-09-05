/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <string.h>
#include "raylib.h"
#include "blockitemrenderer.h"
#include "world.h"
#include "player.h"
#include "block.h"
#include "blockmeshgeneration.h"
#include "resource.h"

Block blockDefinitions[256];
static Block defaultDefinitions[256];
static bool defined[256], overridden[256], dirty[256];
static bool textureAvailable[256];
static bool lightingChanged;

static void Block_Finalize(Block *block) {
    block->fullCube = Block_IsFullSize(block);
    block->fastOpaqueCube = block->fullCube && block->modelType == BLOCK_MODEL_SOLID &&
                            block->renderType == BLOCK_RENDER_OPAQUE;
}


static unsigned char Block_GetLightPassFaces(const Block *block) {
    if (block->renderType != BLOCK_RENDER_OPAQUE) return 0x3F;

    bool fullX = block->minBB.x <= 0 && block->maxBB.x >= 16;
    bool fullY = block->minBB.y <= 0 && block->maxBB.y >= 16;
    bool fullZ = block->minBB.z <= 0 && block->maxBB.z >= 16;
    unsigned char faces = 0;

    // Light can cross an axis when the block does not fill the complete cross-section perpendicular to that axis.
    if (!(fullY && fullZ)) faces |= (1u << BLOCK_FACE_LEFT) | (1u << BLOCK_FACE_RIGHT);
    if (!(fullX && fullZ)) faces |= (1u << BLOCK_FACE_TOP) | (1u << BLOCK_FACE_BOTTOM);
    if (!(fullX && fullY)) faces |= (1u << BLOCK_FACE_FRONT) | (1u << BLOCK_FACE_BACK);
    return faces;
}

static void Block_LoadLiquidTints(void) {
    Image atlas = Resource_LoadImage("terrain.png");
    if (!atlas.data) return;

    for (int i = 0; i < 256; i++) {
        textureAvailable[i] = (i % 16) * 16 + 16 <= atlas.width &&
                              (i / 16) * 16 + 16 <= atlas.height;
        Block *block = &blockDefinitions[i];
        if (block->colliderType != BLOCK_COLLIDER_LIQUID) continue;

        int textureIndex = block->textures[BLOCK_FACE_TOP];
        int pixelX = (textureIndex % 16) * 16;
        int pixelY = (textureIndex / 16) * 16;
        if (pixelX >= atlas.width || pixelY >= atlas.height) continue;

        block->liquidTint = GetImageColor(atlas, pixelX, pixelY);
        block->liquidTint.a = 105;
    }

    UnloadImage(atlas);
}

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
    blockDefinitions[16].renderType = BLOCK_RENDER_TRANSLUCENT;
    blockDefinitions[16].lightType = BLOCK_LIGHT_EMIT;

    Block_Define(17, "stone_slab", 1, 1, 1);
    blockDefinitions[17].maxBB = (Vector3) {16, 8, 16};

    Block_Define(18, "wood_slab", 4, 4, 4);
    blockDefinitions[18].maxBB = (Vector3) {16, 8, 16};

    for (int i = 0; i < 256; i++) {
        Block *block = &blockDefinitions[i];
        Block_Finalize(block);
        block->lightPassFaces = Block_GetLightPassFaces(block);
    }
    Block_LoadLiquidTints();
    BlockMesh_BuildTemplates();
    memcpy(defaultDefinitions, blockDefinitions, sizeof(defaultDefinitions));
    for (int i = 0; i < 256; i++) defined[i] = i <= BLOCK_DEFAULT_LAST_ID;
    memset(overridden, 0, sizeof(overridden));
    memset(dirty, 0, sizeof(dirty));
    lightingChanged = false;
}

const Block *Block_GetDefinition(int id) {
    if ((unsigned int)id < 256) {
        return &blockDefinitions[id];
    }
    return &blockDefinitions[0];
}

Block* Block_Define(int id, char name[], int topTexture, int bottomTexture, int sideTexture) {
    if (id < 0 || id > 255 || !name) return NULL;
    Block *block = &blockDefinitions[id];
    strncpy(block->name, name, sizeof(block->name) - 1);
    block->name[sizeof(block->name) - 1] = 0;
    
    block->modelType = BLOCK_MODEL_SOLID;
    block->renderType = BLOCK_RENDER_OPAQUE;
    block->colliderType = BLOCK_COLLIDER_SOLID;
    block->lightType = BLOCK_LIGHT_NONE;
    block->liquidTint = BLANK;
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

bool Block_IsDefined(int id) {
    return id >= 0 && id < 256 && defined[id];
}

bool Block_IsSelectable(int id) {
    return id > 0 && Block_IsDefined(id) && blockDefinitions[id].modelType != BLOCK_MODEL_GAS;
}

bool Block_IsOverridden(int id) {
    return id > 0 && id < 256 && overridden[id];
}

int Block_NextSelectable(int id, int direction) {
    id = (id >= 0 && id < 256) ? id : 0;
    for (int i = 0; i < 256; i++) {
        id = (id + (direction < 0 ? 255 : 1)) % 256;
        if (Block_IsSelectable(id)) return id;
    }
    return 0;
}

static void Block_Replace(int id, Block block) {
    const Block *old = &blockDefinitions[id];
    lightingChanged |= old->renderType != block.renderType || old->lightType != block.lightType ||
                       old->fullCube != block.fullCube || old->lightPassFaces != block.lightPassFaces;
    blockDefinitions[id] = block;
    BlockMesh_BuildTemplate(id);
    dirty[id] = true;
}

bool Block_ApplyDefinition(int id, const BlockDefinition *d) {
    if (!BlockDefinition_Validate(id, d)) return false;
    for (int i = 0; i < 6; i++) if (!textureAvailable[d->textures[i]]) return false;
    Block block = {0};
    memcpy(block.name, d->name, sizeof(block.name));
    for (int i = 0; i < 6; i++) block.textures[i] = d->textures[i];
    block.modelType = d->modelType;
    block.renderType = d->renderType;
    block.colliderType = d->colliderType;
    block.lightType = d->lightType;
    block.minBB = (Vector3){d->min[0], d->min[1], d->min[2]};
    block.maxBB = (Vector3){d->max[0], d->max[1], d->max[2]};
    Block_Finalize(&block);
    block.lightPassFaces = Block_GetLightPassFaces(&block);
    Block_Replace(id, block);
    defined[id] = overridden[id] = true;
    return true;
}

void Block_RemoveDefinition(int id) {
    if (!Block_IsOverridden(id)) return;
    Block_Replace(id, defaultDefinitions[id]);
    defined[id] = id <= BLOCK_DEFAULT_LAST_ID;
    overridden[id] = false;
}

void Block_ResetDefinitions(void) {
    for (int id = 1; id < 256; id++) Block_RemoveDefinition(id);
}

void Block_FlushDefinitionChanges(void) {
    bool changed = false;
    for (int id = 1; id < 256; id++) changed |= dirty[id];
    if (!changed) return;
    Block_LoadLiquidTints();
    for (int id = 1; id < 256; id++) {
        if (dirty[id]) BlockItemRenderer_Refresh(id);
        dirty[id] = false;
    }
    World_InvalidateBlockDefinitions(lightingChanged);
    lightingChanged = false;
    if (!Block_IsSelectable(player.blockSelected))
        player.blockSelected = Block_NextSelectable(player.blockSelected, 1);
}
