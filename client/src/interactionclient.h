/**
 * Copyright (c) 2026 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */
#ifndef MIDLESS_CLIENT_INTERACTION_H
#define MIDLESS_CLIENT_INTERACTION_H
#include "raylib.h"
#include <stdint.h>
void ClientInteraction_Reset(void);
void ClientInteraction_Update(void);
void ClientInteraction_Acknowledge(uint32_t sequence);
void ClientInteraction_Use(Vector3 hit, Vector3 normal, int block);
void ClientInteraction_Dig(bool held, bool clicked, Vector3 hit, Vector3 normal, int block);
void ClientInteraction_SetDigProgress(uint32_t sequence, int milliseconds);
float ClientInteraction_DigProgress(Vector3 *position);
#endif
