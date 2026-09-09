#ifndef MIDLESS_INVENTORY_H
#define MIDLESS_INVENTORY_H

#include <stdbool.h>
#include <stdint.h>

#define INVENTORY_STORAGE_SLOTS 27
#define INVENTORY_HOTBAR_SLOTS 9
#define INVENTORY_SLOT_COUNT 36
#define INVENTORY_NO_SLOT 255

typedef struct ItemStack {
    uint16_t itemId;
    uint8_t count;
} ItemStack;

typedef struct Inventory {
    ItemStack slots[INVENTORY_SLOT_COUNT];
    ItemStack cursor;
    uint8_t cursorOrigin;
    uint8_t selectedHotbar;
    bool open;
} Inventory;

typedef enum InventoryActionType {
    INVENTORY_OPEN,
    INVENTORY_CLOSE,
    INVENTORY_LEFT_CLICK,
    INVENTORY_RIGHT_CLICK,
    INVENTORY_SELECT,
    INVENTORY_BREAK,
    INVENTORY_PLACE,
    INVENTORY_THROW_STACK,
    INVENTORY_THROW_ONE,
    INVENTORY_VIEW_LEFT,
    INVENTORY_VIEW_RIGHT,
    INVENTORY_VIEW_SHIFT,
    INVENTORY_CRAFT,
    INVENTORY_CRAFT_ALL
} InventoryActionType;

typedef struct InventoryAction {
    uint32_t sequence;
    uint8_t type, slot;
    int32_t x, y, z;
    uint8_t face; // -X, +X, -Y, +Y, -Z, +Z
    uint16_t targetBlock;
    uint8_t hit[3]; // Point within the target cell, in units of 1/255 block.
} InventoryAction;

int Item_GetMaxStack(uint16_t itemId);
void Inventory_Init(Inventory *inventory);
ItemStack *Inventory_GetSelected(Inventory *inventory);
bool Inventory_Add(Inventory *inventory, uint16_t itemId, int count);
int Inventory_AddPartial(Inventory *inventory, uint16_t itemId, int count);
int Inventory_AddToSlots(ItemStack *slots, int slotCount, int firstSlot, uint16_t itemId, int count);
bool Inventory_Close(Inventory *inventory);
void Inventory_ApplyAction(Inventory *inventory, const InventoryAction *action);

#endif
