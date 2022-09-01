/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_ENTITY_H
#define G_ENTITY_H

#include "raylib.h"
#include "entityModel.h"

typedef struct Entity{
    char type;
    Vector3 position;
    Vector3 rotation;
    EntityModel model;
} Entity;

void Entity_Draw(Entity *entity);
void Entity_Remove(Entity *entity);

#endif