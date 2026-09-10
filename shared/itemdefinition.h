#ifndef MIDLESS_ITEM_DEFINITION_H
#define MIDLESS_ITEM_DEFINITION_H

#include "packetsizes.h"
#include <stdbool.h>
#include <stdint.h>
#define ITEM_LIMIT 4096
#define PACKET_DEFINE_ITEM 24
typedef struct ItemDefinition {
    char identifier[65], name[65];
    uint8_t maxStack, texture;
    bool defined;
} ItemDefinition;
extern const char *const itemBuiltinNames[19];
void Item_SetMaxStack(int id, int count);
#endif
