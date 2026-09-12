/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_ENTITY_TEXTURE_H
#define MIDLESS_ENTITY_TEXTURE_H
#include "entity.h"
void ServerEntityTexture_Set(Entity *entity, const char *name);
int ServerEntityTexture_Id(const Entity *entity);
unsigned char *ServerEntityTexture_Packet(const Entity *entity, int recipientEntityId);
#endif
