#ifndef G_ENTITY_H
#define G_ENTITY_H

#include "raylib.h"

typedef struct Entity{
    char type;
    Vector3 position;
} Entity;

void Entity_Draw(Entity *entity);

#endif