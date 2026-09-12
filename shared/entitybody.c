/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <math.h>
#include "entitybody.h"

#define CONTACT_EPSILON 0.0001f

EntityBody EntityBody_Default(void) {
    return (EntityBody){.localBounds = {{-0.3f, 0, -0.3f}, {0.3f, 1.5f, 0.3f}},
        .gravityScale = 1, .groundFriction = 8};
}

bool EntityBody_Validate(const EntityBody *body) {
    float minimum[] = {body->localBounds.min.x, body->localBounds.min.y, body->localBounds.min.z};
    float maximum[] = {body->localBounds.max.x, body->localBounds.max.y, body->localBounds.max.z};
    float velocity[] = {body->velocity.x, body->velocity.y, body->velocity.z};
    for (int axis = 0; axis < 3; axis++) {
        if (!isfinite(minimum[axis]) || !isfinite(maximum[axis]) || minimum[axis] >= maximum[axis] ||
            minimum[axis] < -4 || maximum[axis] > 4 || !isfinite(velocity[axis]) || fabsf(velocity[axis]) > 100) return false;
    }
    return isfinite(body->gravityScale) && body->gravityScale >= 0 && body->gravityScale <= 10 &&
        isfinite(body->groundFriction) && body->groundFriction >= 0 && body->groundFriction <= 100 &&
        isfinite(body->restitution) && body->restitution >= 0 && body->restitution <= 1;
}

BoundingBox EntityBody_Bounds(const EntityBody *body, Vector3 position) {
    BoundingBox bounds = body->localBounds;
    bounds.min.x += position.x; bounds.max.x += position.x;
    bounds.min.y += position.y; bounds.max.y += position.y;
    bounds.min.z += position.z; bounds.max.z += position.z;
    return bounds;
}

static bool Overlaps(BoundingBox a, BoundingBox b) {
    return a.min.x < b.max.x && a.max.x > b.min.x && a.min.y < b.max.y &&
        a.max.y > b.min.y && a.min.z < b.max.z && a.max.z > b.min.z;
}

// Sweep the body by expanding a block by its extents and tracing the body origin.
static bool Sweep(BoundingBox body, BoundingBox block, Vector3 move, float *time, int *axisHit) {
    float minimum[] = {body.min.x, body.min.y, body.min.z};
    float maximum[] = {body.max.x, body.max.y, body.max.z};
    float blockMin[] = {block.min.x, block.min.y, block.min.z};
    float blockMax[] = {block.max.x, block.max.y, block.max.z};
    float delta[] = {move.x, move.y, move.z};
    float entry = -INFINITY, exit = INFINITY;
    int normal = -1;
    for (int axis = 0; axis < 3; axis++) {
        if (fabsf(delta[axis]) < 0.0000001f) {
            if (maximum[axis] <= blockMin[axis] || minimum[axis] >= blockMax[axis]) return false;
            continue;
        }
        float first = (blockMin[axis] - maximum[axis]) / delta[axis];
        float last = (blockMax[axis] - minimum[axis]) / delta[axis];
        if (first > last) { float swap = first; first = last; last = swap; }
        if (first > entry) { entry = first; normal = axis; }
        exit = fminf(exit, last);
    }
    if (normal < 0 || entry > exit || entry < -CONTACT_EPSILON || entry > 1 || exit < 0) return false;
    *time = fmaxf(0, entry);
    *axisHit = normal;
    return true;
}

static BlockShape Query(void *context, EntityBody_QueryBlock query, Vector3 cell, bool *loaded) {
    BlockShape shape = {0};
    *loaded = query(context, cell, &shape);
    if (!*loaded) shape = (BlockShape){.solid = true, .bounds = {cell, {cell.x + 1, cell.y + 1, cell.z + 1}}};
    if(!*loaded) { shape.collisionCount=1; shape.collision[0]=shape.bounds; }
    return shape;
}

