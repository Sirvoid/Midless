/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SERVER_ENTITY_H
#define MIDLESS_SERVER_ENTITY_H

#include "raylib.h"

typedef struct Entity{
    int id;
    char type;
    unsigned char model;
    Vector3 position;
    Vector3 rotation;
} Entity;

#endif
