#ifndef MIDLESS_ENTITY_TEXTURE_H
#define MIDLESS_ENTITY_TEXTURE_H
#include "entity.h"
#include "minilua.h"
int ServerEntityTexture_Set(lua_State *state, Entity *entity);
int ServerEntityTexture_Get(lua_State *state, Entity *entity);
int ServerEntityTexture_Id(const Entity *entity);
unsigned char *ServerEntityTexture_Packet(const Entity *entity, int recipientEntityId);
#endif
