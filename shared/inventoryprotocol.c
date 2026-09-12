/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "inventoryprotocol.h"
#include <string.h>

static void WriteU32(uint8_t *data, uint32_t value) {
    for (int i = 0; i < 4; i++) data[i] = (uint8_t)(value >> (24 - i * 8));
}

static uint32_t ReadU32(const uint8_t *data) {
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) | data[3];
}

void InventoryProtocol_WriteAction(uint8_t *data, const InventoryAction *action) {
    data[0] = PACKET_INVENTORY_ACTION;
    WriteU32(data + 1, action->sequence);
    data[5] = action->type;
    data[6] = action->slot;
    WriteU32(data + 7, (uint32_t)action->x);
    WriteU32(data + 11, (uint32_t)action->y);
    WriteU32(data + 15, (uint32_t)action->z);
    data[19] = action->face;
    data[20] = (uint8_t)(action->targetBlock >> 8);
    data[21] = (uint8_t)action->targetBlock;
    for (int i = 0; i < 3; i++) data[22 + i] = action->hit[i];
}

bool InventoryProtocol_ReadAction(const uint8_t *data, int length, InventoryAction *action) {
    if (length != INVENTORY_ACTION_PACKET_SIZE || data[0] != PACKET_INVENTORY_ACTION) return false;
    *action = (InventoryAction){
        .sequence = ReadU32(data + 1), .type = data[5], .slot = data[6],
        .x = (int32_t)ReadU32(data + 7), .y = (int32_t)ReadU32(data + 11),
        .z = (int32_t)ReadU32(data + 15), .face = data[19],
        .targetBlock = (uint16_t)((data[20] << 8) | data[21])
    };
    for (int i = 0; i < 3; i++) action->hit[i] = data[22 + i];
    return true;
}

void InventoryProtocol_WriteState(uint8_t *data, const Inventory *inventory, uint32_t revision, uint32_t acknowledged) {
    data[0] = PACKET_INVENTORY_STATE;
    WriteU32(data + 1, revision);
    WriteU32(data + 5, acknowledged);
    data[9] = inventory->selectedHotbar;
    data[10] = inventory->open;
    data[11] = inventory->cursorOrigin;
    for (int i = 0; i <= INVENTORY_SLOT_COUNT; i++) {
        const ItemStack *stack = i == INVENTORY_SLOT_COUNT ? &inventory->cursor : &inventory->slots[i];
        int offset = 12 + i * ITEM_STACK_PACKET_SIZE;
        data[offset] = (uint8_t)(stack->itemId >> 8);
        data[offset + 1] = (uint8_t)stack->itemId;
        data[offset + 2] = stack->count;
        data[offset + 3] = stack->metadataSize;
        data[offset + 4] = stack->metadataVersion >> 8;
        data[offset + 5] = stack->metadataVersion;
        memcpy(data + offset + 6, stack->metadata, ITEM_METADATA_BYTES);
    }
}

bool InventoryProtocol_ReadState(const uint8_t *data, int length, Inventory *inventory, uint32_t *revision, uint32_t *acknowledged) {
    if (length != INVENTORY_STATE_PACKET_SIZE || data[0] != PACKET_INVENTORY_STATE ||
        data[9] >= INVENTORY_HOTBAR_SLOTS || data[10] > 1 ||
        (data[11] >= INVENTORY_SLOT_COUNT && data[11] != INVENTORY_NO_SLOT)) return false;
    Inventory result = {0};
    result.selectedHotbar = data[9];
    result.open = data[10] != 0;
    result.cursorOrigin = data[11];
    for (int i = 0; i <= INVENTORY_SLOT_COUNT; i++) {
        ItemStack *stack = i == INVENTORY_SLOT_COUNT ? &result.cursor : &result.slots[i];
        int offset = 12 + i * ITEM_STACK_PACKET_SIZE;
        stack->itemId = (uint16_t)((data[offset] << 8) | data[offset + 1]);
        stack->count = data[offset + 2];
        stack->metadataSize = data[offset + 3];
        stack->metadataVersion = (data[offset + 4] << 8) | data[offset + 5];
        if (stack->metadataSize > ITEM_METADATA_BYTES || (stack->metadataSize && (!stack->count || !stack->metadataVersion))) return false;
        memcpy(stack->metadata, data + offset + 6, ITEM_METADATA_BYTES);
        if ((stack->count == 0) != (stack->itemId == 0) || stack->count > Item_GetMaxStack(stack->itemId)) return false;
    }
    if (!result.open && result.cursor.count) return false;
    *inventory = result;
    *revision = ReadU32(data + 1);
    *acknowledged = ReadU32(data + 5);
    return true;
}
