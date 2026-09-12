/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_INVENTORY_PROTOCOL_H
#define MIDLESS_INVENTORY_PROTOCOL_H

#include "packetopcodes.h"

#include "packetsizes.h"

#include "inventory.h"


void InventoryProtocol_WriteAction(uint8_t *data, const InventoryAction *action);
bool InventoryProtocol_ReadAction(const uint8_t *data, int length, InventoryAction *action);
void InventoryProtocol_WriteState(uint8_t *data, const Inventory *inventory, uint32_t revision, uint32_t acknowledged);
bool InventoryProtocol_ReadState(const uint8_t *data, int length, Inventory *inventory, uint32_t *revision, uint32_t *acknowledged);

#endif
