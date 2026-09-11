#ifndef MIDLESS_SERVER_LIGHTING_H
#define MIDLESS_SERVER_LIGHTING_H
#include "world/chunk/chunk.h"
void ServerLighting_Changed(Chunk *chunk);
void ServerLighting_Removed(Vector3 position);
void ServerLighting_Invalidate(void);
void ServerLighting_Update(void);
void ServerLighting_Send(Chunk *chunk, Player *player);
bool ServerLighting_Get(Vector3 position, int *block, int *sky, int *level);
#endif
