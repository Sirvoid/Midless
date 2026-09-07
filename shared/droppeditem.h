#ifndef ISLEFORGE_DROPPED_ITEM_H
#define ISLEFORGE_DROPPED_ITEM_H

#include "inventory.h"

#define ENTITY_TYPE_DROPPED_ITEM 3
#define PACKET_DROPPED_ITEM 22
#define DROPPED_ITEM_PACKET_SIZE 18

// Gameplay time in seconds; zero disables expiry. Items remain in memory across
// chunk unloads, but are not saved across server restarts.
#ifndef DROPPED_ITEM_LIFETIME
#define DROPPED_ITEM_LIFETIME 300.0f
#endif

typedef struct DroppedItem {
    ItemStack stack;
    float age, pickupDelay;
    bool viewers[256];
} DroppedItem;

#endif
