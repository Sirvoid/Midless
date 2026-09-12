#ifndef MIDLESS_SERVER_TEXT_COLORS_H
#define MIDLESS_SERVER_TEXT_COLORS_H
#include "player.h"
#include "entity.h"

void ServerTextColors_Define(unsigned char code, Color color);
void ServerTextColors_Remove(unsigned char code);
void ServerTextColors_Send(Player *player);
void ServerTextColors_Reset(void);
void ServerNametag_Set(Entity *entity, Nametag tag);
unsigned char *ServerNametag_CreatePacket(const Entity *entity);
#endif
