#ifndef MIDLESS_SERVER_HUD_BARS_H
#define MIDLESS_SERVER_HUD_BARS_H
#include "player.h"
int ServerHudBars_Find(const char *name);
const char *ServerHudBars_Define(const char *name, const HudBarDefinition *definition);
void ServerHudBars_Remove(const char *name);
bool ServerHudBars_Set(Player *player, int id, HudBarState state);
void ServerHudBars_Send(Player *player);
void ServerHudBars_Reset(void);
bool ServerHudBars_UsesTexture(int texture);
#endif
