/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "itemdefinition.h"
#include <stdatomic.h>
#include <math.h>
#include <string.h>

void ItemBar_Write(BinaryWriter *out, const ItemBar *bar) {
    Binary_U16(out, bar->version); Binary_U8(out, bar->count); Binary_U8(out, bar->hideWhenFull);
    Binary_Float(out, bar->maximum); Binary_Float(out, bar->defaultValue);
    Binary_Write(out, bar->fields, sizeof(bar->fields));
}
bool ItemBar_Read(BinaryReader *in, ItemBar *bar) {
    *bar = (ItemBar){0};
    bar->version = Binary_ReadU16(in); bar->count = Binary_ReadU8(in);
    int hide = Binary_ReadU8(in); bar->hideWhenFull = hide;
    bar->maximum = Binary_ReadFloat(in); bar->defaultValue = Binary_ReadFloat(in);
    const uint8_t *fields = Binary_Read(in, sizeof(bar->fields));
    if (!fields || hide > 1 || bar->count > 64) return false;
    memcpy(bar->fields, fields, sizeof(bar->fields));
    if (!bar->count) return !in->failed;
    if (!bar->version || !isfinite(bar->maximum) || bar->maximum <= 0 || !isfinite(bar->defaultValue)) return false;
    for (int i = 0; i < bar->count; i++) if (!bar->fields[i] || bar->fields[i] > 66) return false;
    return bar->fields[bar->count - 1] != 66 && !in->failed;
}
bool ItemBar_Fraction(const ItemBar *bar, const ItemStack *stack, float *fraction) {
    if (!stack->count || !bar->count || bar->count > 64 || !isfinite(bar->maximum) || bar->maximum <= 0) return false;
    double value = bar->defaultValue;
    if (stack->metadataSize) {
        if (stack->metadataSize > ITEM_METADATA_BYTES || stack->metadataVersion != bar->version) return false;
        BinaryReader in = {stack->metadata, stack->metadataSize};
        uint8_t byte = 0; int remaining = 0;
        for (int i = 0; i < bar->count; i++) {
            int code = bar->fields[i];
            if (code >= 1 && code <= 64) {
                int bits = code > 32 ? code - 32 : code;
                uint32_t number = 0;
                for (int bit = 0; bit < bits; bit++) {
                    if (!remaining) { byte = Binary_ReadU8(&in); remaining = 8; }
                    number |= (uint32_t)(byte & 1) << bit;
                    byte >>= 1; remaining--;
                }
                value = number;
                if (code > 32 && (number & (1u << (bits - 1)))) value -= (int64_t)1 << bits;
            } else {
                if (byte) return false;
                remaining = 0;
                if (code == 65) value = Binary_ReadFloat(&in);
                else if (code == 66 && i + 1 < bar->count) {
                    uint32_t size = Binary_ReadVarUInt(&in);
                    Binary_Read(&in, size);
                } else return false;
            }
            if (in.failed) return false;
        }
    }
    if (!isfinite(value) || (bar->hideWhenFull && value >= bar->maximum)) return false;
    *fraction = (float)fmin(1, fmax(0, value / bar->maximum));
    return true;
}
static atomic_uchar limits[65536];
const char *const itemBuiltinNames[19] = {"air", "stone", "dirt", "grass", "wood", "water", "sand",
    "iron_ore", "coal_ore", "gold_ore", "log", "leaves", "rose", "dandelion", "glass", "fire", "lava", "stone_slab", "wood_slab"};
void Item_SetMaxStack(int id, int count) { if (id > 0 && id < 65536) atomic_store(&limits[id], count); }
int Item_GetMaxStack(uint16_t id) {
    if (!id) return 0;
    int count = atomic_load(&limits[id]);
    return count ? count : 64;
}
