/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_RAYCAST_H
#define MIDLESS_CLIENT_RAYCAST_H

#include "raylib.h"

typedef struct RaycastResult {
    Vector3 hitPos;
    Vector3 prevPos;
    int hitblockId;
    Vector3 normal;
} RaycastResult;

RaycastResult Raycast_Cast(Vector3 position, Vector3 direction, bool ignoreLiquid);


#endif