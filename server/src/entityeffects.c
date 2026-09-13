/**
 * Copyright (c) 2026 Sirvoid
 * Released under the MIT License. https://opensource.org/licenses/MIT
 */
#include "entityeffects.h"
#include "packet.h"
#include "networkhandler.h"
#include "world/world.h"

void ServerEntityEffects_Flash(Entity *entity, Color color, float duration) {
    unsigned char *packet = ServerPacket_CreateEntityFlash(entity->id, color, duration);
    if (packet) ServerWorld_BroadcastExcluding(packet, entity->ownerPlayerId);
    if (entity->ownerPlayerId >= 0) {
        Player *owner = serverWorld.players[entity->ownerPlayerId];
        if (owner && !owner->disconnected && owner->movementReady) {
            packet = ServerPacket_CreateEntityFlash(65535, color, duration);
            if (packet) ServerNetwork_Send(owner, packet);
        }
    }
}

void ServerEntityEffects_DamageFlash(Entity *entity) {
    ServerEntityEffects_Flash(entity, (Color){200, 60, 75, 255}, 0.2f);
}
