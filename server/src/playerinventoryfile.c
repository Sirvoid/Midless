#include "serverinventory.h"
#include "world/world.h"
#include "binarydata.h"
#include "savefile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#if defined(OS_WINDOWS)
#include <direct.h>
#else
#include <sys/stat.h>
#endif

static bool Filename(const Player *player, char path[160]) {
    size_t length = player->name ? strlen(player->name) : 0;
    if (!length || length > 64) return false;
    // Hex encoding is collision-free and keeps names out of filesystem paths.
    strcpy(path, "world/players/");
    const char *hex = "0123456789abcdef";
    for (size_t i = 0; i < length; i++) {
        unsigned char c = player->name[i];
        path[14 + 2*i] = hex[c >> 4];
        path[15 + 2*i] = hex[c & 15];
    }
    strcpy(path + 14 + 2*length, ".dat");
    return true;
}
static void WriteStack(BinaryWriter *out, ItemStack stack) {
    Binary_U16(out, stack.itemId); Binary_U8(out, stack.count);
}
static ItemStack ReadStack(BinaryReader *in) {
    ItemStack stack = {0};
    stack.itemId = Binary_ReadU16(in); stack.count = Binary_ReadU8(in);
    if ((!stack.itemId != !stack.count) || stack.count > Item_GetMaxStack(stack.itemId)) in->failed = true;
    return stack;
}
bool ServerInventory_Save(Player *player) {
    if (!player->inventoryLoaded) return true;
    char path[160];
    if (!Filename(player, path)) return false;
    BinaryWriter out = {0};
    Binary_Write(&out, "MDPI", 4); Binary_U8(&out, 1);
    Binary_U8(&out, player->inventory.selectedHotbar);
    int occupied = 0;
    for (int i = 0; i < INVENTORY_SLOT_COUNT; i++) if (player->inventory.slots[i].count) occupied++;
    Binary_U8(&out, occupied);
    for (int i = 0; i < INVENTORY_SLOT_COUNT; i++) if (player->inventory.slots[i].count) {
        Binary_U8(&out, i); WriteStack(&out, player->inventory.slots[i]);
    }
    WriteStack(&out, player->inventory.cursor);
    bool ok = !out.failed && SaveFile_WriteAtomic(path, out.data, out.size);
    free(out.data);
    if (!ok) TraceLog(LOG_ERROR, "Could not save player inventory %s; original file retained", path);
    return ok;
}
bool ServerInventory_Load(Player *player) {
    player->inventoryLoaded = false;
    char path[160];
    if (!Filename(player, path)) return false;
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *other = serverWorld.players[i];
        if (other && other != player && other->name && !strcmp(other->name, player->name)) return false;
    }
#if defined(OS_WINDOWS)
    if (_mkdir("world/players") && errno != EEXIST) return false;
#else
    if (mkdir("world/players", 0755) && errno != EEXIST) return false;
#endif
    FILE *file = fopen(path, "rb");
    if (!file) {
        if (errno != ENOENT) return false;
        Inventory_Init(&player->inventory);
        ServerInventory_GiveStartingBlocks(player);
        player->inventoryLoaded = true;
        // Create even an empty save, so future joins never refill it.
        if (ServerInventory_Save(player)) return true;
        player->inventoryLoaded = false;
        return false;
    }
    uint8_t data[155]; // At most 154 bytes; one extra byte detects oversized files.
    size_t size = fread(data, 1, sizeof(data), file);
    bool ok = !ferror(file);
    fclose(file);
    BinaryReader in = {data, size};
    const uint8_t *magic = Binary_Read(&in, 4);
    if (!ok || !magic || memcmp(magic, "MDPI", 4) || Binary_ReadU8(&in) != 1) return false;
    Inventory inventory; Inventory_Init(&inventory);
    inventory.selectedHotbar = Binary_ReadU8(&in);
    int count = Binary_ReadU8(&in), previous = -1;
    if (inventory.selectedHotbar >= INVENTORY_HOTBAR_SLOTS || count > INVENTORY_SLOT_COUNT) return false;
    for (int i = 0; i < count; i++) {
        int slot = Binary_ReadU8(&in);
        if (slot <= previous || slot >= INVENTORY_SLOT_COUNT) return false;
        inventory.slots[slot] = ReadStack(&in);
        if (!inventory.slots[slot].count) return false;
        previous = slot;
    }
    inventory.cursor = ReadStack(&in);
    if (!Binary_End(&in)) return false;
    inventory.open = inventory.cursor.count != 0;
    player->inventory = inventory;
    player->inventoryLoaded = true;
    return true;
}
