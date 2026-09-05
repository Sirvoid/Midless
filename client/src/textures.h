#ifndef ISLEFORGE_CLIENT_TEXTURES_H
#define ISLEFORGE_CLIENT_TEXTURES_H
#include "raylib.h"
void ClientTextures_Init(Texture2D terrain);
Texture2D ClientTextures_Get(int id);
void ClientTextures_Reset(void);
void ClientTextures_HandleBegin(void);
void ClientTextures_HandleData(void);
void ClientTextures_HandleTerrain(void);
void ClientTextures_UpdateLiquidTints(void);
#endif
