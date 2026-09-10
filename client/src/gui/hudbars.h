#ifndef MIDLESS_CLIENT_HUD_BARS_H
#define MIDLESS_CLIENT_HUD_BARS_H
#include "raylib.h"
#include "hudbarprotocol.h"
void ClientHudBars_Reset(void);
void ClientHudBars_Define(int id, HudBarDefinition bar);
void ClientHudBars_Set(int id, HudBarState state);
void ClientHudBars_Remove(int id);
void ClientHudBars_Draw(Rectangle hotbar);
#endif
