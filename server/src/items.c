#include "items.h"
#include "world/world.h"
#include "world/textures.h"
#include "scripting/luametadata.h"
#include "savefile.h"
#include "networkhandler.h"
#include "packet.h"
#include "binarydata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

extern lua_State *L;
ItemDefinition serverItems[ITEM_LIMIT];
static bool ready;
static bool SaveNames(void) {
    BinaryWriter out = {0};
    Binary_Write(&out, "MDID", 4); Binary_U16(&out, 1);
    int count = 0;
    for (int i = 1; i < ITEM_LIMIT; i++) if (serverItems[i].identifier[0]) count++;
    Binary_U16(&out, count);
    for (int i = 1; i < ITEM_LIMIT; i++) if (serverItems[i].identifier[0]) {
        Binary_U16(&out, i);
        int length = strlen(serverItems[i].identifier);
        Binary_U8(&out, length); Binary_Write(&out, serverItems[i].identifier, length);
    }
    bool ok = !out.failed && SaveFile_WriteAtomic("world/items.dat", out.data, out.size);
    free(out.data); return ok;
}
bool ServerItems_Ready(void) { return ready; }
void ServerItems_PushId(lua_State *state, int id) {
    if (id >= 0 && id < ITEM_LIMIT && serverItems[id].identifier[0]) lua_pushstring(state, serverItems[id].identifier);
    else lua_pushinteger(state, id);
}
bool ServerItems_Init(void) {
    memset(serverItems, 0, sizeof(serverItems)); ready = false;
    for (int i = 1; i < ITEM_LIMIT; i++) Item_SetMaxStack(i, 64);
    for (int i = 0; i < 19; i++) {
        snprintf(serverItems[i].identifier, 65, "midless:%s", itemBuiltinNames[i]);
        strcpy(serverItems[i].name, itemBuiltinNames[i]);
        serverItems[i].defined = true; serverItems[i].maxStack = 64;
    }
    FILE *file = fopen("world/items.dat", "rb");
    if (!file) return ready = errno == ENOENT;
    unsigned char *data = malloc(ITEM_LIMIT * 68);
    if (!data) { fclose(file); return false; }
    size_t size = fread(data, 1, ITEM_LIMIT * 68, file);
    bool ok = !ferror(file); fclose(file);
    BinaryReader in = {data, size};
    const void *magic = Binary_Read(&in, 4);
    if (!magic || memcmp(magic, "MDID", 4) || Binary_ReadU16(&in) != 1) ok = false;
    bool seen[ITEM_LIMIT] = {0};
    int count = Binary_ReadU16(&in);
    if (count >= ITEM_LIMIT) ok = false;
    for (int entry = 0; ok && entry < count; entry++) {
        int id = Binary_ReadU16(&in), length = Binary_ReadU8(&in);
        const char *name = (const char *)Binary_Read(&in, length);
        if (!name || id < 1 || id >= ITEM_LIMIT || seen[id] || !length || length > 64 || memchr(name, 0, length)) { ok = false; break; }
        char identifier[65] = {0}; memcpy(identifier, name, length);
        for (int j = 0; j < ITEM_LIMIT; j++) if (j != id && !strcmp(serverItems[j].identifier, identifier)) ok = false;
        if (id < 19 && strcmp(serverItems[id].identifier, identifier)) ok = false;
        strcpy(serverItems[id].identifier, identifier); seen[id] = true;
    }
    ready = ok && Binary_End(&in); free(data);
    if (!ready) TraceLog(LOG_ERROR, "Cannot read world/items.dat; refusing item registration and player joins");
    return ready;
}
static const char *CheckIdentifier(lua_State *state, int index) {
    size_t length; const char *name = luaL_checklstring(state, index, &length);
    if (!length || length > 64 || memchr(name, 0, length)) luaL_error(state, "invalid item identifier");
    const char *colon = strchr(name, ':');
    if (!colon || colon == name || !colon[1] || strchr(colon + 1, ':')) luaL_error(state, "use a namespaced identifier such as example:gem");
    for (size_t i = 0; i < length; i++) if (!(islower((unsigned char)name[i]) || isdigit((unsigned char)name[i]) || strchr("_:./-", name[i])))
        luaL_error(state, "invalid character in item identifier");
    return name;
}
int ServerItems_Id(lua_State *state, int index, bool block, bool reserve) {
    if (lua_type(state, index) == LUA_TNUMBER) {
        lua_Integer id = luaL_checkinteger(state, index);
        if (id < 0 || id >= (block ? 256 : 65536)) return luaL_error(state, "item ID out of range");
        return id;
    }
    const char *name = CheckIdentifier(state, index);
    for (int id = 0; id < ITEM_LIMIT; id++) if (!strcmp(name, serverItems[id].identifier)) {
        if (block && id >= 256) return luaL_error(state, "identifier belongs to an ordinary item");
        return id;
    }
    if (!reserve || !ready) return luaL_error(state, "unknown item '%s'", name);
    int start = block ? 19 : 256, end = block ? 256 : ITEM_LIMIT;
    for (int id = start; id < end; id++) if (!serverItems[id].identifier[0] && !serverItems[id].defined) {
        strcpy(serverItems[id].identifier, name);
        if (!SaveNames()) { serverItems[id].identifier[0] = 0; return luaL_error(state, "cannot save item name mapping"); }
        return id;
    }
    return luaL_error(state, "item registry is full");
}
bool ServerItems_IsDefined(int id) { return id > 0 && id < ITEM_LIMIT && (serverItems[id].defined || (id < 256 && ServerWorld_IsBlockDefined(id))); }
void ServerItems_Send(struct Player *player) {
    for (int id = 1; id < ITEM_LIMIT; id++) if (serverItems[id].identifier[0] || serverItems[id].defined) {
        unsigned char *data = calloc(1, ITEM_DEFINITION_PACKET_SIZE);
        if (!data) return;
        data[0] = PACKET_DEFINE_ITEM; data[1] = id >> 8; data[2] = id;
        memcpy(data + 3, serverItems[id].identifier, 65); memcpy(data + 68, serverItems[id].name, 65);
        data[133] = serverItems[id].maxStack ? serverItems[id].maxStack : 64;
        data[134] = serverItems[id].texture;
        ServerNetwork_Send(player, data);
    }
}
void ServerItems_DefineBlock(int id) {
    serverItems[id].defined = true; serverItems[id].maxStack = 64;
    strcpy(serverItems[id].name, serverWorld.blockDefinitions[id].name);
    Item_SetMaxStack(id, 64);
}
int ServerItems_Define(void) {
    int id = ServerItems_Declare(L, false);
    if (id < 256 || id >= ITEM_LIMIT || serverItems[id].defined) return luaL_error(L, "item already defined or ID out of range");
    luaL_checktype(L, 2, LUA_TTABLE);
    ItemDefinition definition = serverItems[id];
    lua_getfield(L, 2, "name");
    size_t length; const char *name = luaL_checklstring(L, -1, &length);
    if (length > 64 || memchr(name, 0, length)) return luaL_error(L, "item name is too long");
    memcpy(definition.name, name, length + 1); lua_pop(L, 1);
    lua_getfield(L, 2, "max_stack");
    int max = lua_isnil(L, -1) ? 64 : luaL_checkinteger(L, -1); lua_pop(L, 1);
    if (max < 1 || max > 64) return luaL_error(L, "max_stack must be 1..64");
    lua_getfield(L, 2, "texture");
    int texture = ServerTextures_Find(luaL_checkstring(L, -1));
    if (texture < 0) return luaL_error(L, "unknown item texture; register it with define_texture first");
    definition.texture = texture; lua_pop(L, 1);
    if (!ServerTextures_ItemSize(definition.texture)) return luaL_error(L, "item textures must be at most 64 by 64 pixels");
    lua_getfield(L, 2, "held_model");
    if (!lua_isnil(L, -1) && strcmp(luaL_checkstring(L, -1), "sprite")) return luaL_error(L, "ordinary items use held_model = sprite");
    lua_pop(L, 1);
    definition.maxStack = max; definition.defined = true;
    LuaMetadata_DefineItem(id, 2);
    serverItems[id] = definition; Item_SetMaxStack(id, max);
    for (int p = 0; p < WORLD_MAX_PLAYERS; p++) if (serverWorld.players[p]) ServerItems_Send(serverWorld.players[p]);
    return 0;
}
int ServerItems_Declare(lua_State *state, bool block) {
    if (!ready) return luaL_error(state, "item name mapping could not be loaded");
    luaL_checktype(state, 2, LUA_TTABLE);
    int id = ServerItems_Id(state, 1, block, true);
    if (lua_type(state, 1) == LUA_TNUMBER && id >= 19 && id < ITEM_LIMIT) {
        if (serverItems[id].identifier[0] && strncmp(serverItems[id].identifier, "legacy:", 7))
            return luaL_error(state, "numeric ID belongs to a named definition");
        if (!serverItems[id].identifier[0]) {
            snprintf(serverItems[id].identifier, 65, "legacy:%s_%d", block ? "block" : "item", id);
            if (!SaveNames()) { serverItems[id].identifier[0] = 0; return luaL_error(state, "cannot save item mapping"); }
        }
    }
    return id;
}
void ServerItems_ReadStack(lua_State *state, int index, ItemStack *stack) {
    *stack = (ItemStack){0};
    if (lua_isnil(state, index)) return;
    index = lua_absindex(state, index); luaL_checktype(state, index, LUA_TTABLE);
    lua_getfield(state, index, "id"); stack->itemId = ServerItems_Id(state, -1, false, false); lua_pop(state, 1);
    lua_getfield(state, index, "count"); int count = luaL_checkinteger(state, -1); lua_pop(state, 1);
    if (!stack->itemId || count < 1 || count > Item_GetMaxStack(stack->itemId)) luaL_error(state, "invalid stack count");
    stack->count = count;
    LuaMetadata_ReadItem(state, index, stack);
}
void ServerItems_PushStack(lua_State *state, ItemStack stack) {
    if (!stack.count) { lua_pushnil(state); return; }
    lua_createtable(state, 0, 3);
    if (stack.itemId < ITEM_LIMIT && serverItems[stack.itemId].identifier[0]) lua_pushstring(state, serverItems[stack.itemId].identifier);
    else lua_pushinteger(state, stack.itemId);
    lua_setfield(state, -2, "id");
    lua_pushinteger(state, stack.count); lua_setfield(state, -2, "count");
    LuaMetadata_PushItem(state, stack); lua_setfield(state, -2, "metadata");
    if (stack.metadataSize) {
        lua_pushlstring(state, (const char *)stack.metadata, stack.metadataSize); lua_setfield(state, -2, "_metadata");
        lua_pushinteger(state, stack.metadataVersion); lua_setfield(state, -2, "_metadata_version");
    }
}
