/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_ENTITY_MODEL_PART_H
#define MIDLESS_CLIENT_ENTITY_MODEL_PART_H

#include "raylib.h"

typedef struct EntityModelPart{
    Mesh mesh;
    Vector3 position;
    Vector3 rotation;
    int type;
} EntityModelPart;

void EntityModelPart_Build(EntityModelPart *part, BoundingBox box, Rectangle *uvs, Vector2 textureSize, Vector3 position);

#endif