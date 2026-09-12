/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_HUD_BAR_PROTOCOL_H
#define MIDLESS_HUD_BAR_PROTOCOL_H

#include "packetopcodes.h"

#include "packetsizes.h"

#include <stdbool.h>
#include <stdint.h>

#define HUD_BAR_LIMIT 8
#define HUD_BAR_MAX_ICONS 16

typedef struct HudBarDefinition {
    bool defined;
    uint8_t texture, icons;
    uint16_t priority, max;
} HudBarDefinition;

typedef struct HudBarState {
    uint16_t value;
    bool visible;
} HudBarState;

void HudBar_WriteDefinition(uint8_t *packet, int id, const HudBarDefinition *bar);
bool HudBar_ReadDefinition(const uint8_t *packet, int length, int *id, HudBarDefinition *bar);
void HudBar_WriteState(uint8_t *packet, int id, HudBarState state);
bool HudBar_ReadState(const uint8_t *packet, int length, int *id, HudBarState *state);
// Visible bars sorted by priority, then registry ID;
int HudBar_VisibleOrder(const HudBarDefinition *bars, const HudBarState *states, int *order);

#endif
