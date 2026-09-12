#include "items.h"
#include "world/world.h"
#include "world/textures.h"
#include "savefile.h"
#include "networkhandler.h"
#include "packet.h"
#include "binarydata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

ItemDefinition serverItems[ITEM_LIMIT];
static bool ready;
static bool SaveNames(void) {
    BinaryWriter out = {0};
    Binary_Write(&out, "MDID", 4);
    Binary_U16(&out, 1);
    int count = 0;
    for (int i = 1; i < ITEM_LIMIT; i++)
        if (serverItems[i].identifier[0])
            count++;
    Binary_U16(&out, count);
    for (int i = 1; i < ITEM_LIMIT; i++)
        if (serverItems[i].identifier[0]) {
            Binary_U16(&out, i);
            int length = strlen(serverItems[i].identifier);
            Binary_U8(&out, length);
            Binary_Write(&out, serverItems[i].identifier, length);
        }
    bool ok = !out.failed && SaveFile_WriteAtomic("world/items.dat", out.data, out.size);
    free(out.data);
    return ok;
}
bool ServerItems_Ready(void) {
    return ready;
}
bool ServerItems_Init(void) {
    memset(serverItems, 0, sizeof(serverItems));
    ready = false;
    for (int i = 1; i < ITEM_LIMIT; i++)
        Item_SetMaxStack(i, 64);
    for (int i = 0; i < 19; i++) {
        snprintf(serverItems[i].identifier, 65, "midless:%s", itemBuiltinNames[i]);
        strcpy(serverItems[i].name, itemBuiltinNames[i]);
        serverItems[i].defined = true;
        serverItems[i].maxStack = 64;
    }
    FILE *file = fopen("world/items.dat", "rb");
    if (!file)
        return ready = errno == ENOENT;
    unsigned char *data = malloc(ITEM_LIMIT * 68);
    if (!data) {
        fclose(file);
        return false;
    }
    size_t size = fread(data, 1, ITEM_LIMIT * 68, file);
    bool ok = !ferror(file);
    fclose(file);
    BinaryReader in = {data, size};
    const void *magic = Binary_Read(&in, 4);
    if (!magic || memcmp(magic, "MDID", 4) || Binary_ReadU16(&in) != 1)
        ok = false;
    bool seen[ITEM_LIMIT] = {0};
    int count = Binary_ReadU16(&in);
    if (count >= ITEM_LIMIT)
        ok = false;
    for (int entry = 0; ok && entry < count; entry++) {
        int id = Binary_ReadU16(&in), length = Binary_ReadU8(&in);
        const char *name = (const char *)Binary_Read(&in, length);
        if (!name || id < 1 || id >= ITEM_LIMIT || seen[id] || !length || length > 64 ||
            memchr(name, 0, length)) {
            ok = false;
            break;
        }
        char identifier[65] = {0};
        memcpy(identifier, name, length);
        for (int j = 0; j < ITEM_LIMIT; j++)
            if (j != id && !strcmp(serverItems[j].identifier, identifier))
                ok = false;
        if (id < 19 && strcmp(serverItems[id].identifier, identifier))
            ok = false;
        strcpy(serverItems[id].identifier, identifier);
        seen[id] = true;
    }
    ready = ok && Binary_End(&in);
    free(data);
    if (!ready)
        TraceLog(LOG_ERROR,
                 "Cannot read world/items.dat; refusing item registration and player joins");
    return ready;
}
bool ServerItems_IsDefined(int id) {
    return id > 0 && id < ITEM_LIMIT &&
           (serverItems[id].defined || (id < 256 && ServerWorld_IsBlockDefined(id)));
}
void ServerItems_Send(struct Player *player) {
    for (int id = 1; id < ITEM_LIMIT; id++)
        if (serverItems[id].identifier[0] || serverItems[id].defined) {
            unsigned char *data = calloc(1, ITEM_DEFINITION_PACKET_SIZE);
            if (!data)
                return;
            data[0] = PACKET_DEFINE_ITEM;
            data[1] = id >> 8;
            data[2] = id;
            memcpy(data + 3, serverItems[id].identifier, 65);
            memcpy(data + 68, serverItems[id].name, 65);
            data[133] = serverItems[id].maxStack ? serverItems[id].maxStack : 64;
            data[134] = serverItems[id].texture;
            BinaryWriter bar = {0};
            ItemBar_Write(&bar, &serverItems[id].bar);
            if (bar.failed) {
                free(bar.data);
                free(data);
                return;
            }
            memcpy(data + 135, bar.data, ITEM_BAR_PACKET_SIZE);
            free(bar.data);
            ServerNetwork_Send(player, data);
        }
}
void ServerItems_DefineBlock(int id) {
    serverItems[id].defined = true;
    serverItems[id].maxStack = 64;
    strcpy(serverItems[id].name, serverWorld.blockDefinitions[id].name);
    Item_SetMaxStack(id, 64);
}

int ServerItems_Find(const char *name) {
    for (int id = 0; id < ITEM_LIMIT; id++) {
        if (!strcmp(name, serverItems[id].identifier))
            return id;
    }
    return -1;
}

const char *ServerItems_Reserve(const char *name, bool block, int *result) {
    if (!ready)
        return "item name mapping could not be loaded";
    if (!name || !name[0] || strlen(name) > 64)
        return "invalid item identifier";
    int start = block ? 19 : 256;
    int end = block ? 256 : ITEM_LIMIT;
    for (int id = start; id < end; id++) {
        if (serverItems[id].identifier[0] || serverItems[id].defined)
            continue;
        strcpy(serverItems[id].identifier, name);
        if (!SaveNames()) {
            serverItems[id].identifier[0] = 0;
            return "cannot save item name mapping";
        }
        *result = id;
        return NULL;
    }
    return "item registry is full";
}

const char *ServerItems_ReserveLegacy(int id, bool block) {
    if (!ready)
        return "item name mapping could not be loaded";
    if (id < 0 || id >= ITEM_LIMIT)
        return "item ID out of range";
    if (id < 19)
        return NULL;
    if (serverItems[id].identifier[0]) {
        if (strncmp(serverItems[id].identifier, "legacy:", 7))
            return "numeric ID belongs to a named definition";
        return NULL;
    }
    snprintf(serverItems[id].identifier, 65, "legacy:%s_%d", block ? "block" : "item", id);
    if (!SaveNames()) {
        serverItems[id].identifier[0] = 0;
        return "cannot save item mapping";
    }
    return NULL;
}

void ServerItems_Define(int id, const ItemDefinition *definition) {
    serverItems[id] = *definition;
    Item_SetMaxStack(id, definition->maxStack);
    if (!serverWorld.players)
        return;
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        if (serverWorld.players[i])
            ServerItems_Send(serverWorld.players[i]);
    }
}
