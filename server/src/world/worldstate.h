/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */
#ifndef MIDLESS_WORLD_STATE_H
#define MIDLESS_WORLD_STATE_H

#include <stdbool.h>

typedef struct WorldState {
    float time;
} WorldState;

// Missing values are valid new worlds. Errors leave the state at defaults.
bool WorldState_Load(WorldState *state);
bool WorldState_Save(const WorldState *state);

#endif
