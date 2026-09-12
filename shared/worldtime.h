/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SHARED_WORLD_TIME_H
#define MIDLESS_SHARED_WORLD_TIME_H

#define WORLD_DAY_LENGTH_SECONDS (24 * 60)
#define WORLD_TIME_SYNC_INTERVAL_MILLISECONDS (10 * 1000)
#include <math.h>
static inline float WorldTime_Sunlight(float time) {
    return fmaxf(fabsf(time-WORLD_DAY_LENGTH_SECONDS/2.0f)/(WORLD_DAY_LENGTH_SECONDS/2.0f),2/16.0f);
}

#endif
