/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "textureprotocol.h"
#include <string.h>
bool Texture_ValidatePNG(const unsigned char *data, int size, int *width, int *height) {
    static const unsigned char signature[8] = {137,80,78,71,13,10,26,10};
    if (!data || size < 33 || size > TEXTURE_MAX_BYTES || memcmp(data,signature,8) ||
        Texture_Read32(data+8)!=13 || memcmp(data+12,"IHDR",4)) return false;
    uint32_t w=Texture_Read32(data+16), h=Texture_Read32(data+20);
    if (!w || !h || w>TEXTURE_MAX_DIMENSION || h>TEXTURE_MAX_DIMENSION) return false;
    *width=w; *height=h; return true;
}
