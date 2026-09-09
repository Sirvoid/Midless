#include "inventory.h"
#include <string.h>

bool ItemStack_Matches(ItemStack a, ItemStack b) {
    return a.itemId == b.itemId && a.metadataSize == b.metadataSize &&
        (!a.metadataSize || (a.metadataVersion == b.metadataVersion && !memcmp(a.metadata, b.metadata, a.metadataSize)));
}
void ItemStack_Write(BinaryWriter *out, ItemStack stack) {
    if (stack.metadataSize > ITEM_METADATA_BYTES) { out->failed = true; return; }
    Binary_U16(out, stack.itemId);
    Binary_U8(out, stack.count | (stack.metadataSize ? 128 : 0));
    if (stack.metadataSize) {
        Binary_U8(out, stack.metadataSize); Binary_U16(out, stack.metadataVersion);
        Binary_Write(out, stack.metadata, stack.metadataSize);
    }
}
ItemStack ItemStack_Read(BinaryReader *in) {
    ItemStack stack = {0};
    stack.itemId = Binary_ReadU16(in);
    int count = Binary_ReadU8(in); stack.count = count & 127;
    if (count & 128) {
        stack.metadataSize = Binary_ReadU8(in); stack.metadataVersion = Binary_ReadU16(in);
        const uint8_t *data = Binary_Read(in, stack.metadataSize);
        if (!data || !stack.metadataSize || stack.metadataSize > ITEM_METADATA_BYTES || !stack.metadataVersion) in->failed = true;
        else memcpy(stack.metadata, data, stack.metadataSize);
    }
    if ((!stack.itemId != !stack.count) || stack.count > 64 || (!stack.count && stack.metadataSize)) in->failed = true;
    return stack;
}

void Inventory_Init(Inventory *inventory) {
    *inventory = (Inventory){0};
    inventory->cursorOrigin = INVENTORY_NO_SLOT;
}

ItemStack *Inventory_GetSelected(Inventory *inventory) {
    return &inventory->slots[INVENTORY_STORAGE_SLOTS + inventory->selectedHotbar];
}

static void MoveItems(ItemStack *source, ItemStack *destination, int requested) {
    if (!source->count || (destination->count && !ItemStack_Matches(*destination, *source))) return;
    int available = Item_GetMaxStack(source->itemId) - destination->count;
    int moved = requested < source->count ? requested : source->count;
    if (moved > available) moved = available;
    if (moved <= 0) return;
    if (!destination->count) { *destination = *source; destination->count = 0; }
    destination->count += moved;
    source->count -= moved;
    if (!source->count) *source = (ItemStack){0};
}

int Inventory_AddToSlots(ItemStack *slots, int slotCount, int firstSlot, uint16_t itemId, int count) {
    return Inventory_AddStackToSlots(slots, slotCount, firstSlot, (ItemStack){.itemId=itemId}, count);
}
int Inventory_AddStackToSlots(ItemStack *slots, int slotCount, int firstSlot, ItemStack stack, int count) {
    if (!stack.itemId || count <= 0 || slotCount <= 0) return 0;
    int remaining = count;
    // Fill matching stacks before empty slots, preferring the hotbar for new stacks.
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < slotCount && remaining > 0; i++) {
            int index = (i + firstSlot) % slotCount;
            ItemStack *slot = &slots[index];
            if (pass == 0 ? (!slot->count || !ItemStack_Matches(*slot, stack)) : slot->count != 0) continue;
            int moved = Item_GetMaxStack(stack.itemId) - slot->count;
            if (moved <= 0) continue;
            if (moved > remaining) moved = remaining;
            if (!slot->count) { *slot = stack; slot->count = 0; }
            slot->count += moved;
            remaining -= moved;
        }
    }
    return count - remaining;
}
int Inventory_AddPartial(Inventory *inventory, uint16_t itemId, int count) {
    return Inventory_AddToSlots(inventory->slots, INVENTORY_SLOT_COUNT, INVENTORY_STORAGE_SLOTS, itemId, count);
}
int Inventory_AddStackPartial(Inventory *inventory, ItemStack stack) {
    return Inventory_AddStackToSlots(inventory->slots, INVENTORY_SLOT_COUNT, INVENTORY_STORAGE_SLOTS, stack, stack.count);
}
bool Inventory_AddStack(Inventory *inventory, ItemStack stack) {
    Inventory next = *inventory;
    if (!stack.count || Inventory_AddStackPartial(&next, stack) != stack.count) return false;
    *inventory = next; return true;
}

bool Inventory_Add(Inventory *inventory, uint16_t itemId, int count) {
    if (!itemId || count <= 0) return false;
    Inventory result = *inventory;
    if (Inventory_AddPartial(&result, itemId, count) != count) return false;
    *inventory = result;
    return true;
}

bool Inventory_Close(Inventory *inventory) {
    if (inventory->cursor.count) return false;
    inventory->cursorOrigin = INVENTORY_NO_SLOT;
    inventory->open = false;
    return true;
}

void Inventory_ApplyAction(Inventory *inventory, const InventoryAction *action) {
    if (action->type == INVENTORY_OPEN) {
        inventory->open = true;
        return;
    }
    if (action->type == INVENTORY_CLOSE) {
        Inventory_Close(inventory);
        return;
    }
    if (action->type == INVENTORY_SELECT) {
        if (action->slot < INVENTORY_HOTBAR_SLOTS) inventory->selectedHotbar = action->slot;
        return;
    }
    if (!inventory->open || action->slot >= INVENTORY_SLOT_COUNT ||
        (action->type != INVENTORY_LEFT_CLICK && action->type != INVENTORY_RIGHT_CLICK)) return;

    ItemStack *slot = &inventory->slots[action->slot];
    ItemStack *cursor = &inventory->cursor;
    bool rightClick = action->type == INVENTORY_RIGHT_CLICK;
    if (!cursor->count) {
        if (!slot->count) return;
        inventory->cursorOrigin = action->slot;
        MoveItems(slot, cursor, rightClick ? (slot->count + 1) / 2 : slot->count);
    } else if (!slot->count || ItemStack_Matches(*slot, *cursor)) {
        MoveItems(cursor, slot, rightClick ? 1 : cursor->count);
    } else if (!rightClick) {
        ItemStack previous = *slot;
        *slot = *cursor;
        *cursor = previous;
        inventory->cursorOrigin = action->slot;
    }
    if (!cursor->count) inventory->cursorOrigin = INVENTORY_NO_SLOT;
}
