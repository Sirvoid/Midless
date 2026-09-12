/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_ITEM_DEFINITION_H
#define MIDLESS_ITEM_DEFINITION_H

#include "packetopcodes.h"

#include "packetsizes.h"
#include <stdbool.h>
#include <stdint.h>
#include "inventory.h"
#define ITEM_BAR_PACKET_SIZE 76
typedef struct ItemBar {
    uint16_t version;
    uint8_t count;
    bool hideWhenFull;
    float maximum, defaultValue;
    uint8_t fields[64];
} ItemBar;
void ItemBar_Write(BinaryWriter *out, const ItemBar *bar);
bool ItemBar_Read(BinaryReader *in, ItemBar *bar);
bool ItemBar_Fraction(const ItemBar *bar, const ItemStack *stack, float *fraction);
#define ITEM_LIMIT 4096
typedef struct ItemDefinition {
    char identifier[65], name[65];
    uint8_t maxStack, texture;
    bool defined;
    ItemBar bar;
} ItemDefinition;
extern const char *const itemBuiltinNames[19];
void Item_SetMaxStack(int id, int count);
#endif
