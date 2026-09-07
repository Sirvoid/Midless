#include "blockshape.h"

void BlockShape_Default(int id, BlockDefinition *definition) {
    definition->modelType = BLOCK_MODEL_SOLID;
    definition->colliderType = BLOCK_COLLIDER_SOLID;
    for (int axis = 0; axis < 3; axis++) {
        definition->min[axis] = 0;
        definition->max[axis] = 16;
    }
    if (id == 0 || id > BLOCK_DEFAULT_LAST_ID) {
        definition->modelType = BLOCK_MODEL_GAS;
        definition->colliderType = BLOCK_COLLIDER_NONE;
    } else if (id == 5 || id == 16) {
        definition->colliderType = BLOCK_COLLIDER_LIQUID;
    } else if (id == 12 || id == 13 || id == 15) {
        definition->modelType = BLOCK_MODEL_SPRITE;
        definition->colliderType = BLOCK_COLLIDER_NONE;
        if (id != 15) {
            definition->min[0] = definition->min[2] = 4;
            definition->max[0] = definition->max[2] = 12;
            definition->max[1] = 10;
        }
    } else if (id == 17 || id == 18) {
        definition->max[1] = 8;
    }
}

BlockShape BlockShape_Get(int id, const BlockDefinition *override, Vector3 position) {
    BlockDefinition defaults = {0};
    if (!override) BlockShape_Default(id, &defaults);
    const BlockDefinition *definition = override ? override : &defaults;
    BlockShape shape = {
        .solid = definition->colliderType == BLOCK_COLLIDER_SOLID,
        .liquid = definition->colliderType == BLOCK_COLLIDER_LIQUID,
        .targetable = definition->modelType != BLOCK_MODEL_GAS && definition->colliderType != BLOCK_COLLIDER_LIQUID,
        .bounds = {
            {position.x + definition->min[0] / 16.0f, position.y + definition->min[1] / 16.0f, position.z + definition->min[2] / 16.0f},
            {position.x + definition->max[0] / 16.0f, position.y + definition->max[1] / 16.0f, position.z + definition->max[2] / 16.0f}
        }
    };
    return shape;
}
