/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SERVER_LIGHTING_H
#define MIDLESS_SERVER_LIGHTING_H
#include "world/chunk/chunk.h"
void ServerLighting_Changed(Chunk *chunk);
void ServerLighting_Removed(Vector3 position);
void ServerLighting_Invalidate(void);
void ServerLighting_Update(void);
void ServerLighting_Forget(Chunk *chunk);
bool ServerLighting_IsSettled(void);
bool ServerLighting_IsReady(Chunk *chunk);
void ServerLighting_Prioritize(Chunk *chunk);
void ServerLighting_Send(Chunk *chunk, Player *player);
bool ServerLighting_Get(Vector3 position, int *block, int *sky, int *level);
#endif
