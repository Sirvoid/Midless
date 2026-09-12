/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_ENTITY_PHYSICS_H
#define MIDLESS_ENTITY_PHYSICS_H
#include "entity.h"
void ServerPhysics_Update(float dt);
void ServerPhysics_Reset(void);
bool ServerPhysics_SetBody(Entity *entity, EntityBody body);
bool ServerPhysics_SetVelocity(Entity *entity, Vector3 velocity);
bool ServerPhysics_ApplyImpulse(Entity *entity, Vector3 impulse);
bool ServerPhysics_Move(Entity *entity, Vector3 direction, float speed, float acceleration);
bool ServerPhysics_Jump(Entity *entity, float speed);
void ServerPhysics_InvalidateIndex(void);
int ServerPhysics_QueryEntities(BoundingBox bounds, int *ids, int capacity);
#endif
