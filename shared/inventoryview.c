#include "inventoryview.h"
#include <math.h>
#include <string.h>

int InventoryView_Offset(const InventoryView *view, int binding) {
    int offset = 0;
    for (int i = 1; i < binding; i++) offset += view->bindingSlots[i];
    return offset;
}
bool InventoryView_Validate(const InventoryView *v) {
    if (!v->session || !v->count || v->count > INVENTORY_VIEW_ELEMENTS ||
        !v->bindingCount || v->bindingCount > INVENTORY_VIEW_BINDINGS || v->bindingSlots[0] != INVENTORY_SLOT_COUNT ||
        !isfinite(v->width) || !isfinite(v->height) || v->width < 1 || v->width > 32 ||
        v->height < 1 || v->height > 32) return false;
    int total = 0, grids[INVENTORY_VIEW_BINDINGS] = {0};
    for (int i = 1; i < v->bindingCount; i++) {
        if (!v->bindingSlots[i]) return false;
        total += v->bindingSlots[i];
    }
    if (total != v->slotCount) return false;
    for (int i = 0; i < v->count; i++) {
        const InventoryElement *e = &v->elements[i];
        if (!isfinite(e->x) || !isfinite(e->y) || e->x < 0 || e->y < 0 ||
            e->x >= v->width || e->y >= v->height) return false;
        if (e->grid && e->crafting) return false;
        if (!e->grid && !e->crafting) continue;
        if (e->binding >= v->bindingCount || (!e->crafting && ++grids[e->binding] > 1) || !e->columns || !e->rows ||
            e->columns * e->rows != v->bindingSlots[e->binding] ||
            e->x + (e->crafting ? 1 : e->columns) > v->width ||
            e->y + (e->crafting ? 1 : e->rows) > v->height) return false;
        if (e->crafting && (!e->binding || e->columns > 3 || e->rows > 3)) return false;
        for (int j = 0; j < i; j++) {
            const InventoryElement *other = &v->elements[j];
            if ((other->grid || other->crafting) &&
                e->x < other->x + (other->crafting ? 1 : other->columns) && other->x < e->x + (e->crafting ? 1 : e->columns) &&
                e->y < other->y + (other->crafting ? 1 : other->rows) && other->y < e->y + (e->crafting ? 1 : e->rows)) return false;
        }
    }
    for (int i = 0; i < v->bindingCount; i++) if (grids[i] != 1) return false;
    for (int i = 0; i < v->count; i++) if (v->elements[i].crafting) {
        for (int j = 0; j < v->count; j++) if (v->elements[j].grid && v->elements[j].binding == v->elements[i].binding &&
            (v->elements[j].columns != v->elements[i].columns || v->elements[j].rows != v->elements[i].rows)) return false;
    }
    return true;
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
    Binary_U8(out, v->bindingCount);
    Binary_Write(out, v->bindingSlots, v->bindingCount);
    for (int i = 0; i < v->count; i++) {
        const InventoryElement *e = &v->elements[i];
        Binary_U8(out, e->grid); Binary_U8(out, e->binding);
        Binary_U8(out, e->crafting);
        Binary_U16(out, e->preview.itemId); Binary_U8(out, e->preview.count);
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
    v->bindingCount = Binary_ReadU8(in);
    if (v->bindingCount > INVENTORY_VIEW_BINDINGS) return false;
    for (int i = 0; i < v->bindingCount; i++) v->bindingSlots[i] = Binary_ReadU8(in);
    if (v->count > INVENTORY_VIEW_ELEMENTS) return false;
    for (int i = 0; i < v->count; i++) {
        InventoryElement *e = &v->elements[i];
        int grid = Binary_ReadU8(in);
        e->binding = Binary_ReadU8(in);
        if (grid > 1) return false;
        e->grid = grid;
        int crafting = Binary_ReadU8(in);
        if (crafting > 1) return false;
        e->crafting = crafting;
        e->preview.itemId = Binary_ReadU16(in); e->preview.count = Binary_ReadU8(in);
        if ((!e->preview.itemId != !e->preview.count) || e->preview.count > Item_GetMaxStack(e->preview.itemId) ||
            (!e->crafting && e->preview.count)) return false;
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
