/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "version.h"
#include "serverinventory.h"
#include "world/world.h"
#include "binarydata.h"
#include "savefile.h"
#include "items.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
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
    Binary_Write(&out, "MDPI", 4); Binary_U8(&out, PLAYER_INVENTORY_VERSION);
    Binary_U8(&out, player->inventory.selectedHotbar);
    int occupied = 0;
    for (int i = 0; i < INVENTORY_SLOT_COUNT; i++) if (player->inventory.slots[i].count) occupied++;
    Binary_U8(&out, occupied);
    for (int i = 0; i < INVENTORY_SLOT_COUNT; i++) if (player->inventory.slots[i].count) {
        Binary_U8(&out, i); WriteStack(&out, player->inventory.slots[i]);
    }
    WriteStack(&out, player->inventory.cursor);
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
    Binary_U8(&out,player->metadataCount);
    for (int i=0;i<player->metadataCount;i++) {
        PlayerMetadata *entry=&player->metadata[i];
        size_t length=strlen(entry->name);
        Binary_U8(&out,length); Binary_Write(&out,entry->name,length);
        Binary_U16(&out,entry->value.version); Binary_U32(&out,entry->value.size);
        Binary_Write(&out,entry->value.data,entry->value.size);
    }
    Binary_Float(&out, player->spawnPoint.x);
    Binary_Float(&out, player->spawnPoint.y);
    Binary_Float(&out, player->spawnPoint.z);
    if (serverWorld.entities && player->entityId >= 0 && player->entityId < WORLD_MAX_ENTITIES) {
        Entity *entity = &serverWorld.entities[player->entityId];
        if (entity->active && entity->ownerPlayerId == player->id) {
            player->savedPosition = entity->position;
            player->hasSavedPosition = true;
        }
    }
    Binary_U8(&out, player->hasSavedPosition);
    Binary_Float(&out, player->savedPosition.x);
    Binary_Float(&out, player->savedPosition.y);
    Binary_Float(&out, player->savedPosition.z);
    size_t textureLength = strlen(player->texture);
    Binary_U8(&out,textureLength); Binary_Write(&out,player->texture,textureLength);
    bool ok = !out.failed && SaveFile_WriteAtomic(path, out.data, out.size);
    free(out.data);
    if (!ok) TraceLog(LOG_ERROR, "Could not save player inventory %s; original file retained", path);
    return ok;
}
static bool DecodePlayer(Player *player, const uint8_t *data, size_t size) {
    BinaryReader in = {data, size};
    const uint8_t *magic = Binary_Read(&in, 4);
    if (!magic || memcmp(magic, "MDPI", 4)) return false;
    if (Binary_ReadU8(&in) != PLAYER_INVENTORY_VERSION) return false;
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
    int namedCount = Binary_ReadU8(&in);
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
    PlayerMetadata metadata[PLAYER_METADATA_GROUPS]={0};
    int metadataCount=Binary_ReadU8(&in);
    if (metadataCount>PLAYER_METADATA_GROUPS) return false;
    for (int i=0;i<metadataCount;i++) {
        int length=Binary_ReadU8(&in);
        const uint8_t *name=Binary_Read(&in,length);
        if (!name || !length || length>64 || memchr(name,0,length)) return false;
        memcpy(metadata[i].name,name,length);
        for (int j=0;j<i;j++) if (!strcmp(metadata[j].name,metadata[i].name)) return false;
        metadata[i].value.version=Binary_ReadU16(&in);
        metadata[i].value.size=Binary_ReadU32(&in);
        if (metadata[i].value.size>65535 || (metadata[i].value.size && !metadata[i].value.version)) return false;
        metadata[i].value.data=(uint8_t *)Binary_Read(&in,metadata[i].value.size);
    }
    Vector3 spawnPoint;
    spawnPoint.x = Binary_ReadFloat(&in);
    spawnPoint.y = Binary_ReadFloat(&in);
    spawnPoint.z = Binary_ReadFloat(&in);
    if (!isfinite(spawnPoint.x) || !isfinite(spawnPoint.y) || !isfinite(spawnPoint.z) ||
        fabsf(spawnPoint.x) > 1000000 || fabsf(spawnPoint.y) > 1000000 || fabsf(spawnPoint.z) > 1000000) return false;
    Vector3 savedPosition;
    int hasSavedPosition = Binary_ReadU8(&in);
    savedPosition.x = Binary_ReadFloat(&in);
    savedPosition.y = Binary_ReadFloat(&in);
    savedPosition.z = Binary_ReadFloat(&in);
    if (hasSavedPosition > 1 || !isfinite(savedPosition.x) || !isfinite(savedPosition.y) || !isfinite(savedPosition.z) ||
        fabsf(savedPosition.x) > 1000000 || fabsf(savedPosition.y) > 1000000 || fabsf(savedPosition.z) > 1000000) return false;
    char texture[65] = {0};
    if (in.offset < in.size) {
        int length = Binary_ReadU8(&in);
        const uint8_t *name = Binary_Read(&in,length);
        if (in.failed || length>64 || (length && memchr(name,0,length))) return false;
        if (length) memcpy(texture,name,length);
    }
    if (!Binary_End(&in)) return false;
    // Copy only after the whole file passes validation. Unknown mod payloads are retained.
    for (int i=0;i<metadataCount;i++) {
        Metadata source=metadata[i].value; metadata[i].value=(Metadata){0};
        if (!Metadata_Copy(&metadata[i].value,&source)) {
            for (int j=0;j<i;j++) Metadata_Free(&metadata[j].value);
            return false;
        }
    }
    for (int i=0;i<player->metadataCount;i++) Metadata_Free(&player->metadata[i].value);
    memcpy(player->metadata,metadata,sizeof(metadata)); player->metadataCount=metadataCount;
    inventory.open = inventory.cursor.count != 0;
    player->inventory = inventory;
    strcpy(player->texture,texture);
    player->spawnPoint = spawnPoint;
    player->savedPosition = savedPosition;
    player->hasSavedPosition = hasSavedPosition != 0;
    memcpy(player->namedInventories, named, sizeof(named));
    player->namedInventoryCount = namedCount;
    player->inventoryLoaded = true;
    return true;
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
        if (!ServerPlayer_FindSpawnPoint(&player->spawnPoint)) return false;
        player->savedPosition = (Vector3){0};
        player->hasSavedPosition = false;
        player->namedInventoryCount = 0;
        for (int i=0;i<player->metadataCount;i++) Metadata_Free(&player->metadata[i].value);
        memset(player->metadata,0,sizeof(player->metadata)); player->metadataCount=0;
        player->inventoryLoaded = true;
        // Create even an empty save, so future joins never refill it.
        if (ServerInventory_Save(player)) return true;
        player->inventoryLoaded = false;
        return false;
    }
    const size_t limit=1400000; // Inventories plus sixteen maximum-sized metadata payloads.
    uint8_t *data=malloc(limit);
    if (!data) { fclose(file); return false; }
    size_t size=fread(data,1,limit,file);
    bool ok=!ferror(file) && fgetc(file)==EOF;
    fclose(file);
    if (ok) ok=DecodePlayer(player,data,size);
    free(data); return ok;
}
