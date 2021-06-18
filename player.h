#ifndef G_PLAYER_H
#define G_PLAYER_H

#include "raylib.h"
#include "raycast.h"

typedef struct Player{
    Camera camera;
    float speed;
    Vector3 position;
    Vector3 velocity;
    BoundingBox collisionBox;
    RaycastResult rayResult;
    bool jumped;
} Player;

//Initialize a player.
void Player_Init(Player *player);

//Check/Do Inputs
void Player_CheckInputs(Player *player);

//Update a player.
void Player_Update(Player *player);

bool Player_TestCollision(Player *player);

#endif