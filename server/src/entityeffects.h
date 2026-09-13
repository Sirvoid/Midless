/**
 * Copyright (c) 2026 Sirvoid
 * Released under the MIT License. https://opensource.org/licenses/MIT
 */
#ifndef MIDLESS_SERVER_ENTITYEFFECTS_H
#define MIDLESS_SERVER_ENTITYEFFECTS_H
#include "entity.h"

void ServerEntityEffects_Flash(Entity *entity, Color color, float duration);
void ServerEntityEffects_DamageFlash(Entity *entity);
#endif
