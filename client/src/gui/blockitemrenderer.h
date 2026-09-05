/**
 * Copyright (c) 2021-2022 Sirvoid
 *
 * This software is released under the MIT License.
 */

#ifndef MIDLESS_CLIENT_BLOCK_ITEM_RENDERER_H
#define MIDLESS_CLIENT_BLOCK_ITEM_RENDERER_H

#include "raylib.h"

void BlockItemRenderer_Init(Texture2D terrain);
void BlockItemRenderer_Shutdown(void);
void BlockItemRenderer_Refresh(int blockId);
void BlockItemRenderer_Draw(int blockId, Rectangle bounds);

#endif
