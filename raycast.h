/**
 * Copyright (c) 2021 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_RAYCAST_H
#define G_RAYCAST_H

#include "raylib.h"

typedef struct RaycastResult {
    Vector3 hitPos;
    Vector3 prevPos;
    int hitBlockID;
} RaycastResult;

RaycastResult Raycast_Do(Vector3 position, Vector3 direction);


#endif