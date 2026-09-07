#ifndef ISLEFORGE_INVENTORY_PROTOCOL_H
#define ISLEFORGE_INVENTORY_PROTOCOL_H

#include "inventory.h"

#define PACKET_INVENTORY_ACTION 6
#define PACKET_INVENTORY_STATE 21
#define INVENTORY_ACTION_PACKET_SIZE 25
#define INVENTORY_STATE_PACKET_SIZE 123

void InventoryProtocol_WriteAction(uint8_t *data, const InventoryAction *action);
bool InventoryProtocol_ReadAction(const uint8_t *data, int length, InventoryAction *action);
void InventoryProtocol_WriteState(uint8_t *data, const Inventory *inventory, uint32_t revision, uint32_t acknowledged);
bool InventoryProtocol_ReadState(const uint8_t *data, int length, Inventory *inventory, uint32_t *revision, uint32_t *acknowledged);

#endif
