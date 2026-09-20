/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */
#ifndef MIDLESS_COLLISION_H
#define MIDLESS_COLLISION_H
#include <math.h>
#include <stdbool.h>
#include "raylib.h"

static inline bool Collision_LeavingBox(BoundingBox before, BoundingBox after, BoundingBox block) {
    float low[] = {before.min.x, before.min.y, before.min.z};
    float high[] = {before.max.x, before.max.y, before.max.z};
    float nextLow[] = {after.min.x, after.min.y, after.min.z};
    float nextHigh[] = {after.max.x, after.max.y, after.max.z};
    float blockLow[] = {block.min.x, block.min.y, block.min.z};
    float blockHigh[] = {block.max.x, block.max.y, block.max.z};
    bool escaping = false;
    for (int axis = 0; axis < 3; axis++) {
        float depth = fminf(high[axis] - blockLow[axis], blockHigh[axis] - low[axis]);
        float next = fminf(nextHigh[axis] - blockLow[axis], blockHigh[axis] - nextLow[axis]);
        float delta = nextLow[axis] - low[axis];
        float centers = low[axis] + high[axis] - blockLow[axis] - blockHigh[axis];
        if (depth <= 0 || next > depth || delta * centers < 0) return false;
        if (next < depth) escaping = true;
    }
    return escaping;
}
#endif
