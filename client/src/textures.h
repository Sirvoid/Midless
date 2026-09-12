/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_TEXTURES_H
#define MIDLESS_CLIENT_TEXTURES_H
#include "raylib.h"
#include <stdint.h>
void ClientTextures_Init(Texture2D terrain);
Texture2D ClientTextures_Get(int id);
void ClientTextures_Reset(void);
void ClientTextures_Begin(int id, uint32_t revision, uint32_t size, int width, int height);
void ClientTextures_Data(int id, uint32_t revision, uint32_t offset, unsigned count, const unsigned char *data);
void ClientTextures_SetTerrain(int id);
void ClientTextures_UpdateLiquidTints(void);
#endif
