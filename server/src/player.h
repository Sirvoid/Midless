/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_PLAYER_H
#define G_PLAYER_H

#include "raylib.h"

typedef struct Player {
    unsigned char id;
    void *peerPtr;
    char *name;
    Vector3 position;
    Vector2 rotation;
} Player;

#endif