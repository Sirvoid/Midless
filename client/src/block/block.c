/**
 * Copyright (c) 2021 Sirvoid
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
#include "blockshape.h"
#include "blockmeshgeneration.h"
#include "resource.h"

Block blockDefinitions[BLOCK_RUNTIME_COUNT];
static Block defaultDefinitions[256];
static bool defined[BLOCK_RUNTIME_COUNT], overridden[BLOCK_RUNTIME_COUNT], dirty[BLOCK_RUNTIME_COUNT];
static bool textureAvailable[256];
static bool lightingChanged;

static void Block_Finalize(Block *block) {
    block->fullCube = Block_IsFullSize(block) && (!block->geometry.enabled || block->geometry.boxCount == 1);
    block->fastOpaqueCube = block->fullCube && block->modelType == BLOCK_MODEL_SOLID &&
                            block->renderType == BLOCK_RENDER_OPAQUE;
}


static unsigned char Block_GetLightPassFaces(const Block *block) {
    if (block->renderType != BLOCK_RENDER_OPAQUE || (block->geometry.enabled && !block->fullCube)) return 0x3F;

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
    }
    for(int i=0;i<BLOCK_RUNTIME_COUNT;i++) {
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
    blockDefinitions[0].renderType = BLOCK_RENDER_TRANSPARENT;

    Block_Define(1, "stone", 1, 1, 1);
    Block_Define(2, "dirt", 2, 2, 2);
    Block_Define(3, "grass", 0, 2, 3);
    Block_Define(4, "wood", 4, 4, 4);
    
    Block_Define(5, "water", 14, 14, 14);
    blockDefinitions[5].renderType = BLOCK_RENDER_TRANSLUCENT;
    
    Block_Define(6, "sand", 11, 11, 11);
    Block_Define(7, "iron_ore", 6, 6, 6);
    Block_Define(8, "coal_ore", 7, 7, 7);
    Block_Define(9, "gold_ore", 5, 5, 5);
    Block_Define(10, "log", 9, 9, 8);
    Block_Define(11, "leaves", 10, 10, 10);
    blockDefinitions[11].renderType = BLOCK_RENDER_TRANSPARENT;
    
    Block_Define(12, "rose", 12, 12, 12);
    blockDefinitions[12].renderType = BLOCK_RENDER_TRANSPARENT;
    
    Block_Define(13, "dandelion", 13, 13, 13);
    blockDefinitions[13].renderType = BLOCK_RENDER_TRANSPARENT;
    
    Block_Define(14, "glass", 17, 17, 17);
    blockDefinitions[14].renderType = BLOCK_RENDER_TRANSPARENT;

    Block_Define(15, "fire", 16, 16, 16);
    blockDefinitions[15].renderType = BLOCK_RENDER_TRANSPARENT;
    blockDefinitions[15].lightType = BLOCK_LIGHT_EMIT;

    Block_Define(16, "lava", 15, 15, 15);
    blockDefinitions[16].lightType = BLOCK_LIGHT_EMIT;

    Block_Define(17, "stone_slab", 1, 1, 1);

    Block_Define(18, "wood_slab", 4, 4, 4);

    for (int i = 0; i < 256; i++) {
        Block *block = &blockDefinitions[i];
        BlockDefinition shape = {0};
        BlockShape_Default(i, &shape);
        block->modelType = shape.modelType;
        block->colliderType = shape.colliderType;
        block->minBB = (Vector3){shape.min[0], shape.min[1], shape.min[2]};
        block->maxBB = (Vector3){shape.max[0], shape.max[1], shape.max[2]};
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
    if ((unsigned int)id < BLOCK_RUNTIME_COUNT) {
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
    return id >= 0 && id < BLOCK_RUNTIME_COUNT && defined[id];
}

bool Block_IsSelectable(int id) {
    return id > 0 && Block_IsDefined(id) && blockDefinitions[id].modelType != BLOCK_MODEL_GAS;
}

bool Block_IsOverridden(int id) {
    return id > 0 && id < BLOCK_RUNTIME_COUNT && overridden[id];
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
    for(int box=0;box<d->geometry.boxCount;box++)
        for(int f=0;f<6;f++) if(!textureAvailable[d->geometry.boxes[box].textures[f]]) return false;
    Block block = {0};
    block.geometry=d->geometry;
    memcpy(block.name, d->name, sizeof(block.name));
    for (int i = 0; i < 6; i++) block.textures[i] = d->textures[i];
    block.modelType = d->modelType;
    block.renderType = d->renderType;
    block.colliderType = d->colliderType;
    block.lightType = d->lightType;
    block.minBB = (Vector3){d->min[0], d->min[1], d->min[2]};
    block.maxBB = (Vector3){d->max[0], d->max[1], d->max[2]};
    if(block.geometry.enabled) {
        BlockBox bounds=BlockBox_Rotate(block.geometry.boxes[0].bounds,block.geometry.rotation);
        for(int i=1;i<block.geometry.boxCount;i++) {
            BlockBox b=BlockBox_Rotate(block.geometry.boxes[i].bounds,block.geometry.rotation);
            for(int a=0;a<3;a++) { if(b.min[a]<bounds.min[a]) bounds.min[a]=b.min[a]; if(b.max[a]>bounds.max[a]) bounds.max[a]=b.max[a]; }
        }
        block.minBB=(Vector3){bounds.min[0],bounds.min[1],bounds.min[2]};
        block.maxBB=(Vector3){bounds.max[0],bounds.max[1],bounds.max[2]};
    }
    Block_Finalize(&block);
    block.lightPassFaces = Block_GetLightPassFaces(&block);
    Block_Replace(id, block);
    defined[id] = overridden[id] = true;
    return true;
}

void Block_RemoveDefinition(int id) {
    if (!Block_IsOverridden(id)) return;
    if(id<256) for(int state=1;state<BLOCK_MAX_STATES;state++) Block_RemoveDefinition(id | (state<<8));
    Block_Replace(id, defaultDefinitions[id<256?id:0]);
    defined[id] = id <= BLOCK_DEFAULT_LAST_ID;
    overridden[id] = false;
}

void Block_ResetDefinitions(void) {
    for (int id = 1; id < 256; id++) Block_RemoveDefinition(id);
}

void Block_FlushDefinitionChanges(void) {
    bool changed = false;
    for (int id = 1; id < BLOCK_RUNTIME_COUNT; id++) changed |= dirty[id];
    if (!changed) return;
    Block_LoadLiquidTints();
    for (int id = 1; id < 256; id++) {
        if (dirty[id]) BlockItemRenderer_Refresh(id);
        dirty[id] = false;
    }
    memset(dirty,0,sizeof(dirty));
    World_InvalidateBlockDefinitions(lightingChanged);
    lightingChanged = false;
    if (!Block_IsSelectable(player.blockSelected))
        player.blockSelected = 0;
}

int Block_BoxCount(const Block *b,bool selection) {
    return b->geometry.enabled?(selection?b->geometry.selectionCount:b->geometry.collisionCount):1;
}
BoundingBox Block_GetBox(const Block *b,int index,Vector3 p,bool selection) {
    if(!b->geometry.enabled) return (BoundingBox){
        {p.x+b->minBB.x/16,p.y+b->minBB.y/16,p.z+b->minBB.z/16},
        {p.x+b->maxBB.x/16,p.y+b->maxBB.y/16,p.z+b->maxBB.z/16}};
    BlockBox box=BlockBox_Rotate(selection?b->geometry.selection[index]:b->geometry.collision[index],b->geometry.rotation);
    return (BoundingBox){{p.x+box.min[0]/16.0f,p.y+box.min[1]/16.0f,p.z+box.min[2]/16.0f},
        {p.x+box.max[0]/16.0f,p.y+box.max[1]/16.0f,p.z+box.max[2]/16.0f}};
}
