/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef S_PLAYER_H
#define S_PLAYER_H

#include "raylib.h"

typedef struct Player {
    unsigned char id;
    void *peerPtr;
    char *name;
    int drawDistance;
} Player;

void Player_UpdatePositionRotation(Player* player, Vector3 position, Vector3 rotation);
void Player_LoadChunks(Player* player);

#endif