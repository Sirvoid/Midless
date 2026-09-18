/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */
#ifndef MIDLESS_SERVER_ATTACHMENTS_H
#define MIDLESS_SERVER_ATTACHMENTS_H
#include "entity.h"
#include "player.h"
Entity *ServerAttachment_Parent(Entity *entity);
bool ServerAttachment_Set(Entity *entity, Entity *parent, Attachment attachment);
void ServerAttachment_Detach(Entity *entity, const Vector3 *position, bool inheritVelocity);
void ServerAttachments_Update(void);
void ServerAttachments_Cleanup(Entity *entity);
void ServerAttachments_Send(Player *player);
void ServerAttachments_Replicate(void);
bool ServerAttachments_ChunkOccupied(Vector3 chunk);
bool ServerAttachments_HasLinks(Entity *entity);
bool ServerControl_Set(Entity *entity, Player *player);
Player *ServerControl_Player(Entity *entity);
void ServerControl_Step(Entity *entity, float dt);
void ServerControl_Input(Player *player, uint32_t session, int forward, int sideways,
                         unsigned flags, Vector3 look);
void ScriptHooks_Control(Entity *entity, Player *player, float dt, int event);
#endif
