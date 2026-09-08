#include "inventoryview.h"
#include <math.h>
#include <string.h>

bool InventoryView_Validate(const InventoryView *v) {
    if (!v->session || !v->slotCount || !v->count || v->count > INVENTORY_VIEW_ELEMENTS ||
        !isfinite(v->width) || !isfinite(v->height) || v->width < 1 || v->width > 32 ||
        v->height < 1 || v->height > 32) return false;
    int grids[2] = {0};
    for (int i = 0; i < v->count; i++) {
        const InventoryElement *e = &v->elements[i];
        if (!isfinite(e->x) || !isfinite(e->y) || e->x < 0 || e->y < 0 ||
            e->x >= v->width || e->y >= v->height) return false;
        if (!e->grid) continue;
        if (++grids[e->container] > 1 || !e->columns || !e->rows ||
            e->columns * e->rows != (e->container ? v->slotCount : INVENTORY_SLOT_COUNT) ||
            e->x + e->columns > v->width || e->y + e->rows > v->height) return false;
        for (int j = 0; j < i; j++) {
            const InventoryElement *other = &v->elements[j];
            if (other->grid && e->x < other->x + other->columns && other->x < e->x + e->columns &&
                e->y < other->y + other->rows && other->y < e->y + e->rows) return false;
        }
    }
    return grids[0] == 1 && grids[1] == 1;
}

static void WriteText(BinaryWriter *out, const char *s) {
    size_t length = strlen(s);
    Binary_U8(out, length);
    Binary_Write(out, s, length);
}
static void ReadText(BinaryReader *in, char *s) {
    int length = Binary_ReadU8(in);
    if (length > 64) { in->failed = true; return; }
    const uint8_t *data = Binary_Read(in, length);
    if (data) memcpy(s, data, length);
    s[length] = 0;
}
void InventoryView_Write(BinaryWriter *out, const InventoryView *v) {
    Binary_U32(out, v->session);
    WriteText(out, v->title);
    Binary_Float(out, v->width); Binary_Float(out, v->height);
    Binary_U8(out, v->count); Binary_U8(out, v->slotCount);
    for (int i = 0; i < v->count; i++) {
        const InventoryElement *e = &v->elements[i];
        Binary_U8(out, e->grid); Binary_U8(out, e->container);
        Binary_Float(out, e->x); Binary_Float(out, e->y);
        Binary_U8(out, e->columns); Binary_U8(out, e->rows);
        WriteText(out, e->text);
    }
    for (int i = 0; i < v->slotCount; i++) {
        Binary_U16(out, v->slots[i].itemId); Binary_U8(out, v->slots[i].count);
    }
}
bool InventoryView_Read(BinaryReader *in, InventoryView *v) {
    *v = (InventoryView){0};
    v->session = Binary_ReadU32(in);
    ReadText(in, v->title);
    v->width = Binary_ReadFloat(in); v->height = Binary_ReadFloat(in);
    v->count = Binary_ReadU8(in); v->slotCount = Binary_ReadU8(in);
    if (v->count > INVENTORY_VIEW_ELEMENTS) return false;
    for (int i = 0; i < v->count; i++) {
        InventoryElement *e = &v->elements[i];
        int grid = Binary_ReadU8(in), container = Binary_ReadU8(in);
        if (grid > 1 || container > 1) return false;
        e->grid = grid; e->container = container;
        e->x = Binary_ReadFloat(in); e->y = Binary_ReadFloat(in);
        e->columns = Binary_ReadU8(in); e->rows = Binary_ReadU8(in);
        ReadText(in, e->text);
    }
    for (int i = 0; i < v->slotCount; i++) {
        ItemStack *s = &v->slots[i];
        s->itemId = Binary_ReadU16(in); s->count = Binary_ReadU8(in);
        if ((!s->itemId != !s->count) || s->count > Item_GetMaxStack(s->itemId)) return false;
    }
    return !in->failed && InventoryView_Validate(v);
}

void Inventory_ClickStack(ItemStack *slot, ItemStack *cursor, bool right) {
    // Reuse the same rules as the ordinary player inventory.
    Inventory temporary = {.open = true, .cursor = *cursor};
    temporary.slots[0] = *slot;
    Inventory_ApplyAction(&temporary, &(InventoryAction){.type = right ? INVENTORY_RIGHT_CLICK : INVENTORY_LEFT_CLICK});
    *slot = temporary.slots[0];
    *cursor = temporary.cursor;
}
void Inventory_Transfer(ItemStack *source, ItemStack *slots, int count) {
    for (int pass = 0; pass < 2 && source->count; pass++) for (int i = 0; i < count && source->count; i++) {
        ItemStack *slot = &slots[i];
        if (pass == 0 ? (!slot->count || slot->itemId != source->itemId) : slot->count != 0) continue;
        int amount = Item_GetMaxStack(source->itemId) - slot->count;
        if (amount > source->count) amount = source->count;
        slot->itemId = source->itemId; slot->count += amount;
        source->count -= amount;
        if (!source->count) source->itemId = 0;
    }
}
