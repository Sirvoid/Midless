#include "inventory.h"

int Item_GetMaxStack(uint16_t itemId) {
    // Current items are blocks. Future item definitions can supply smaller limits here.
    return itemId != 0 ? 64 : 0;
}

void Inventory_Init(Inventory *inventory) {
    *inventory = (Inventory){0};
    inventory->cursorOrigin = INVENTORY_NO_SLOT;
}

ItemStack *Inventory_GetSelected(Inventory *inventory) {
    return &inventory->slots[INVENTORY_STORAGE_SLOTS + inventory->selectedHotbar];
}

static void MoveItems(ItemStack *source, ItemStack *destination, int requested) {
    if (!source->count || (destination->count && destination->itemId != source->itemId)) return;
    int available = Item_GetMaxStack(source->itemId) - destination->count;
    int moved = requested < source->count ? requested : source->count;
    if (moved > available) moved = available;
    if (moved <= 0) return;
    destination->itemId = source->itemId;
    destination->count += moved;
    source->count -= moved;
    if (!source->count) source->itemId = 0;
}

int Inventory_AddToSlots(ItemStack *slots, int slotCount, int firstSlot, uint16_t itemId, int count) {
    if (!itemId || count <= 0 || slotCount <= 0) return 0;
    int remaining = count;
    // Fill matching stacks before empty slots, preferring the hotbar for new stacks.
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < slotCount && remaining > 0; i++) {
            int index = (i + firstSlot) % slotCount;
            ItemStack *slot = &slots[index];
            if (pass == 0 ? (!slot->count || slot->itemId != itemId) : slot->count != 0) continue;
            int moved = Item_GetMaxStack(itemId) - slot->count;
            if (moved > remaining) moved = remaining;
            slot->itemId = itemId;
            slot->count += moved;
            remaining -= moved;
        }
    }
    return count - remaining;
}
int Inventory_AddPartial(Inventory *inventory, uint16_t itemId, int count) {
    return Inventory_AddToSlots(inventory->slots, INVENTORY_SLOT_COUNT, INVENTORY_STORAGE_SLOTS, itemId, count);
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
    } else if (!slot->count || slot->itemId == cursor->itemId) {
        MoveItems(cursor, slot, rightClick ? 1 : cursor->count);
    } else if (!rightClick) {
        ItemStack previous = *slot;
        *slot = *cursor;
        *cursor = previous;
        inventory->cursorOrigin = action->slot;
    }
    if (!cursor->count) inventory->cursorOrigin = INVENTORY_NO_SLOT;
}
