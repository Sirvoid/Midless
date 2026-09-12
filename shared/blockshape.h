/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_BLOCK_SHAPE_H
#define MIDLESS_BLOCK_SHAPE_H
#include "raylib.h"
#include "blockdefinition.h"

typedef struct BlockShape {
    BoundingBox bounds;
    int collisionCount, selectionCount;
    BoundingBox collision[BLOCK_MODEL_MAX_BOXES], selection[BLOCK_MODEL_MAX_BOXES];
    bool solid, targetable, liquid;
} BlockShape;

void BlockShape_Default(int id, BlockDefinition *definition);
BlockShape BlockShape_Get(int id, const BlockDefinition *override, Vector3 position);
#endif
