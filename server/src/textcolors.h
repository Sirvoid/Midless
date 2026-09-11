#ifndef MIDLESS_SERVER_TEXT_COLORS_H
#define MIDLESS_SERVER_TEXT_COLORS_H
#include "player.h"
#include "entity.h"
#include "minilua.h"

int ServerTextColors_Define(void);
int ServerTextColors_Remove(void);
int ServerTextColors_Escape(void);
void ServerTextColors_Send(Player *player);
void ServerTextColors_Reset(void);
int ServerNametag_Set(lua_State *state, Entity *entity);
unsigned char *ServerNametag_CreatePacket(const Entity *entity);
#endif
