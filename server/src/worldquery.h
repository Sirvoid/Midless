/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_WORLD_QUERY_H
#define MIDLESS_WORLD_QUERY_H
#include "entity.h"

typedef struct WorldHit {
    enum { HIT_NOTHING, HIT_BLOCK, HIT_ENTITY, HIT_UNLOADED } type;
    Vector3 position, block, normal;
    float distance;
    int entityId;
} WorldHit;

bool ServerQuery_Block(Vector3 cell, BlockShape *shape);
bool ServerQuery_Clear(EntityBody body, Vector3 position);
bool ServerQuery_CanWalk(EntityBody body, Vector3 from, Vector3 to);
WorldHit ServerQuery_Raycast(Vector3 from, Vector3 to, bool entities, int ignoreId);
// Loaded ground only; returns zero when no complete route fits the search budget.
int ServerQuery_FindPath(EntityBody body, Vector3 from, Vector3 to, Vector3 *path, int capacity);
#endif
