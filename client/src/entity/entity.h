/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_ENTITY_H
#define MIDLESS_CLIENT_ENTITY_H

#include "raylib.h"
#include "entitymodel.h"
#include "entityanimation.h"

#define ENTITY_ARM_SWING_DURATION 0.275f

typedef struct EntityAnimation {
    float armSwingTime[2];
    bool armSwinging[2];
    float walkTime;
    float walkAmount;
    float walkSpeed;
    float timeSinceMovement;
    Vector3 lastPosition;
} EntityAnimation;

typedef struct Entity{
    char type;
    unsigned char modelId;
    Vector3 position;
    Vector3 rotation;
    Vector3 targetPosition;
    Vector3 targetRotation;
    float targetHeadPitch;
    EntityModel model;
    EntityAnimation animation;
} Entity;

typedef struct EntityArmSwing {
    float arc;
    float twist;
} EntityArmSwing;

EntityArmSwing Entity_EvaluateArmSwing(float progress);
void EntityAnimation_Init(EntityAnimation *animation, Vector3 position);
void EntityAnimation_Start(EntityAnimation *animation, EntityAnimationType type);
void EntityAnimation_Update(EntityAnimation *animation, Vector3 position, float deltaTime);
float EntityAnimation_GetSwingProgress(const EntityAnimation *animation, EntityAnimationType type);
void Entity_Draw(Entity *entity);
void Entity_DrawFirstPerson(Entity *entity, Camera camera, float swingProgress);
void Entity_Destroy(Entity *entity);

#endif
