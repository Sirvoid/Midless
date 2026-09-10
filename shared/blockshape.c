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
    shape.collisionCount=shape.selectionCount=1;
    shape.collision[0]=shape.selection[0]=shape.bounds;
    const BlockGeometry *g=&definition->geometry;
    if(g->enabled) {
        shape.collisionCount=g->collisionCount; shape.selectionCount=g->selectionCount;
        for(int group=0;group<2;group++) {
            int count=group?g->selectionCount:g->collisionCount;
            for(int i=0;i<count;i++) {
                BlockBox b=BlockBox_Rotate(group?g->selection[i]:g->collision[i],g->rotation);
                BoundingBox box={{position.x+b.min[0]/16.0f,position.y+b.min[1]/16.0f,position.z+b.min[2]/16.0f},
                    {position.x+b.max[0]/16.0f,position.y+b.max[1]/16.0f,position.z+b.max[2]/16.0f}};
                if(group) shape.selection[i]=box; else shape.collision[i]=box;
            }
        }
        shape.targetable &= shape.selectionCount>0;
        shape.solid &= shape.collisionCount>0;
        int count=shape.selectionCount?shape.selectionCount:shape.collisionCount;
        BoundingBox *boxes=shape.selectionCount?shape.selection:shape.collision;
        if(count) shape.bounds=boxes[0];
        for(int i=1;i<count;i++) {
            if(boxes[i].min.x<shape.bounds.min.x) shape.bounds.min.x=boxes[i].min.x;
            if(boxes[i].min.y<shape.bounds.min.y) shape.bounds.min.y=boxes[i].min.y;
            if(boxes[i].min.z<shape.bounds.min.z) shape.bounds.min.z=boxes[i].min.z;
            if(boxes[i].max.x>shape.bounds.max.x) shape.bounds.max.x=boxes[i].max.x;
            if(boxes[i].max.y>shape.bounds.max.y) shape.bounds.max.y=boxes[i].max.y;
            if(boxes[i].max.z>shape.bounds.max.z) shape.bounds.max.z=boxes[i].max.z;
        }
    }
    return shape;
}
