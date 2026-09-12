/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_DROPPED_ITEM_H
#define MIDLESS_DROPPED_ITEM_H

#include "packetopcodes.h"

#include "packetsizes.h"

#include "inventory.h"

#define ENTITY_TYPE_DROPPED_ITEM 3

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
