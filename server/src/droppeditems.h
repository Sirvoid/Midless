/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SERVER_DROPPED_ITEMS_H
#define MIDLESS_SERVER_DROPPED_ITEMS_H

#include "player.h"
#include "entity.h"

int ServerDrops_Spawn(ItemStack stack, Vector3 position, Vector3 velocity, float pickupDelay);
bool ServerDrops_Throw(Player *player, bool oneItem);
void ServerDrops_Update(float dt);
void ServerDrops_Replicate(Entity *entity);
void ServerDrops_Remove(Entity *entity);
void ServerDrops_ForgetPlayer(int playerId);
void ServerDrops_Reset(void);

#endif
