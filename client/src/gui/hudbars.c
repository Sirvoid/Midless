#include <math.h>
#include <string.h>
#include "hudbars.h"
#include "hudbarprotocol.h"
#include "../textures.h"

static HudBarDefinition bars[HUD_BAR_LIMIT];
static HudBarState states[HUD_BAR_LIMIT];

void ClientHudBars_Reset(void) {
    memset(bars, 0, sizeof(bars));
    memset(states, 0, sizeof(states));
}

void ClientHudBars_Define(int id, HudBarDefinition bar) {
    bars[id] = bar;
    if (states[id].value > bar.max) states[id].value = bar.max;
}

void ClientHudBars_Set(int id, HudBarState state) {
    if (!bars[id].defined) return;
    if (state.value > bars[id].max) state.value = bars[id].max;
    states[id] = state;
}

void ClientHudBars_Remove(int id) {
    bars[id] = (HudBarDefinition){0};
    states[id] = (HudBarState){0};
}

void ClientHudBars_Draw(Rectangle hotbar) {
    int order[HUD_BAR_LIMIT];
    int count = HudBar_VisibleOrder(bars, states, order);
    float gap = fmaxf(2, hotbar.height / 12);
    float columnWidth = (hotbar.width - gap) / 2;
    float rowHeight = hotbar.height * 0.4f;
    for (int index = 0; index < count; index++) {
        int id = order[index];
        const HudBarDefinition *bar = &bars[id];
        Texture2D texture = ClientTextures_Get(bar->texture);
        if (!texture.id || texture.width != 9 || texture.height != 9) continue;
        bool right = index % 2 != 0;
        // Each texture pixel covers a whole number of screen pixels.
        int scale = (int)floorf(fminf(rowHeight / 9, columnWidth / (bar->icons * 8 + 1)));
        if (scale < 1) continue;
        int size = 9 * scale;
        int spacing = 8 * scale; // Adjacent icons share their edge pixel.
        float y = roundf(hotbar.y - gap - size - (index / 2) * (rowHeight + gap));
        float filled = (float)states[id].value * bar->icons / bar->max;
        for (int icon = 0; icon < bar->icons; icon++) {
            float x = roundf(right ? hotbar.x + hotbar.width - size - (bar->icons - 1 - icon) * spacing : hotbar.x + icon * spacing);
            DrawTexturePro(texture, (Rectangle){0, 0, 9, 9}, (Rectangle){x, y, size, size},
                           (Vector2){0}, 0, (Color){70, 70, 70, 160});
            float fraction = fminf(1, fmaxf(0, filled - icon));
            int pixels = (int)roundf(9 * fraction);
            if (pixels <= 0) continue;
            DrawTexturePro(texture, (Rectangle){0, 0, pixels, 9},
                           (Rectangle){x, y, pixels * scale, size}, (Vector2){0}, 0, WHITE);
        }
    }
}
