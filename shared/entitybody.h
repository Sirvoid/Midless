/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_ENTITY_BODY_H
#define MIDLESS_ENTITY_BODY_H
#include "blockshape.h"

typedef struct EntityBody {
    Vector3 velocity;
    BoundingBox localBounds;
    float gravityScale, groundFriction, restitution;
    bool enabled, grounded, sleeping, blockedByUnloaded;
} EntityBody;

typedef bool (*EntityBody_QueryBlock)(void *context, Vector3 cell, BlockShape *shape);
EntityBody EntityBody_Default(void);
bool EntityBody_Validate(const EntityBody *body);
BoundingBox EntityBody_Bounds(const EntityBody *body, Vector3 position);
void EntityBody_Step(EntityBody *body, Vector3 *position, float dt, EntityBody_QueryBlock query, void *context);
#endif
