/**
 * Copyright (c) 2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */
#ifndef MIDLESS_SERVER_INTERACTION_H
#define MIDLESS_SERVER_INTERACTION_H
#include "player.h"
#include "worldaction.h"
void ServerInteraction_Apply(Player *player, WorldAction action);
void ServerInteraction_UpdateDigging(void);
void ServerInteraction_InvalidateDig(Vector3 position);
void ServerInteraction_CancelDig(Player *player);
#endif
