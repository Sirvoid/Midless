#include "serverinventory.h"
#include "world/world.h"
#include "binarydata.h"
#include "savefile.h"
#include "items.h"
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
static void WriteStack(BinaryWriter *out, ItemStack stack) { ItemStack_Write(out, stack); }
static ItemStack ReadStack(BinaryReader *in) { return ItemStack_Read(in); }
bool ServerInventory_Save(Player *player) {
    if (!player->inventoryLoaded) return true;
    char path[160];
    if (!ServerItems_Ready() || !Filename(player, path)) return false;
    BinaryWriter out = {0};
    Binary_Write(&out, "MDPI", 4); Binary_U8(&out, player->namedInventoryCount ? 2 : 1);
    Binary_U8(&out, player->inventory.selectedHotbar);
    int occupied = 0;
    for (int i = 0; i < INVENTORY_SLOT_COUNT; i++) if (player->inventory.slots[i].count) occupied++;
    Binary_U8(&out, occupied);
    for (int i = 0; i < INVENTORY_SLOT_COUNT; i++) if (player->inventory.slots[i].count) {
        Binary_U8(&out, i); WriteStack(&out, player->inventory.slots[i]);
    }
    WriteStack(&out, player->inventory.cursor);
    if (player->namedInventoryCount) {
        Binary_U8(&out, player->namedInventoryCount);
        for (int n = 0; n < player->namedInventoryCount; n++) {
            NamedInventory *inventory = &player->namedInventories[n];
            size_t length = strlen(inventory->name);
            Binary_U8(&out, length); Binary_Write(&out, inventory->name, length);
            Binary_U8(&out, inventory->count);
            int occupied = 0;
            for (int i = 0; i < inventory->count; i++) if (inventory->slots[i].count) occupied++;
            Binary_U8(&out, occupied);
            for (int i = 0; i < inventory->count; i++) if (inventory->slots[i].count) {
                Binary_U8(&out, i); WriteStack(&out, inventory->slots[i]);
            }
        }
    }
    bool ok = !out.failed && SaveFile_WriteAtomic(path, out.data, out.size);
    free(out.data);
    if (!ok) TraceLog(LOG_ERROR, "Could not save player inventory %s; original file retained", path);
    return ok;
}
bool ServerInventory_Load(Player *player) {
    player->inventoryLoaded = false;
    char path[160];
    if (!ServerItems_Ready() || !Filename(player, path)) return false;
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
        player->namedInventoryCount = 0;
        ServerInventory_GiveStartingBlocks(player);
        player->inventoryLoaded = true;
        // Create even an empty save, so future joins never refill it.
        if (ServerInventory_Save(player)) return true;
        player->inventoryLoaded = false;
        return false;
    }
    uint8_t data[300000]; // Main inventory plus at most 16 named inventories.
    size_t size = fread(data, 1, sizeof(data), file);
    bool ok = !ferror(file);
    fclose(file);
    BinaryReader in = {data, size};
    const uint8_t *magic = Binary_Read(&in, 4);
    if (!ok || !magic || memcmp(magic, "MDPI", 4)) return false;
    int version = Binary_ReadU8(&in);
    if (version != 1 && version != 2) return false;
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
    NamedInventory named[PLAYER_INVENTORIES] = {0};
    int namedCount = version == 2 ? Binary_ReadU8(&in) : 0;
    if (namedCount > PLAYER_INVENTORIES) return false;
    for (int n = 0; n < namedCount; n++) {
        int length = Binary_ReadU8(&in);
        const uint8_t *name = Binary_Read(&in, length);
        if (!name || !length || length > 64 || memchr(name, 0, length)) return false;
        memcpy(named[n].name, name, length);
        for (int j = 0; j < n; j++) if (!strcmp(named[j].name, named[n].name)) return false;
        named[n].count = Binary_ReadU8(&in);
        int occupied = Binary_ReadU8(&in), previousSlot = -1;
        if (!named[n].count || occupied > named[n].count) return false;
        for (int i = 0; i < occupied; i++) {
            int slot = Binary_ReadU8(&in);
            if (slot <= previousSlot || slot >= named[n].count) return false;
            named[n].slots[slot] = ReadStack(&in);
            if (!named[n].slots[slot].count) return false;
            previousSlot = slot;
        }
    }
    if (!Binary_End(&in)) return false;
    inventory.open = inventory.cursor.count != 0;
    player->inventory = inventory;
    memcpy(player->namedInventories, named, sizeof(named));
    player->namedInventoryCount = namedCount;
    player->inventoryLoaded = true;
    return true;
}
