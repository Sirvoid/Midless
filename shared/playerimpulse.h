/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_PLAYER_IMPULSE_H
#define MIDLESS_PLAYER_IMPULSE_H

#include "packetopcodes.h"
#include "raylib.h"
#include <math.h>
#define PLAYER_IMPULSE_PACKET_SIZE 13
#define PLAYER_IMPULSE_SCALE 1000.0f
#define PLAYER_IMPULSE_WINDOW 3.0
static inline bool PlayerImpulse_Valid(Vector3 v) {
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z) &&
        fabsf(v.x) <= 20 && fabsf(v.y) <= 20 && fabsf(v.z) <= 20;
}
// Client velocity is measured in blocks per 60 Hz frame.
static inline Vector3 PlayerImpulse_Add(Vector3 velocity, Vector3 impulse) {
    return (Vector3){fmaxf(-0.5f, fminf(0.5f, velocity.x + impulse.x / 60)),
        fmaxf(-1, fminf(0.5f, velocity.y + impulse.y / 60)),
        fmaxf(-0.5f, fminf(0.5f, velocity.z + impulse.z / 60))};
}
#endif
