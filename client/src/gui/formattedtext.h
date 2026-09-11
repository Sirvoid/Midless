#ifndef MIDLESS_FORMATTED_TEXT_H
#define MIDLESS_FORMATTED_TEXT_H
#include "textcolor.h"

typedef struct TextGlyph {
    int codepoint, line;
    float x;
    Color color;
} TextGlyph;

extern Color textColors[256];
int FormattedText_Layout(const char *text, Color color, float fontSize, float wrapWidth,
                         TextGlyph *glyphs, int capacity, int *lines, float *width);
void FormattedText_DrawGlyph(TextGlyph glyph, Vector2 position, float fontSize, float opacity);
#endif
