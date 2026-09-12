/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_TEXTURE_PROTOCOL_H
#define MIDLESS_TEXTURE_PROTOCOL_H

#include "packetopcodes.h"

#include "packetsizes.h"
#include <stdint.h>
#include <stdbool.h>
#define TEXTURE_LIMIT 66
#define TEXTURE_MAX_BYTES (2 * 1024 * 1024)
#define TEXTURE_MAX_DIMENSION 1024
#define TEXTURE_TOTAL_BYTES (32 * 1024 * 1024)
#define TEXTURE_TOTAL_PIXELS (16 * 1024 * 1024)
#define TEXTURE_CHUNK_BYTES 4096
static inline uint32_t Texture_Read32(const unsigned char *p) {
    return ((uint32_t)p[0]<<24) | ((uint32_t)p[1]<<16) | ((uint32_t)p[2]<<8) | p[3];
}
static inline unsigned Texture_Read16(const unsigned char *p) { return (p[0]<<8)|p[1]; }
static inline void Texture_Write32(unsigned char *p, uint32_t n) { p[0]=n>>24; p[1]=n>>16; p[2]=n>>8; p[3]=n; }
static inline void Texture_Write16(unsigned char *p, unsigned n) { p[0]=n>>8; p[1]=n; }
bool Texture_ValidatePNG(const unsigned char *data, int size, int *width, int *height);
#endif
