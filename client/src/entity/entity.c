/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "raylib.h"
#include "raymath.h"
#include "entity.h"

void Entity_Draw(Entity *entity) {
    EntityModel *model = &entity->model;
    for(int i = 0; i < model->amountParts; i++) {
        
        EntityModelPart *part = &model->parts[i];

        Matrix drawMatrix = (Matrix) {1,0,0,0,
                                      0,1,0,0,
                                      0,0,1,0,
                                      0,0,0,1};

        drawMatrix = MatrixMultiply(drawMatrix, MatrixRotateXYZ(part->rotation));

        drawMatrix.m12 += part->position.x / 16;
        drawMatrix.m13 += part->position.y / 16;
        drawMatrix.m14 += part->position.z / 16;

        drawMatrix = MatrixMultiply(drawMatrix, MatrixRotateXYZ(entity->rotation));

        drawMatrix.m12 += entity->position.x;
        drawMatrix.m13 += entity->position.y;
        drawMatrix.m14 += entity->position.z;

        DrawMesh(part->mesh, model->mat, drawMatrix);
    }
}

void Entity_Remove(Entity *entity) {
    if(entity->type == 0) return;
    entity->type = 0;
    EntityModel_Free(&entity->model);
}
