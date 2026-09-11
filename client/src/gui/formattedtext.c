#include "formattedtext.h"

Color textColors[256];

int FormattedText_Layout(const char *text, Color color, float fontSize, float wrapWidth,
                         TextGlyph *glyphs, int capacity, int *lines, float *width) {
    Font font = GetFontDefault();
    float scale = fontSize / font.baseSize;
    float x = 0, widest = 0;
    int count = 0, line = 0, codepoint;
    while ((codepoint = TextColor_Next(&text, textColors, &color)) && count < capacity) {
        if (codepoint == '\r') continue;
        if (codepoint == '\n') { x = 0; line++; continue; }
        int index = GetGlyphIndex(font, codepoint);
        float advance = (font.glyphs[index].advanceX ? font.glyphs[index].advanceX : font.recs[index].width) * scale;
        if (wrapWidth > 0 && x > 0 && x + advance > wrapWidth) { x = 0; line++; }
        glyphs[count++] = (TextGlyph){codepoint, line, x, color};
        if (x + advance > widest) widest = x + advance;
        x += advance + scale;
    }
    *lines = line + 1;
    *width = widest;
    return count;
}

void FormattedText_DrawGlyph(TextGlyph glyph, Vector2 position, float fontSize, float opacity) {
    Color color = glyph.color;
    color.a = (unsigned char)(color.a * opacity);
    float shadow = fontSize / GetFontDefault().baseSize;
    DrawTextCodepoint(GetFontDefault(), glyph.codepoint,
        (Vector2){position.x + shadow, position.y + shadow}, fontSize, (Color){0, 0, 0, color.a});
    DrawTextCodepoint(GetFontDefault(), glyph.codepoint, position, fontSize, color);
}
