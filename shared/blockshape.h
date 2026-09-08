#ifndef MIDLESS_BLOCK_SHAPE_H
#define MIDLESS_BLOCK_SHAPE_H
#include "raylib.h"
#include "blockdefinition.h"

typedef struct BlockShape {
    BoundingBox bounds;
    bool solid, targetable, liquid;
} BlockShape;

void BlockShape_Default(int id, BlockDefinition *definition);
BlockShape BlockShape_Get(int id, const BlockDefinition *override, Vector3 position);
#endif
