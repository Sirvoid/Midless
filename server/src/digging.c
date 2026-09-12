/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "digging.h"
#include "blockdefinition.h"
#include "scripthooks.h"
#include <math.h>
#include <string.h>

static const BlockDigging builtinBlocks[BLOCK_DEFAULT_LAST_ID + 1] = {
    [0] = {.hardness = 0, .group = "gas"},      // air
    [1] = {.hardness = 3, .group = "stone"},    // stone
    [2] = {.hardness = 0.5, .group = "soil"},   // dirt
    [3] = {.hardness = 0.6, .group = "soil"},   // grass
    [4] = {.hardness = 2, .group = "wood"},     // wood
    [5] = {.hardness = 0, .group = "liquid"},   // water
    [6] = {.hardness = 0.5, .group = "soil"},   // sand
    [7] = {.hardness = 4, .group = "stone"},    // iron ore
    [8] = {.hardness = 4, .group = "stone"},    // coal ore
    [9] = {.hardness = 4, .group = "stone"},    // gold ore
    [10] = {.hardness = 2, .group = "wood"},    // log
    [11] = {.hardness = 0.2, .group = "wood"},  // leaves
    [12] = {.hardness = 0, .group = "plant"},   // rose
    [13] = {.hardness = 0, .group = "plant"},   // dandelion
    [14] = {.hardness = 0.3, .group = "glass"}, // glass
    [15] = {.hardness = 0, .group = "fire"},    // fire
    [16] = {.hardness = 0, .group = "liquid"},  // lava
    [17] = {.hardness = 2, .group = "stone"},   // stone slab
    [18] = {.hardness = 2, .group = "wood"},    // wood slab
};
static BlockDigging blocks[256];

void ServerDigging_Reset(void) {
    memset(blocks, 0, sizeof(blocks));
    for (int i = 0; i < 256; i++)
        blocks[i].hardness = 1;
    memcpy(blocks, builtinBlocks, sizeof(builtinBlocks));
}

void ServerDigging_Define(int id, const BlockDigging *definition) {
    if (id >= 0 && id < 256)
        blocks[id] = *definition;
}

double ServerDigging_Time(Player *player, Vector3 position, int block, ItemStack stack) {
    if (block < 0 || block >= 256 || blocks[block].unbreakable)
        return -1;
    double speed = ScriptHooks_DiggingSpeed(stack, blocks[block].group);
    if (!isfinite(speed) || speed <= 0)
        return -1;
    double seconds = blocks[block].hardness / speed;
    return ScriptHooks_DiggingTime(player, position, stack, seconds);
}