void EntityBody_Step(EntityBody *body, Vector3 *position, float dt, EntityBody_QueryBlock query, void *context) {
    if (!body->enabled || !EntityBody_Validate(body) || !isfinite(dt) || dt <= 0 || dt > 0.05f ||
        !isfinite(position->x) || !isfinite(position->y) || !isfinite(position->z) ||
        fabsf(position->x) > 1000000 || fabsf(position->y) > 1000000 || fabsf(position->z) > 1000000) return;
    body->blockedByUnloaded = false;
    for (int attempt = 0; attempt < 8; attempt++) {
        BoundingBox bounds = EntityBody_Bounds(body, *position);
        Vector3 correction = {0};
        float distance = INFINITY;
        for (int x = (int)floorf(bounds.min.x); x <= (int)floorf(bounds.max.x); x++)
        for (int y = (int)floorf(bounds.min.y); y <= (int)floorf(bounds.max.y); y++)
        for (int z = (int)floorf(bounds.min.z); z <= (int)floorf(bounds.max.z); z++) {
            bool loaded;
            BlockShape shape = Query(context, query, (Vector3){x,y,z}, &loaded);
            for(int box=0;box<shape.collisionCount;box++) {
                BoundingBox obstacle=shape.collision[box];
                if (!shape.solid || !Overlaps(bounds, obstacle)) continue;
                if (!loaded) { body->blockedByUnloaded = true; return; }
                float offsets[] = {obstacle.min.x - bounds.max.x, obstacle.max.x - bounds.min.x,
                    obstacle.min.y - bounds.max.y, obstacle.max.y - bounds.min.y,
                    obstacle.min.z - bounds.max.z, obstacle.max.z - bounds.min.z};
                for (int face = 0; face < 6; face++) {
                    if (fabsf(offsets[face]) >= distance) continue;
                    distance = fabsf(offsets[face]);
                    float offset = offsets[face] + (face % 2 ? CONTACT_EPSILON : -CONTACT_EPSILON);
                    correction = (Vector3){face / 2 == 0 ? offset : 0, face / 2 == 1 ? offset : 0, face / 2 == 2 ? offset : 0};
                }
            }
        }
        if (!isfinite(distance)) break;
        if (attempt == 7 || distance > 0.5f) { body->velocity = (Vector3){0}; return; }
        position->x += correction.x; position->y += correction.y; position->z += correction.z;
        body->sleeping = false;
    }
    
    BoundingBox bounds = EntityBody_Bounds(body, *position);
    bool supported = false;
    for (int x = (int)floorf(bounds.min.x); x <= (int)floorf(bounds.max.x); x++)
    for (int z = (int)floorf(bounds.min.z); z <= (int)floorf(bounds.max.z); z++) {
        bool loaded;
        BlockShape shape = Query(context, query, (Vector3){x, floorf(bounds.min.y - 0.01f), z}, &loaded);
        float time; int axis;
        for(int box=0;box<shape.collisionCount;box++)
        if (shape.solid && Sweep(bounds, shape.collision[box], (Vector3){0,-0.01f,0}, &time, &axis)) {
            supported = true;
            if (!loaded) body->blockedByUnloaded = true;
        }
    }
    if (body->sleeping && supported) return;
    body->sleeping = false;
    body->grounded = false;
    body->velocity.y = fmaxf(-100, body->velocity.y - 20 * body->gravityScale * dt);
    float remaining = dt;
    for (int contact = 0; contact < 6 && remaining > 0.000001f; contact++) {
        bounds = EntityBody_Bounds(body, *position);
        Vector3 move = {body->velocity.x * remaining, body->velocity.y * remaining, body->velocity.z * remaining};
        float earliest = 1;
        int hitAxis = -1;
        bool unloadedHit = false;
        for (int x = (int)floorf(bounds.min.x + fminf(0,move.x)); x <= (int)floorf(bounds.max.x + fmaxf(0,move.x)); x++)
        for (int y = (int)floorf(bounds.min.y + fminf(0,move.y)); y <= (int)floorf(bounds.max.y + fmaxf(0,move.y)); y++)
        for (int z = (int)floorf(bounds.min.z + fminf(0,move.z)); z <= (int)floorf(bounds.max.z + fmaxf(0,move.z)); z++) {
            bool loaded;
            BlockShape shape = Query(context, query, (Vector3){x,y,z}, &loaded);
            float time; int axis;
            for(int box=0;box<shape.collisionCount;box++)
            if (shape.solid && Sweep(bounds, shape.collision[box], move, &time, &axis) && time <= earliest) {
                earliest = time; hitAxis = axis; unloadedHit = !loaded;
            }
        }
        position->x += move.x * earliest; position->y += move.y * earliest; position->z += move.z * earliest;
        if (hitAxis < 0) break;
        float *velocity = hitAxis == 0 ? &body->velocity.x : hitAxis == 1 ? &body->velocity.y : &body->velocity.z;
        float *coordinate = hitAxis == 0 ? &position->x : hitAxis == 1 ? &position->y : &position->z;
        if (unloadedHit) { body->blockedByUnloaded = true; break; }
        if (hitAxis == 1 && *velocity < 0) body->grounded = true;
        *coordinate += *velocity > 0 ? -CONTACT_EPSILON : CONTACT_EPSILON;
        *velocity = fabsf(*velocity) < 1 ? 0 : -*velocity * body->restitution;
        remaining *= 1 - earliest;
    }
    if (body->grounded) {
        float drag = expf(-body->groundFriction * dt);
        body->velocity.x *= drag; body->velocity.z *= drag;
        if (fabsf(body->velocity.x) + fabsf(body->velocity.y) + fabsf(body->velocity.z) < 0.02f) {
            body->velocity = (Vector3){0}; body->sleeping = true;
        }
    }
}
