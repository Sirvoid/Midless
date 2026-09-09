#ifndef MIDLESS_DROPPED_ITEM_H
#define MIDLESS_DROPPED_ITEM_H

#include "inventory.h"

#define ENTITY_TYPE_DROPPED_ITEM 3
#define PACKET_DROPPED_ITEM 22
#define DROPPED_ITEM_PACKET_SIZE (21 + ITEM_METADATA_BYTES)

// Gameplay time in seconds; zero disables expiry. Items are saved with their
// chunk; age and pickup delay pause while the chunk is unloaded.
#ifndef DROPPED_ITEM_LIFETIME
#define DROPPED_ITEM_LIFETIME 300.0f
#endif

typedef struct DroppedItem {
    ItemStack stack;
    float age, pickupDelay;
    bool viewers[256];
} DroppedItem;

#endif
