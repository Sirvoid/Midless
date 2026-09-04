/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "raylib.h"
#include "raymath.h"
#include "entity.h"

static void Entity_DrawFiltered(Entity *entity, bool firstPersonOnly) {
    EntityModel *model = &entity->model;
    for (int i = 0; i < model->partCount; i++) {
        EntityModelPart *part = &model->parts[i];
        if (firstPersonOnly && !part->visibleInFirstPerson) continue;

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

        DrawMesh(part->mesh, model->material, drawMatrix);
    }
}

void Entity_Draw(Entity *entity) {
    Entity_DrawFiltered(entity, false);
}

void Entity_DrawFirstPerson(Entity *entity, Camera camera) {
    Matrix cameraTransform = MatrixInvert(GetCameraMatrix(camera));

    EntityModel *model = &entity->model;
    for (int i = 0; i < model->partCount; i++) {
        EntityModelPart *part = &model->parts[i];
        if (!part->visibleInFirstPerson) continue;

        Matrix drawMatrix = MatrixRotateXYZ(Vector3Scale((Vector3) {-60.0f, 0.0f, -190.0f}, DEG2RAD));
        drawMatrix.m12 = 0.35f;
        drawMatrix.m13 = -0.42f;
        drawMatrix.m14 = -0.25f;
        drawMatrix = MatrixMultiply(drawMatrix, cameraTransform);
        DrawMesh(part->mesh, model->material, drawMatrix);
    }
}

void Entity_Destroy(Entity *entity) {
    if (entity->type == 0) return;
    entity->type = 0;
    EntityModel_Unload(&entity->model);
    EntityModel_Destroy(&entity->model);
}
