#ifndef MIDLESS_INVENTORY_VIEW_H
#define MIDLESS_INVENTORY_VIEW_H
#include "inventory.h"
#include "binarydata.h"

#define INVENTORY_VIEW_ELEMENTS 16
#define INVENTORY_VIEW_SLOTS 255
#define INVENTORY_VIEW_BINDINGS 16
#define PACKET_INVENTORY_VIEW 23

typedef struct InventoryElement {
    bool grid, crafting;
    uint8_t binding;
    ItemStack preview;
    float x, y;
    uint8_t columns, rows;
    char text[65];
} InventoryElement;

typedef struct InventoryView {
    uint32_t session;
    char title[65];
    float width, height;
    uint8_t count, slotCount;
    uint8_t bindingCount, bindingSlots[INVENTORY_VIEW_BINDINGS];
    InventoryElement elements[INVENTORY_VIEW_ELEMENTS];
    ItemStack slots[INVENTORY_VIEW_SLOTS];
} InventoryView;

bool InventoryView_Validate(const InventoryView *view);
int InventoryView_Offset(const InventoryView *view, int binding);
void InventoryView_Write(BinaryWriter *out, const InventoryView *view);
bool InventoryView_Read(BinaryReader *in, InventoryView *view);
void Inventory_ClickStack(ItemStack *slot, ItemStack *cursor, bool right);
void Inventory_Transfer(ItemStack *source, ItemStack *slots, int count);
#endif
