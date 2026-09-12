/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <math.h>
#include <string.h>
#include "hudbars.h"
#include "hudbarprotocol.h"
#include "../textures.h"

static HudBarDefinition bars[HUD_BAR_LIMIT];
static HudBarState states[HUD_BAR_LIMIT];
static struct { int value; double until; } losses[HUD_BAR_LIMIT];
#define LOSS_DURATION 0.4

void ClientHudBars_Reset(void) {
    memset(bars, 0, sizeof(bars));
    memset(states, 0, sizeof(states));
    memset(losses, 0, sizeof(losses));
}

void ClientHudBars_Define(int id, HudBarDefinition bar) {
    bars[id] = bar;
    losses[id].until = 0;
    if (states[id].value > bar.max) states[id].value = bar.max;
}

void ClientHudBars_Set(int id, HudBarState state) {
    if (!bars[id].defined) return;
    if (state.value > bars[id].max) state.value = bars[id].max;
    double now = GetTime();
    if (!state.visible || !states[id].visible) losses[id].until = 0;
    else if (state.value < states[id].value) {
        if (losses[id].until <= now || losses[id].value < states[id].value)
            losses[id].value = states[id].value;
        losses[id].until = now + LOSS_DURATION;
    } else if (state.value >= losses[id].value) losses[id].until = 0;
    states[id] = state;
}

void ClientHudBars_Remove(int id) {
    bars[id] = (HudBarDefinition){0};
    states[id] = (HudBarState){0};
    losses[id].until = 0;
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
        double remaining = losses[id].until - GetTime();
        float lostFill = (float)losses[id].value * bar->icons / bar->max;
        for (int icon = 0; icon < bar->icons; icon++) {
            float x = roundf(right ? hotbar.x + hotbar.width - size - (bar->icons - 1 - icon) * spacing : hotbar.x + icon * spacing);
            DrawTexturePro(texture, (Rectangle){0, 0, 9, 9}, (Rectangle){x, y, size, size},
                           (Vector2){0}, 0, (Color){70, 70, 70, 160});
            float fraction = fminf(1, fmaxf(0, filled - icon));
            int pixels = (int)roundf(9 * fraction);
            if (pixels > 0) DrawTexturePro(texture, (Rectangle){0, 0, pixels, 9},
                           (Rectangle){x, y, pixels * scale, size}, (Vector2){0}, 0, WHITE);
            int oldPixels = (int)roundf(9 * fminf(1, fmaxf(0, lostFill - icon)));
            if (remaining > 0 && oldPixels > pixels) {
                float pulse = 0.55f + 0.45f * cosf((float)(LOSS_DURATION - remaining) * 10 * PI);
                Color flash = {255, 180, 180, (unsigned char)(255 * pulse)};
                DrawTexturePro(texture, (Rectangle){pixels, 0, oldPixels - pixels, 9},
                    (Rectangle){x + pixels * scale, y, (oldPixels - pixels) * scale, size},
                    (Vector2){0}, 0, flash);
            }
        }
    }
}
