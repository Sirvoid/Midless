#ifndef MIDLESS_HUD_BAR_PROTOCOL_H
#define MIDLESS_HUD_BAR_PROTOCOL_H

#include "packetsizes.h"

#include <stdbool.h>
#include <stdint.h>

#define HUD_BAR_LIMIT 8
#define HUD_BAR_MAX_ICONS 16
#define PACKET_DEFINE_HUD_BAR 27
#define PACKET_SET_HUD_BAR 28
#define PACKET_REMOVE_HUD_BAR 29

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
