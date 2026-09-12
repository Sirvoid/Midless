/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_STREAM_PROFILE_H
#define MIDLESS_STREAM_PROFILE_H

#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"

// Each counter has one owning thread. Worker durations are passed back in jobs.
typedef struct StreamProfile {
    unsigned int count;
    double total, maximum, reportedAt;
} StreamProfile;

static inline void StreamProfile_Add(StreamProfile *profile, const char *stage, double seconds) {
    profile->count++;
    profile->total += seconds;
    if (seconds > profile->maximum) profile->maximum = seconds;
    double now = GetTime();
    if (now - profile->reportedAt < 5.0) return;
    if (getenv("MIDLESS_STREAM_PROFILE")) {
        fprintf(stderr, "Streaming %s: %u jobs, %.2f ms average, %.2f ms max\n",
                stage, profile->count, profile->total * 1000 / profile->count,
                profile->maximum * 1000);
    }
    profile->count = 0;
    profile->total = profile->maximum = 0;
    profile->reportedAt = now;
}

#endif
