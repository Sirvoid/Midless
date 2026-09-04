/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "raylib.h"
#include "raymath.h"
#include "entity.h"
#include "world.h"

static float Entity_GetBrightness(Vector3 position) {
    Vector3 samplePosition = {position.x, position.y + 0.75f, position.z};
    return World_GetBrightness(samplePosition);
}

static void Entity_ApplyBrightness(Entity *entity) {
    float brightness = Entity_GetBrightness(entity->position);
    unsigned char value = (unsigned char)(brightness * 255.0f);
    entity->model.material.maps[MATERIAL_MAP_DIFFUSE].color =
        (Color){value, value, value, 255};
}

EntityArmSwing Entity_EvaluateArmSwing(float progress) {
    progress = Clamp(progress, 0.0f, 1.0f);
    return (EntityArmSwing) {
        .arc = sinf(sqrtf(progress) * PI),
        .twist = sinf(progress * progress * PI)
    };
}

void EntityAnimation_Init(EntityAnimation *animation, Vector3 position) {
    *animation = (EntityAnimation) {0};
    animation->lastPosition = position;
}

void EntityAnimation_Start(EntityAnimation *animation, EntityAnimationType type) {
    if (type < ENTITY_ANIMATION_SWING_RIGHT_ARM || type > ENTITY_ANIMATION_SWING_LEFT_ARM) return;
    animation->armSwingTime[type] = 0.0f;
    animation->armSwinging[type] = true;
}

float EntityAnimation_GetSwingProgress(const EntityAnimation *animation, EntityAnimationType type) {
    if (type < ENTITY_ANIMATION_SWING_RIGHT_ARM || type > ENTITY_ANIMATION_SWING_LEFT_ARM) return 0.0f;
    if (!animation->armSwinging[type]) return 0.0f;
    return Clamp(animation->armSwingTime[type] / ENTITY_ARM_SWING_DURATION, 0.0f, 1.0f);
}

void EntityAnimation_Update(EntityAnimation *animation, Vector3 position, float deltaTime) {
    animation->timeSinceMovement += deltaTime;
    float movedX = position.x - animation->lastPosition.x;
    float movedZ = position.z - animation->lastPosition.z;
    float horizontalDistance = sqrtf(movedX * movedX + movedZ * movedZ);
    animation->lastPosition = position;

    if (horizontalDistance > 0.0001f) {
        float movementInterval = fmaxf(animation->timeSinceMovement, deltaTime);
        animation->walkSpeed = horizontalDistance / movementInterval;
        animation->timeSinceMovement = 0.0f;
    }

    bool recentlyMoving = animation->timeSinceMovement < 0.1f;
    if (recentlyMoving) animation->walkTime += animation->walkSpeed * deltaTime * 1.5f;
    float targetWalkAmount = recentlyMoving
        ? Clamp(animation->walkSpeed / 7.5f, 0.0f, 1.0f)
        : 0.0f;
    animation->walkAmount = Lerp(animation->walkAmount, targetWalkAmount,
                                 Clamp(deltaTime * 12.0f, 0.0f, 1.0f));

    for (int i = 0; i < 2; i++) {
        if (animation->armSwinging[i]) {
            animation->armSwingTime[i] += deltaTime;
            if (animation->armSwingTime[i] >= ENTITY_ARM_SWING_DURATION) {
                animation->armSwingTime[i] = ENTITY_ARM_SWING_DURATION;
                animation->armSwinging[i] = false;
            }
        }
    }
}

static void Entity_ApplyThirdPersonAnimation(Entity *entity) {
    float rightProgress = EntityAnimation_GetSwingProgress(
        &entity->animation, ENTITY_ANIMATION_SWING_RIGHT_ARM);
    float leftProgress = EntityAnimation_GetSwingProgress(
        &entity->animation, ENTITY_ANIMATION_SWING_LEFT_ARM);
    EntityArmSwing rightSwing = Entity_EvaluateArmSwing(rightProgress);
    EntityArmSwing leftSwing = Entity_EvaluateArmSwing(leftProgress);
    float legRotation = cosf(entity->animation.walkTime) * 1.2f * entity->animation.walkAmount;
    float idleAmount = 1.0f - entity->animation.walkAmount;
    float idleForward = sinf((float)GetTime() * 0.9f) * 0.08f * idleAmount;
    float idleSide = cosf((float)GetTime() * 0.95f) * 0.05f * idleAmount;
    float rightAttackWeight = entity->animation.armSwinging[ENTITY_ANIMATION_SWING_RIGHT_ARM]
        ? sinf(rightProgress * PI) : 0.0f;
    float leftAttackWeight = entity->animation.armSwinging[ENTITY_ANIMATION_SWING_LEFT_ARM]
        ? sinf(leftProgress * PI) : 0.0f;
    Vector3 rightArmRotation = Vector3Add(
        (Vector3) {-rightSwing.arc * 1.4f, -rightSwing.twist * 0.35f, rightSwing.twist * 0.20f},
        Vector3Scale((Vector3) {-legRotation + idleForward, 0.0f, idleSide},
                     1.0f - rightAttackWeight)
    );
    Vector3 leftArmRotation = Vector3Add(
        (Vector3) {-leftSwing.arc * 1.4f, leftSwing.twist * 0.35f, -leftSwing.twist * 0.20f},
        Vector3Scale((Vector3) {legRotation - idleForward, 0.0f, -idleSide},
                     1.0f - leftAttackWeight)
    );

    for (int i = 0; i < entity->model.partCount; i++) {
        EntityModelPart *part = &entity->model.parts[i];
        if (part->type == PART_TYPE_RIGHT_ARM) part->rotation = rightArmRotation;
        if (part->type == PART_TYPE_LEFT_ARM) part->rotation = leftArmRotation;
        if (part->type == PART_TYPE_RIGHT_LEG) part->rotation.x = legRotation;
        if (part->type == PART_TYPE_LEFT_LEG) part->rotation.x = -legRotation;
    }
}

static void Entity_DrawFiltered(Entity *entity, bool firstPersonOnly) {
    EntityModel *model = &entity->model;
    Entity_ApplyBrightness(entity);
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
    Entity_ApplyThirdPersonAnimation(entity);
    Entity_DrawFiltered(entity, false);
}

void Entity_DrawFirstPerson(Entity *entity, Camera camera, float swingProgress) {
    Matrix cameraTransform = MatrixInvert(GetCameraMatrix(camera));

    EntityArmSwing swing = Entity_EvaluateArmSwing(swingProgress);
    Vector3 rotation = {
        (-60.0f - swing.arc * 30.0f) * DEG2RAD,
        0.0f,
        (-190.0f + swing.twist * 35.0f) * DEG2RAD
    };

    EntityModel *model = &entity->model;
    Entity_ApplyBrightness(entity);
    for (int i = 0; i < model->partCount; i++) {
        EntityModelPart *part = &model->parts[i];
        if (!part->visibleInFirstPerson) continue;

        Matrix drawMatrix = MatrixRotateXYZ(rotation);
        drawMatrix.m12 = 0.35f - swing.arc * 0.10f;
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
