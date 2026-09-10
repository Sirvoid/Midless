#ifndef MIDLESS_SERVER_HUD_BARS_H
#define MIDLESS_SERVER_HUD_BARS_H
#include "player.h"
int ServerHudBars_Define(void);
int ServerHudBars_Remove(void);
int ServerHudBars_Set(Player *player);
void ServerHudBars_Send(Player *player);
void ServerHudBars_Reset(void);
bool ServerHudBars_UsesTexture(int texture);
#endif
