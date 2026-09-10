#include "hudbarprotocol.h"
#include "textureprotocol.h"

void HudBar_WriteDefinition(uint8_t *packet, int id, const HudBarDefinition *bar) {
    packet[0] = PACKET_DEFINE_HUD_BAR;
    packet[1] = id;
    packet[2] = bar->texture;
    packet[3] = bar->icons;
    Texture_Write16(packet + 4, bar->priority);
    Texture_Write16(packet + 6, bar->max);
}

bool HudBar_ReadDefinition(const uint8_t *packet, int length, int *id, HudBarDefinition *bar) {
    if (length != HUD_BAR_DEFINE_SIZE || packet[0] != PACKET_DEFINE_HUD_BAR ||
        packet[1] >= HUD_BAR_LIMIT || packet[2] < 2 || packet[2] >= TEXTURE_LIMIT ||
        !packet[3] || packet[3] > HUD_BAR_MAX_ICONS || !Texture_Read16(packet + 6)) return false;
    *id = packet[1];
    *bar = (HudBarDefinition){true, packet[2], packet[3], Texture_Read16(packet + 4), Texture_Read16(packet + 6)};
    return true;
}

void HudBar_WriteState(uint8_t *packet, int id, HudBarState state) {
    packet[0] = PACKET_SET_HUD_BAR;
    packet[1] = id;
    Texture_Write16(packet + 2, state.value);
    packet[4] = state.visible;
}

bool HudBar_ReadState(const uint8_t *packet, int length, int *id, HudBarState *state) {
    if (length != HUD_BAR_STATE_SIZE || packet[0] != PACKET_SET_HUD_BAR ||
        packet[1] >= HUD_BAR_LIMIT || packet[4] > 1) return false;
    *id = packet[1];
    *state = (HudBarState){Texture_Read16(packet + 2), packet[4] != 0};
    return true;
}

int HudBar_VisibleOrder(const HudBarDefinition *bars, const HudBarState *states, int *order) {
    int count = 0;
    for (int id = 0; id < HUD_BAR_LIMIT; id++) {
        if (!bars[id].defined || !states[id].visible) continue;
        int position = count++;
        while (position > 0 && bars[order[position - 1]].priority > bars[id].priority) {
            order[position] = order[position - 1];
            position--;
        }
        order[position] = id;
    }
    return count;
}
