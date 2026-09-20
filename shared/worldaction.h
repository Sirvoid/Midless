/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_WORLD_ACTION_H
#define MIDLESS_WORLD_ACTION_H
#include <stdbool.h>
#include <stdint.h>
typedef enum WorldActionType {
    WORLD_START_DIG,
    WORLD_CANCEL_DIG,
    WORLD_USE
} WorldActionType;
typedef struct WorldAction {
    uint32_t sequence, inventorySequence;
    uint8_t type;
    bool clicked;
    int32_t x, y, z;
    uint16_t targetBlock;
    uint8_t face, hit[3];
} WorldAction;
#endif
