/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_PLAYER_H
#define MIDLESS_CLIENT_PLAYER_H

#include "raylib.h"
#include "raycast.h"
#include "entitymodel.h"
#include "entity.h"

typedef enum PlayerCameraMode {
    PLAYER_CAMERA_FIRST_PERSON,
    PLAYER_CAMERA_THIRD_PERSON_BEHIND,
    PLAYER_CAMERA_THIRD_PERSON_FRONT
} PlayerCameraMode;

typedef struct Player{
    Camera camera;
    float speed;
    Vector3 position;
    Vector3 direction;
    Vector3 velocity;
    BoundingBox collisionBox;
    RaycastResult rayResult;
    int blockSelected;
    bool canJump;
    unsigned char entityType;
    unsigned char modelId;
    bool hasEntityModel;
    PlayerCameraMode cameraMode;
    EntityModel entityModel;
    EntityAnimation animation;
} Player;
extern Player player;

//Initialize a player.
void Player_Init(void);

//Check/Do Inputs
void Player_CheckInputs(void);

//Update a player.
void Player_Update(void);
void Player_Draw(void);
void Player_SetEntityModel(int type, int modelId);
void Player_ClearEntityModel(void);
void Player_Teleport(Vector3 position);

bool Player_TryPlaceBlock(Vector3 pos, int blockId);

bool Player_TestCollision(Vector3 offset);
Vector3 Player_GetForwardVector(void);

//Get player position in chunk units.
Vector3 Player_GetChunkPosition(void);

#endif
