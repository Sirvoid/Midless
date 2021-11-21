/**
 * Copyright (c) 2021 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "raylib.h"
#include "raymath.h"
#include "entity.h"

void Entity_Draw(Entity *entity) {
    DrawCube(Vector3Add(entity->position, (Vector3){0.5f, 1, 0.5f}), 1, 2, 1, (Color){255, 255, 255, 255});
}