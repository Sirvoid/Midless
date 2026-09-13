/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */
#include "worldstate.h"
#include "../savedatabase.h"

bool WorldState_Load(WorldState *state) {
    *state = (WorldState){0};
    return SaveDatabase_LoadTime(&state->time) != SAVE_ERROR;
}

bool WorldState_Save(const WorldState *state) {
    return SaveDatabase_SaveTime(state->time);
}
