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

typedef struct Entity{
    char type;
    unsigned char modelId;
    Vector3 position;
    Vector3 rotation;
    EntityModel model;
} Entity;

void Entity_Draw(Entity *entity);
void Entity_DrawFirstPerson(Entity *entity, Camera camera);
void Entity_Destroy(Entity *entity);

#endif
