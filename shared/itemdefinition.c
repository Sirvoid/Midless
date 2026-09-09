#include "itemdefinition.h"
#include <stdatomic.h>
static atomic_uchar limits[65536];
const char *const itemBuiltinNames[19] = {"air", "stone", "dirt", "grass", "wood", "water", "sand",
    "iron_ore", "coal_ore", "gold_ore", "log", "leaves", "rose", "dandelion", "glass", "fire", "lava", "stone_slab", "wood_slab"};
void Item_SetMaxStack(int id, int count) { if (id > 0 && id < 65536) atomic_store(&limits[id], count); }
int Item_GetMaxStack(uint16_t id) {
    if (!id) return 0;
    int count = atomic_load(&limits[id]);
    return count ? count : 64;
}
