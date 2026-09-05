/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SERVER_ENTITY_H
#define MIDLESS_SERVER_ENTITY_H

#include "raylib.h"
#include <stdint.h>

typedef struct Entity{
    int id;
    uint64_t generation;
    bool active, pendingRemoval, dirty, announced;
    int ownerPlayerId;
    int definitionId;
    int scriptRef;
    char type;
    unsigned char model;
    Vector3 position;
    Vector3 rotation;
} Entity;

#endif
