/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "textcolor.h"

static const Color defaultColors[16] = {
    { 32,  34,  40, 255}, // 0: charcoal
    {108, 113, 125, 255}, // 1: dark gray
    {183, 186, 194, 255}, // 2: light gray
    { 82,  99, 160, 255}, // 3: dark blue
    { 85, 140, 101, 255}, // 4: dark green
    {168,  88,  94, 255}, // 5: dark red
    { 79, 143, 149, 255}, // 6: teal
    {148, 100, 158, 255}, // 7: purple
    {221, 174,  99, 255}, // 8: gold
    {238, 217, 153, 255}, // 9: yellow
    {215, 159, 222, 255}, // a: pink
    {235, 128, 128, 255}, // b: red
    {151, 211, 163, 255}, // c: green
    {139, 163, 232, 255}, // d: blue
    {144, 213, 219, 255}, // e: cyan
    {255, 255, 255, 255}, // f: white
};

bool TextColor_ValidCode(unsigned char code) {
    return code > 32 && code < 127 && code != '&' && code != '%';
}

bool TextColor_Get(const Color *palette, unsigned char code, Color *color) {
    if (palette[code].a) { 
        *color = palette[code]; 
        return true; 
    } else { //Default colors
        int index = -1;
        if (code >= '0' && code <= '9') index = code - '0';
        if (code >= 'a' && code <= 'f') index = code - 'a' + 10;
        if (code >= 'A' && code <= 'F') index = code - 'A' + 10;
        if (index < 0) return false;
        *color = defaultColors[index];
        return true;
    }
}

// Return one visible UTF-8 codepoint, consuming any preceding color codes.
int TextColor_Next(const char **text, const Color *palette, Color *color) {
    const unsigned char *p = (const unsigned char *)*text;
    while (*p == '&' && p[1]) {
        if (p[1] == '&') { 
            *text = (const char *)p + 2; 
            return '&'; 
        }
        if (!TextColor_Get(palette, p[1], color)) break;
        p += 2;
    }
    if (!*p) { *text = (const char *)p; return 0; }
    int code = *p++, count = 0, minimum = 0;
    if (code >= 0xc2 && code <= 0xdf) { code &= 31; count = 1; minimum = 128; }
    else if (code >= 0xe0 && code <= 0xef) { code &= 15; count = 2; minimum = 2048; }
    else if (code >= 0xf0 && code <= 0xf4) { code &= 7; count = 3; minimum = 65536; }
    else if (code >= 128) code = '?';
    for (int i = 0; i < count; i++) {
        if ((*p & 0xc0) != 0x80) { 
            *text = (const char *)p; 
            return '?'; 
        }
        code = (code << 6) | (*p++ & 63);
    }
    *text = (const char *)p;
    if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return '?';
    return code;
}

size_t TextColor_Escape(char *output, const char *text) {
    size_t length = 0;
    while (*text) {
        if (*text == '&') { 
            if (output) output[length] = '&'; 
            length++; 
        }
        if (output) output[length] = *text;
        length++;
        text++;
    }
    if (output) output[length] = 0;
    return length;
}
