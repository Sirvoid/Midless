#ifndef MIDLESS_SERVER_TEXTURES_H
#define MIDLESS_SERVER_TEXTURES_H
#include "../player.h"
int ServerTextures_Find(const char *name);
bool ServerTextures_ItemSize(int id);
bool ServerTextures_HudSize(int id);
bool ServerTextures_Define(const char *name, const char *path);
bool ServerTextures_SetTerrain(int id);
void ServerTextures_SendTerrain(Player *player);
void ServerTextures_Update(void);
void ServerTextures_Shutdown(void);
void ServerTextures_Acknowledge(Player *player, int id, uint32_t revision, uint32_t offset);
bool ServerTextures_SetBreaking(int id);
void ServerTextures_SendBreaking(Player *player);
#endif
