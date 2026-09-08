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
#include "entitybody.h"
#include "droppeditem.h"
#include "world/chunk/chunkmetadata.h"

typedef struct Entity{
    EntityBody body;
    DroppedItem drop;
    int id;
    uint64_t generation;
    bool active, pendingRemoval, dirty, announced;
    int ownerPlayerId;
    int definitionId;
    int scriptRef;
    unsigned char heldBlock;
    char type;
    unsigned char model;
    Vector3 position;
    Vector3 rotation; // XYZ Euler radians: X pitch, Y yaw, Z roll.
    Metadata metadata;
} Entity;

#endif
