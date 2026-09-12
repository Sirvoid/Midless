/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_TEXT_COLOR_H
#define MIDLESS_TEXT_COLOR_H

#include "packetopcodes.h"

#include "raylib.h"
#include <stddef.h>

#define TEXT_COLOR_PACKET_SIZE 6
#define NAMETAG_TEXT_SIZE 129
#define NAMETAG_PACKET_SIZE (3 + NAMETAG_TEXT_SIZE + 4 + 1 + 4)

typedef struct Nametag {
    char text[NAMETAG_TEXT_SIZE];
    Color color;
    bool visible;
    float offset;
} Nametag;

bool TextColor_ValidCode(unsigned char code);
bool TextColor_Get(const Color *palette, unsigned char code, Color *color);
int TextColor_Next(const char **text, const Color *palette, Color *color);
size_t TextColor_Escape(char *output, const char *text);

#endif
