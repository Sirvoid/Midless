/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef S_ENTITY_H
#define S_ENTITY_H

#include "raylib.h"

typedef struct Entity{
    int ID;
    char type;
    Vector3 position;
    Vector3 rotation;
} Entity;

#endif