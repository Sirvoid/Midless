/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "luaplayers.h"
#include "../metadatainternal.h"
#include "../scripthooks.h"
#include "luaitems.h"
#include "version.h"
#include "luametadata.h"
#include "blockstates.h"
#include "binarydata.h"
#include "luaentities.h"
#include "../items.h"
#include "../world/world.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <limits.h>

extern lua_State *L;
#define METADATA_MAX_SCHEMAS 512
#define METADATA_MAX_FIELDS 64
#define BLOCK_OBJECT "midless.Block"
#define INVENTORY_REF "midless.MetadataInventory"

static int ruleReferences[METADATA_MAX_SCHEMAS];
static struct {
    char name[65];
    int schema;
} playerSchemas[PLAYER_METADATA_GROUPS];
static int playerSchemaCount;
#define PLAYER_CHANGE_LIMIT 256
static struct {
    char key[130];
    int callback;
} playerListeners[PLAYER_CHANGE_LIMIT];
static int playerListenerCount;
typedef struct PlayerChange {
    int player, oldValue, newValue, listeners;
    uint64_t connection;
    char key[130];
} PlayerChange;
static PlayerChange playerChanges[PLAYER_CHANGE_LIMIT];
static int playerChangeCount;
static bool notifyingPlayers;

static void NotifyPlayer(Player *player, const char *key, int oldValue, int newValue) {
    if (lua_rawequal(L, oldValue, newValue))
        return;
    bool listening = false;
    for (int i = 0; i < playerListenerCount; i++)
        if (!strcmp(playerListeners[i].key, key))
            listening = true;
    if (!listening)
        return;
    if (playerChangeCount == PLAYER_CHANGE_LIMIT) {
        TraceLog(LOG_WARNING,
                 "Player metadata callback limit reached; further notifications skipped");
        return;
    }
    PlayerChange *change = &playerChanges[playerChangeCount++];
    change->player = player->id;
    change->connection = player->connectionId;
    change->listeners = playerListenerCount;
    strcpy(change->key, key);
    lua_pushvalue(L, oldValue);
    change->oldValue = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushvalue(L, newValue);
    change->newValue = luaL_ref(L, LUA_REGISTRYINDEX);
    if (notifyingPlayers)
        return;
    notifyingPlayers = true;
    // Nested writes append here; never recursively invoke another listener.
    for (int next = 0; next < playerChangeCount; next++) {
        PlayerChange current = playerChanges[next];
        for (int i = 0; i < current.listeners; i++) {
            Player *owner = serverWorld.players ? serverWorld.players[current.player] : NULL;
            if (!owner || owner->connectionId != current.connection || owner->disconnected)
                break;
            if (strcmp(playerListeners[i].key, current.key))
                continue;
            int top = lua_gettop(L);
            lua_rawgeti(L, LUA_REGISTRYINDEX, playerListeners[i].callback);
            LuaPlayers_Push(owner);
            lua_rawgeti(L, LUA_REGISTRYINDEX, current.oldValue);
            lua_rawgeti(L, LUA_REGISTRYINDEX, current.newValue);
            if (lua_pcall(L, 3, 0, 0) != LUA_OK)
                TraceLog(LOG_WARNING, "Player metadata callback: %s", lua_tostring(L, -1));
            lua_settop(L, top);
        }
        luaL_unref(L, LUA_REGISTRYINDEX, current.oldValue);
        luaL_unref(L, LUA_REGISTRYINDEX, current.newValue);
    }
    playerChangeCount = 0;
    notifyingPlayers = false;
}

static int timerCallbacks[256], inventoryCallbacks[256];
static bool notifying;

// Pending notifications run after inventory transactions have fully committed.
typedef struct InventoryChange {
    Vector3 position;
    int block;
    char field[65];
} InventoryChange;
static InventoryChange *changes;
static int changeCount, changeCapacity;

static int Integer(lua_State *state, int index, int min, int max) {
    lua_Integer value = luaL_checkinteger(state, index);
    if (value < min || value > max)
        luaL_error(state, "integer is outside the schema range");
    return (int)value;
}
static int IntegerField(lua_State *state, int index, const char *name, int fallback, int min,
                        int max) {
    lua_getfield(state, index, name);
    int value = lua_isnil(state, -1) ? fallback : Integer(state, -1, min, max);
    lua_pop(state, 1);
    return value;
}
static void ReadSchema(lua_State *state, int index, int version, MetadataSchema *layout) {
    index = lua_absindex(state, index);
    luaL_checktype(state, index, LUA_TTABLE);
    *layout = (MetadataSchema){.version = version};
    size_t count = lua_rawlen(state, index);
    if (count > METADATA_MAX_FIELDS)
        luaL_error(state, "metadata supports at most 64 fields");
    layout->count = count;
    for (int i = 0; i < layout->count; i++) {
        lua_rawgeti(state, index, i + 1);
        luaL_checktype(state, -1, LUA_TTABLE);
        int entry = lua_gettop(state);
        MetadataField *field = &layout->fields[i];
        lua_getfield(state, entry, "name");
        size_t length;
        const char *name = luaL_checklstring(state, -1, &length);
        if (!length || length > 64 || memchr(name, 0, length))
            luaL_error(state, "invalid metadata field name");
        memcpy(field->name, name, length + 1);
        for (int j = 0; j < i; j++)
            if (!strcmp(layout->fields[j].name, name))
                luaL_error(state, "duplicate metadata field");
        lua_pop(state, 1);
        lua_getfield(state, entry, "type");
        const char *type = luaL_checkstring(state, -1);
        const char *types[] = {"uint", "int", "bool", "float", "string", "inventory"};
        int kind = 0;
        while (kind < 6 && strcmp(type, types[kind]))
            kind++;
        if (kind == 6)
            luaL_error(state, "unknown metadata field type");
        field->type = kind;
        lua_pop(state, 1);
        field->bits = kind == FIELD_BOOL ? 1 : IntegerField(state, entry, "bits", 16, 1, 32);
        field->limit = kind == FIELD_INVENTORY
                           ? IntegerField(state, entry, "slots", 27, 1, 255)
                           : IntegerField(state, entry, "max_length", 256, 0, 4096);
        lua_getfield(state, entry, "default");
        if (!lua_isnil(state, -1)) {
            if (kind == FIELD_STRING) {
                const char *value = luaL_checklstring(state, -1, &length);
                if (length > 256 || length > (size_t)field->limit || memchr(value, 0, length))
                    luaL_error(state, "invalid string default");
                memcpy(field->defaultString, value, length + 1);
            } else if (kind == FIELD_BOOL) {
                luaL_checktype(state, -1, LUA_TBOOLEAN);
                field->defaultNumber = lua_toboolean(state, -1);
            } else if (kind == FIELD_INVENTORY) {
                luaL_error(state, "inventory default is always empty");
            } else {
                double value = luaL_checknumber(state, -1);
                double min = kind == FIELD_INT ? -ldexp(1, field->bits - 1) : 0;
                double max =
                    kind == FIELD_INT ? ldexp(1, field->bits - 1) - 1 : ldexp(1, field->bits) - 1;
                if (!isfinite(value) ||
                    (kind != FIELD_FLOAT &&
                     (value != floor(value) || value < min || value > max)) ||
                    (kind == FIELD_FLOAT && !isfinite((float)value)))
                    luaL_error(state, "invalid metadata default");
                field->defaultNumber = kind == FIELD_FLOAT ? (float)value : value;
            }
        }
        lua_pop(state, 2);
    }
}

int LuaMetadata_Register(lua_State *state, int definition) {
    definition = lua_absindex(state, definition);
    lua_getfield(state, definition, "metadata");
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        return -1;
    }
    if (serverMetadataSchemaCount == METADATA_MAX_SCHEMAS)
        return luaL_error(state, "metadata schema registry is full");
    MetadataSchema layout;
    int version =
        IntegerField(state, definition, "metadata_version", METADATA_DEFAULT_VERSION, 1, 65535);
    ReadSchema(state, -1, version, &layout);
    lua_pop(state, 1);
    lua_newtable(state);
    int rules = lua_gettop(state);
    lua_getfield(state, definition, "metadata");
    for (int i = 0; i < layout.count; i++) {
        lua_rawgeti(state, -1, i + 1);
        lua_getfield(state, -1, "rules");
        if (!lua_isnil(state, -1)) {
            if (layout.fields[i].type != FIELD_INVENTORY)
                luaL_error(state, "rules require an inventory");
            luaL_checktype(state, -1, LUA_TTABLE);
            int input = lua_gettop(state);
            lua_pushnil(state);
            while (lua_next(state, input)) {
                Integer(state, -2, 1, layout.fields[i].limit);
                lua_pop(state, 1);
            }
            lua_newtable(state);
            for (int slot = 1; slot <= layout.fields[i].limit; slot++) {
                lua_rawgeti(state, input, slot);
                if (!lua_isnil(state, -1)) {
                    luaL_checktype(state, -1, LUA_TTABLE);
                    lua_getfield(state, -1, "insert");
                    if (!lua_isnil(state, -1))
                        luaL_checktype(state, -1, LUA_TBOOLEAN);
                    bool denied = lua_isboolean(state, -1) && !lua_toboolean(state, -1);
                    lua_pop(state, 1);
                    lua_getfield(state, -1, "items");
                    if (denied)
                        lua_pushboolean(state, false);
                    else if (!lua_isnil(state, -1)) {
                        luaL_checktype(state, -1, LUA_TTABLE);
                        int items = lua_gettop(state);
                        lua_newtable(state);
                        for (int j = 1; j <= lua_rawlen(state, items); j++) {
                            lua_rawgeti(state, items, j);
                            int id = LuaItems_Id(state, -1, false, false);
                            lua_pop(state, 1);
                            lua_pushboolean(state, true);
                            lua_rawseti(state, -2, id);
                        }
                    } else
                        lua_pushboolean(state, true);
                    lua_rawseti(state, -4, slot);
                    lua_pop(state, 1);
                }
                lua_pop(state, 1);
            }
            lua_setfield(state, rules, layout.fields[i].name);
        }
        lua_pop(state, 2);
    }
    lua_pop(state, 1);
    int id = ServerMetadata_Register(&layout);
    if (id < 0)
        return luaL_error(state, "metadata defaults exceed the size limit or memory is exhausted");
    ruleReferences[id] = luaL_ref(state, LUA_REGISTRYINDEX);
    return id;
}
void LuaMetadata_DefineBlock(int blockId, int definition) {
    const char *names[] = {"on_timer", "on_inventory_changed"};
    for (int i = 0; i < 2; i++) {
        lua_getfield(L, definition, names[i]);
        if (!lua_isnil(L, -1))
            luaL_checktype(L, -1, LUA_TFUNCTION);
        lua_pop(L, 1);
    }
    if (serverBlockSchemas[blockId] >= 0)
        luaL_error(L, "block metadata schema is already registered");
    serverBlockSchemas[blockId] = LuaMetadata_Register(L, definition);
    luaL_unref(L, LUA_REGISTRYINDEX, timerCallbacks[blockId]);
    lua_getfield(L, definition, "on_timer");
    timerCallbacks[blockId] = luaL_ref(L, LUA_REGISTRYINDEX);
    luaL_unref(L, LUA_REGISTRYINDEX, inventoryCallbacks[blockId]);
    lua_getfield(L, definition, "on_inventory_changed");
    inventoryCallbacks[blockId] = luaL_ref(L, LUA_REGISTRYINDEX);
}

// Numeric fields share bytes. Variable-size fields start at the next byte.
static void PushDefault(lua_State *state, const MetadataField *field) {
    if (field->type == FIELD_STRING)
        lua_pushstring(state, field->defaultString);
    else if (field->type == FIELD_INVENTORY)
        lua_newtable(state);
    else if (field->type == FIELD_BOOL)
        lua_pushboolean(state, field->defaultNumber != 0);
    else
        lua_pushnumber(state, field->defaultNumber);
}
// Read just one field. Validation uses the same reader without creating Lua values.
static bool ReadField(lua_State *state, const MetadataField *field, BinaryReader *in, MetadataBits *bits) {
    BinaryReader source = *in;
    MetadataBits sourceBits = *bits;
    if (!MetadataCodec_ReadField(field, in, bits))
        return false;
    if (!state)
        return true;
    if (field->type <= FIELD_BOOL) {
        uint32_t number = MetadataCodec_ReadBits(&source, &sourceBits, field->bits);
        if (field->type == FIELD_BOOL)
            lua_pushboolean(state, number);
        else {
            int64_t value = number;
            if (field->type == FIELD_INT && (number & (1u << (field->bits - 1))))
                value -= (int64_t)1 << field->bits;
            lua_pushinteger(state, value);
        }
    } else if (field->type == FIELD_FLOAT) {
        lua_pushnumber(state, Binary_ReadFloat(&source));
    } else if (field->type == FIELD_STRING) {
        uint32_t size = Binary_ReadVarUInt(&source);
        const uint8_t *string = Binary_Read(&source, size);
        lua_pushlstring(state, (const char *)string, size);
    } else {
        int count = Binary_ReadU8(&source);
        lua_newtable(state);
        for (int i = 0; i < count; i++) {
            int slot = Binary_ReadU8(&source);
            LuaItems_PushStack(state, ItemStack_Read(&source));
            lua_rawseti(state, -2, slot + 1);
        }
    }
    return true;
}
static void WriteField(lua_State *state, const MetadataField *field, int input, BinaryWriter *out) {
    if (!input || lua_isnil(state, input)) {
        MetadataBits bits = {0};
        MetadataCodec_WriteDefault(out, &bits, field);
        MetadataCodec_FlushBits(out, &bits);
        return;
    }
    if (field->type <= FIELD_BOOL) {
        double number;
        if (field->type == FIELD_BOOL) {
            luaL_checktype(state, input, LUA_TBOOLEAN);
            number = lua_toboolean(state, input);
        } else
            number = luaL_checknumber(state, input);
        double min = field->type == FIELD_INT ? -ldexp(1, field->bits - 1) : 0;
        double max =
            field->type == FIELD_INT ? ldexp(1, field->bits - 1) - 1 : ldexp(1, field->bits) - 1;
        if (!isfinite(number) || number != floor(number) || number < min || number > max)
            luaL_error(state, "metadata '%s' is out of range", field->name);
        MetadataBits bits = {0};
        MetadataCodec_WriteBits(out, &bits, (uint32_t)(int64_t)number, field->bits);
        MetadataCodec_FlushBits(out, &bits);
    } else if (field->type == FIELD_FLOAT) {
        float number = luaL_checknumber(state, input);
        if (!isfinite(number))
            luaL_error(state, "metadata float must be finite");
        Binary_Float(out, number);
    } else if (field->type == FIELD_STRING) {
        size_t size;
        const char *text = luaL_checklstring(state, input, &size);
        if (size > (size_t)field->limit)
            luaL_error(state, "metadata string is too long");
        Binary_VarUInt(out, size);
        Binary_Write(out, text, size);
    } else {
        luaL_checktype(state, input, LUA_TTABLE);
        ItemStack stacks[255] = {0};
        int count = 0;
        lua_pushnil(state);
        while (lua_next(state, input)) {
            int slot = Integer(state, -2, 1, field->limit) - 1;
            luaL_checktype(state, -1, LUA_TTABLE);
            LuaItems_ReadStack(state, -1, &stacks[slot]);
            count++;
            lua_pop(state, 1);
        }
        Binary_U8(out, count);
        for (int slot = 0; slot < field->limit; slot++)
            if (stacks[slot].count) {
                Binary_U8(out, slot);
                ItemStack_Write(out, stacks[slot]);
            }
    }
}
static MetadataSchema *GetSchema(lua_State *state, int id) {
    if (id < 0 || id >= serverMetadataSchemaCount)
        luaL_error(state, "no metadata schema is registered");
    return serverMetadataSchemas[id];
}
static const MetadataField *FindField(lua_State *state, MetadataSchema *schema, int key) {
    const char *name = luaL_checkstring(state, key);
    for (int i = 0; i < schema->count; i++)
        if (!strcmp(name, schema->fields[i].name))
            return &schema->fields[i];
    luaL_error(state, "unknown metadata field '%s'", name);
    return NULL;
}
// Find a field's byte/bit location without decoding unrelated values or inventories.
static BinaryReader Locate(lua_State *state, MetadataSchema *schema, const Metadata *value,
                           const MetadataField *field, MetadataBits *bits) {
    if (value->size && value->version != schema->version)
        luaL_error(state, "metadata schema version does not match");
    const Metadata *source = value->size ? value : &schema->defaults;
    BinaryReader in = {source->data, source->size};
    for (const MetadataField *previous = schema->fields; previous != field; previous++)
        if (!MetadataCodec_SkipField(previous, &in, bits))
            luaL_error(state, "invalid metadata payload");
    if (field->type > FIELD_BOOL)
        *bits = (MetadataBits){0};
    return in;
}
int LuaMetadata_Get(lua_State *state, int id, Metadata *value, int key) {
    MetadataSchema *schema = GetSchema(state, id);
    const MetadataField *field = FindField(state, schema, key);
    if (!value->size) {
        PushDefault(state, field);
        return 1;
    }
    MetadataBits bits = {0};
    BinaryReader in = Locate(state, schema, value, field, &bits);
    if (!ReadField(state, field, &in, &bits))
        return luaL_error(state, "invalid metadata payload");
    return 1;
}
static BinaryWriter *NewWriter(lua_State *state) {
    BinaryWriter *out = lua_newuserdata(state, sizeof(*out));
    *out = (BinaryWriter){0};
    luaL_setmetatable(state, "midless.MetadataWriter");
    return out;
}
int LuaMetadata_Set(lua_State *state, int id, Metadata *value, int key, int input) {
    if (input)
        input = lua_absindex(state, input);
    MetadataSchema *schema = GetSchema(state, id);
    const MetadataField *field = FindField(state, schema, key);
    MetadataBits bits = {0};
    BinaryReader in = Locate(state, schema, value, field, &bits);
    size_t startBit = in.offset * 8 - bits.count;
    size_t start = in.offset;
    if (!MetadataCodec_SkipField(field, &in, &bits))
        return luaL_error(state, "invalid metadata payload");
    BinaryWriter *replacement = NewWriter(state);
    WriteField(state, field, input, replacement);
    BinaryWriter *out = NewWriter(state);
    if (field->type <= FIELD_BOOL) {
        Binary_Write(out, in.data, in.size);
        if (!out->failed && !replacement->failed)
            for (int bit = 0; bit < field->bits; bit++) {
                size_t position = startBit + bit;
                uint8_t mask = 1u << (position % 8);
                out->data[position / 8] =
                    (out->data[position / 8] & ~mask) |
                    (((replacement->data[bit / 8] >> (bit % 8)) & 1) ? mask : 0);
            }
    } else {
        Binary_Write(out, in.data, start);
        Binary_Write(out, replacement->data, replacement->size);
        Binary_Write(out, in.data + in.offset, in.size - in.offset);
    }
    if (out->failed || replacement->failed || out->size > 65535)
        return luaL_error(state, "metadata exceeds the size limit or memory is exhausted");
    Metadata_Free(value);
    value->version = schema->version;
    bool defaults =
        out->size == schema->defaults.size && !memcmp(out->data, schema->defaults.data, out->size);
    if (!defaults) {
        value->data = out->data;
        value->size = out->size;
        out->data = NULL;
    }
    free(out->data);
    out->data = NULL;
    free(replacement->data);
    replacement->data = NULL;
    lua_pop(state, 2);
    return 0;
}

static Chunk *ResolveBlock(lua_State *state, int *index) {
    Vector3 *position = luaL_checkudata(state, 1, BLOCK_OBJECT);
    Vector3 chunkPosition = {floorf(position->x / 16), floorf(position->y / 16),
                             floorf(position->z / 16)};
    Chunk *chunk = ServerWorld_GetChunkAt(chunkPosition);
    if (!chunk)
        luaL_error(state, "block chunk is not loaded");
    *index = ChunkData_PositionToIndex((int)(position->x - chunkPosition.x * 16),
                                       (int)(position->y - chunkPosition.y * 16),
                                       (int)(position->z - chunkPosition.z * 16));
    if (chunk->data[*index] >= 256)
        luaL_error(state, "block type has no metadata schema");
    return chunk;
}
static int BlockGet(lua_State *state) {
    int index;
    Chunk *chunk = ResolveBlock(state, &index);
    Metadata empty = {0}, *value = ChunkMetadata_Get(chunk, index);
    return LuaMetadata_Get(state, serverBlockSchemas[chunk->data[index]], value ? value : &empty, 2);
}
static int BlockWrite(lua_State *state, bool reset) {
    int index;
    Chunk *chunk = ResolveBlock(state, &index);
    chunk->states[index] = ServerBlockStates_Resolve(chunk, index);
    Metadata *existing = ChunkMetadata_Get(chunk, index);
    Metadata *value = existing;
    if (!value) {
        value = lua_newuserdata(state, sizeof(*value));
        *value = (Metadata){0};
        luaL_setmetatable(state, "midless.MetadataValue");
    }
    // The setter commits only after encoding succeeds, so existing data needs no backup.
    LuaMetadata_Set(state, serverBlockSchemas[chunk->data[index]], value, 2, reset ? 0 : 3);
    if (existing) {
        if (!value->size)
            ChunkMetadata_Clear(chunk, index);
    } else {
        if (!ChunkMetadata_Set(chunk, index, value))
            return luaL_error(state, "out of memory");
        Metadata_Free(value);
    }
    ServerBlockStates_Changed(chunk, index);
    const MetadataField *field = FindField(state, GetSchema(state, serverBlockSchemas[chunk->data[index]]), 2);
    if (field->type == FIELD_INVENTORY)
        ScriptHooks_MetadataInventoryChanged(LuaMetadata_CheckBlock(state, 1), field->name);
    return 0;
}
static int BlockStartTimer(lua_State *state) {
    int index;
    Chunk *chunk = ResolveBlock(state, &index);
    double interval = luaL_checknumber(state, 2);
    if (!isfinite(interval) || interval < 0.05 || interval > 86400)
        return luaL_error(state, "timer interval must be between 0.05 and 86400 seconds");
    if (!BlockTimer_Start(chunk, index, interval))
        return luaL_error(state, "out of memory");
    return 0;
}
static int BlockStopTimer(lua_State *state) {
    int index;
    Chunk *chunk = ResolveBlock(state, &index);
    BlockTimer_Stop(chunk, index);
    return 0;
}
static int BlockTimerStarted(lua_State *state) {
    int index;
    Chunk *chunk = ResolveBlock(state, &index);
    bool started = false;
    for (int i = 0; i < chunk->timerCount; i++)
        if (chunk->timers[i].index == index)
            started = true;
    lua_pushboolean(state, started);
    return 1;
}
static int BlockSet(lua_State *state) {
    return BlockWrite(state, false);
}
static int BlockReset(lua_State *state) {
    return BlockWrite(state, true);
}
static int BlockId(lua_State *state) {
    int index;
    Chunk *chunk = ResolveBlock(state, &index);
    LuaItems_PushId(state, chunk->data[index]);
    return 1;
}
static int BlockPosition(lua_State *state) {
    Vector3 *position = luaL_checkudata(state, 1, BLOCK_OBJECT);
    lua_createtable(state, 0, 3);
    lua_pushinteger(state, position->x);
    lua_setfield(state, -2, "x");
    lua_pushinteger(state, position->y);
    lua_setfield(state, -2, "y");
    lua_pushinteger(state, position->z);
    lua_setfield(state, -2, "z");
    return 1;
}
static int BlockLoaded(lua_State *state) {
    Vector3 *position = luaL_checkudata(state, 1, BLOCK_OBJECT);
    Vector3 chunkPosition = {floorf(position->x / 16), floorf(position->y / 16),
                             floorf(position->z / 16)};
    lua_pushboolean(state, ServerWorld_GetChunkAt(chunkPosition) != NULL);
    return 1;
}
static int BlockSetId(lua_State *state) {
    int index;
    ResolveBlock(state, &index);
    int id = LuaItems_Id(state, 2, true, false);
    if (!ServerWorld_IsBlockDefined(id))
        return luaL_error(state, "block ID is not defined");
    Vector3 *position = luaL_checkudata(state, 1, BLOCK_OBJECT);
    ServerWorld_SetBlock(*position, id, true, false, true);
    return 0;
}

// Inventory references retain the safe owner handle, not a pointer into a chunk or entity.
static void PushInventory(lua_State *state, int owner, const char *field) {
    owner = lua_absindex(state, owner);
    lua_newuserdata(state, 1);
    lua_newtable(state);
    lua_pushvalue(state, owner);
    lua_setfield(state, -2, "owner");
    lua_pushstring(state, field);
    lua_setfield(state, -2, "field");
    lua_setuservalue(state, -2);
    luaL_setmetatable(state, INVENTORY_REF);
}
int LuaMetadata_Inventory(lua_State *state, int schema, int owner, int key) {
    const MetadataField *field = FindField(state, GetSchema(state, schema), key);
    if (field->type != FIELD_INVENTORY)
        return luaL_error(state, "field is not an inventory");
    PushInventory(state, owner, field->name);
    lua_getuservalue(state, -1);
    lua_pushinteger(state, field->limit);
    lua_setfield(state, -2, "slots");
    lua_pop(state, 1);
    return 1;
}
static int BlockInventory(lua_State *state) {
    int index;
    Chunk *chunk = ResolveBlock(state, &index);
    return LuaMetadata_Inventory(state, serverBlockSchemas[chunk->data[index]], 1, 2);
}
// All takes and gives affect one inventory, so encoding is a single commit.
static int BlockTransaction(lua_State *state) {
    int index;
    Chunk *chunk = ResolveBlock(state, &index);
    const char *name = luaL_checkstring(state, 2);
    const MetadataField *field = FindField(state, GetSchema(state, serverBlockSchemas[chunk->data[index]]), 2);
    if (field->type != FIELD_INVENTORY)
        return luaL_error(state, "field is not an inventory");
    luaL_checktype(state, 3, LUA_TTABLE);
    ItemStack slots[255] = {0};
    Vector3 position = LuaMetadata_CheckBlock(state, 1);
    if (!ServerMetadata_BlockInventory(position, name, slots, field->limit, false))
        return luaL_error(state, "cannot read inventory");
    const char *operations[] = {"take", "give"};
    for (int op = 0; op < 2; op++) {
        lua_getfield(state, 3, operations[op]);
        if (!lua_isnil(state, -1)) {
            luaL_checktype(state, -1, LUA_TTABLE);
            int list = lua_gettop(state);
            for (int i = 1; i <= lua_rawlen(state, list); i++) {
                lua_rawgeti(state, list, i);
                luaL_checktype(state, -1, LUA_TTABLE);
                lua_getfield(state, -1, "slot");
                int slot = Integer(state, -1, 1, field->limit) - 1;
                lua_pop(state, 1);
                ItemStack requested;
                LuaItems_ReadStack(state, -1, &requested);
                int id = requested.itemId, count = requested.count;
                lua_getfield(state, -1, "metadata");
                bool exact = !lua_isnil(state, -1);
                lua_pop(state, 1);
                ItemStack *stack = &slots[slot];
                bool fits = op == 0 ? stack->itemId == id && stack->count >= count &&
                                          (!exact || ItemStack_Matches(*stack, requested))
                                    : (!stack->count || ItemStack_Matches(*stack, requested)) &&
                                          stack->count + count <= Item_GetMaxStack(id);
                if (!fits) {
                    lua_pushboolean(state, false);
                    return 1;
                }
                if (op && !stack->count) {
                    *stack = requested;
                    stack->count = 0;
                }
                stack->count += op == 0 ? -count : count;
                if (!stack->count)
                    *stack = (ItemStack){0};
                lua_pop(state, 1);
            }
        }
        lua_pop(state, 1);
    }
    lua_pushboolean(state,
                    ServerMetadata_BlockInventory(position, name, slots, field->limit, true));
    return 1;
}
static int InventorySlot(lua_State *state) {
    luaL_checkudata(state, 1, INVENTORY_REF);
    lua_getuservalue(state, 1);
    lua_getfield(state, -1, "slots");
    int slots = lua_tointeger(state, -1);
    lua_pop(state, 2);
    return Integer(state, 2, 1, slots);
}
static int InventoryOwner(lua_State *state) {
    lua_getuservalue(state, 1);
    lua_getfield(state, -1, "owner");
    lua_replace(state, 1);
    lua_getfield(state, -1, "field");
    return lua_gettop(state);
}
static int OwnerGet(lua_State *state, int key) {
    if (luaL_testudata(state, 1, BLOCK_OBJECT)) {
        int index;
        Chunk *chunk = ResolveBlock(state, &index);
        Metadata empty = {0}, *value = ChunkMetadata_Get(chunk, index);
        return LuaMetadata_Get(state, serverBlockSchemas[chunk->data[index]], value ? value : &empty,
                               key);
    }
    Entity *entity = LuaEntities_Check(state, 1);
    return LuaMetadata_Get(state, ServerEntities_MetadataSchema(entity->definitionId),
                           &entity->metadata, key);
}
static int InventoryGet(lua_State *state) {
    int slot = InventorySlot(state);
    int key = InventoryOwner(state);
    OwnerGet(state, key);
    lua_rawgeti(state, -1, slot);
    return 1;
}
static int InventorySet(lua_State *state) {
    int slot = InventorySlot(state);
    lua_settop(state, 3); // A missing stack means an empty slot.
    int key = InventoryOwner(state);
    OwnerGet(state, key);
    lua_pushvalue(state, 3);
    lua_rawseti(state, -2, slot);
    int values = lua_gettop(state);
    if (luaL_testudata(state, 1, BLOCK_OBJECT)) {
        lua_pushvalue(state, key);
        lua_replace(state, 2);
        lua_pushvalue(state, values);
        lua_replace(state, 3);
        return BlockWrite(state, false);
    }
    Entity *entity = LuaEntities_Check(state, 1);
    return LuaMetadata_Set(state, ServerEntities_MetadataSchema(entity->definitionId),
                           &entity->metadata, key, values);
}
static int InventoryAdd(lua_State *state) {
    luaL_checkudata(state, 1, INVENTORY_REF);
    luaL_checktype(state, 2, LUA_TTABLE);
    lua_getfield(state, 2, "id");
    int id = LuaItems_Id(state, -1, false, false);
    lua_pop(state, 1);
    if (!id)
        return luaL_error(state, "air is not an item");
    lua_getfield(state, 2, "count");
    int count = Integer(state, -1, 1, INT_MAX);
    lua_pop(state, 1);
    lua_getuservalue(state, 1);
    lua_getfield(state, -1, "slots");
    int slots = lua_tointeger(state, -1);
    lua_pop(state, 2);
    int key = InventoryOwner(state);
    OwnerGet(state, key);
    int values = lua_gettop(state);
    ItemStack items[255] = {0};
    for (int i = 0; i < slots; i++) {
        lua_rawgeti(state, values, i + 1);
        if (!lua_isnil(state, -1)) {
            LuaItems_ReadStack(state, -1, &items[i]);
        }
        lua_pop(state, 1);
    }
    ItemStack added = {.itemId = id};
    LuaMetadata_ReadItem(state, 2, &added);
    if (Inventory_AddStackToSlots(items, slots, 0, added, count) != count) {
        lua_pushboolean(state, false);
        return 1;
    }
    for (int i = 0; i < slots; i++)
        if (items[i].count) {
            LuaItems_PushStack(state, items[i]);
            lua_rawseti(state, values, i + 1);
        }
    if (luaL_testudata(state, 1, BLOCK_OBJECT)) {
        lua_pushvalue(state, key);
        lua_replace(state, 2);
        lua_pushvalue(state, values);
        lua_replace(state, 3);
        BlockWrite(state, false);
    } else {
        Entity *entity = LuaEntities_Check(state, 1);
        LuaMetadata_Set(state, ServerEntities_MetadataSchema(entity->definitionId),
                        &entity->metadata, key, values);
    }
    lua_pushboolean(state, true);
    return 1;
}
int LuaMetadata_GetBlock(void) {
    luaL_checktype(L, 1, LUA_TTABLE);
    Vector3 position;
    float *coordinates[] = {&position.x, &position.y, &position.z};
    const char *names[] = {"x", "y", "z"};
    for (int i = 0; i < 3; i++) {
        lua_getfield(L, 1, names[i]);
        double value = luaL_checknumber(L, -1);
        lua_pop(L, 1);
        if (!isfinite(value) || fabs(value) > 33554430)
            return luaL_error(L, "block coordinate out of range");
        *coordinates[i] = floor(value);
    }
    LuaMetadata_PushBlock(L, position);
    return 1;
}
void LuaMetadata_PushBlock(lua_State *state, Vector3 position) {
    Vector3 *handle = lua_newuserdata(state, sizeof(*handle));
    *handle = position;
    luaL_setmetatable(state, BLOCK_OBJECT);
}
Vector3 LuaMetadata_CheckBlock(lua_State *state, int index) {
    return *(Vector3 *)luaL_checkudata(state, index, BLOCK_OBJECT);
}
int LuaMetadata_CheckBlockInventory(lua_State *state, int index, Vector3 position, char field[65]) {
    luaL_checkudata(state, index, INVENTORY_REF);
    lua_getuservalue(state, index);
    lua_getfield(state, -1, "owner");
    Vector3 owner = LuaMetadata_CheckBlock(state, -1);
    if (owner.x != position.x || owner.y != position.y || owner.z != position.z)
        return luaL_error(state, "inventory must belong to the screen's block");
    lua_pop(state, 1);
    lua_getfield(state, -1, "field");
    snprintf(field, 65, "%s", luaL_checkstring(state, -1));
    lua_pop(state, 1);
    lua_getfield(state, -1, "slots");
    int count = lua_tointeger(state, -1);
    lua_pop(state, 2);
    return count;
}
bool ScriptHooks_MetadataCanInsert(Vector3 position, const char *field, int slot, int item) {
    int block = ServerWorld_GetBlock(position);
    int schema = block >= 0 && block < 256 ? serverBlockSchemas[block] : -1;
    if (schema < 0)
        return false;
    int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ruleReferences[schema]);
    lua_getfield(L, -1, field);
    bool allowed = true;
    if (lua_istable(L, -1)) {
        lua_rawgeti(L, -1, slot + 1);
        if (lua_isboolean(L, -1))
            allowed = lua_toboolean(L, -1);
        else if (lua_istable(L, -1)) {
            lua_rawgeti(L, -1, item);
            allowed = lua_toboolean(L, -1);
        }
    }
    lua_settop(L, top);
    return allowed;
}
bool ScriptHooks_MetadataTimer(Vector3 position, float dt) {
    int id = ServerWorld_GetBlock(position);
    if (!L || id < 0 || id >= 256 || timerCallbacks[id] < 0)
        return false;
    int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, timerCallbacks[id]);
    LuaMetadata_PushBlock(L, position);
    lua_pushnumber(L, dt);
    bool ok = lua_pcall(L, 2, 1, 0) == LUA_OK;
    if (!ok)
        TraceLog(LOG_WARNING, "Block on_timer: %s", lua_tostring(L, -1));
    bool repeat = ok && lua_toboolean(L, -1);
    lua_settop(L, top);
    return repeat;
}
void ScriptHooks_MetadataInventoryChanged(Vector3 position, const char *field) {
    int block = ServerWorld_GetBlock(position);
    if (notifying || block < 0 || block >= 256 || inventoryCallbacks[block] < 0)
        return;
    for (int i = 0; i < changeCount; i++)
        if (changes[i].position.x == position.x && changes[i].position.y == position.y &&
            changes[i].position.z == position.z && !strcmp(changes[i].field, field))
            return;
    if (changeCount == changeCapacity) {
        int capacity = changeCapacity ? changeCapacity * 2 : 16;
        InventoryChange *next = realloc(changes, capacity * sizeof(*next));
        if (!next) {
            TraceLog(LOG_ERROR, "Could not queue inventory change");
            return;
        }
        changes = next;
        changeCapacity = capacity;
    }
    changes[changeCount] = (InventoryChange){.position = position, .block = block};
    strcpy(changes[changeCount++].field, field);
}
void ScriptHooks_MetadataFlushChanges(void) {
    notifying = true;
    for (int i = 0; i < changeCount; i++) {
        InventoryChange *change = &changes[i];
        if (ServerWorld_GetBlock(change->position) != change->block)
            continue;
        int top = lua_gettop(L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, inventoryCallbacks[change->block]);
        LuaMetadata_PushBlock(L, change->position);
        lua_pushstring(L, change->field);
        if (lua_pcall(L, 2, 0, 0) != LUA_OK)
            TraceLog(LOG_WARNING, "Block on_inventory_changed: %s", lua_tostring(L, -1));
        lua_settop(L, top);
    }
    changeCount = 0;
    notifying = false;
}
void LuaMetadata_DefineItem(int id, int definition) {
    int schema = LuaMetadata_Register(L, definition);
    if (schema >= 0) {
        for (int i = 0; i < serverMetadataSchemas[schema]->count; i++)
            if (serverMetadataSchemas[schema]->fields[i].type == FIELD_INVENTORY)
                luaL_error(L, "item metadata cannot contain an inventory");
        if (serverMetadataSchemas[schema]->defaults.size > ITEM_METADATA_BYTES)
            luaL_error(L, "item metadata supports up to 64 encoded bytes");
    }
    serverItemSchemas[id] = schema;
}
void LuaMetadata_ItemBar(int id, int definition, ItemBar *bar) {
    *bar = (ItemBar){0};
    lua_getfield(L, definition, "inventory_bar");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    luaL_checktype(L, -1, LUA_TTABLE);
    int table = lua_gettop(L);
    lua_getfield(L, table, "field");
    size_t length;
    const char *name = luaL_checklstring(L, -1, &length);
    if (!length || length > 64 || memchr(name, 0, length))
        luaL_error(L, "invalid inventory_bar field");
    int schema = serverItemSchemas[id], target = -1;
    if (schema >= 0)
        for (int i = 0; i < serverMetadataSchemas[schema]->count; i++)
            if (!strcmp(name, serverMetadataSchemas[schema]->fields[i].name))
                target = i;
    if (target < 0 || serverMetadataSchemas[schema]->fields[target].type == FIELD_BOOL ||
        serverMetadataSchemas[schema]->fields[target].type > FIELD_FLOAT)
        luaL_error(L, "inventory_bar.field must reference numeric item metadata");
    lua_pop(L, 1);
    lua_getfield(L, table, "max");
    double maximum = luaL_checknumber(L, -1);
    if (!isfinite(maximum) || !isfinite((float)maximum) || (float)maximum <= 0)
        luaL_error(L, "inventory_bar.max must be finite and positive");
    bar->maximum = maximum;
    lua_pop(L, 1);
    lua_getfield(L, table, "hide_when_full");
    if (!lua_isnil(L, -1))
        luaL_checktype(L, -1, LUA_TBOOLEAN);
    bar->hideWhenFull = lua_isnil(L, -1) || lua_toboolean(L, -1);
    lua_pop(L, 2);
    MetadataSchema *layout = serverMetadataSchemas[schema];
    bar->version = layout->version;
    bar->count = target + 1;
    bar->defaultValue = layout->fields[target].defaultNumber;
    for (int i = 0; i <= target; i++) {
        MetadataField *field = &layout->fields[i];
        bar->fields[i] = field->type <= FIELD_BOOL
                             ? field->bits + (field->type == FIELD_INT ? 32 : 0)
                         : field->type == FIELD_FLOAT ? 65
                                                      : 66;
    }
}
void LuaMetadata_ReadItem(lua_State *state, int index, ItemStack *stack) {
    index = lua_absindex(state, index);
    int schema = stack->itemId < ITEM_LIMIT ? serverItemSchemas[stack->itemId] : -1;
    lua_getfield(state, index, "_metadata_version");
    int version = lua_tointeger(state, -1);
    lua_pop(state, 1);
    if (schema < 0 || (version && version != serverMetadataSchemas[schema]->version)) {
        lua_getfield(state, index, "_metadata");
        if (!lua_isnil(state, -1)) {
            size_t size;
            const char *raw = luaL_checklstring(state, -1, &size);
            if (size > ITEM_METADATA_BYTES || version < 1 || version > 65535)
                luaL_error(state, "invalid opaque item metadata");
            memcpy(stack->metadata, raw, size);
            stack->metadataSize = size;
            stack->metadataVersion = version;
        }
        lua_pop(state, 1);
        if (!stack->metadataSize) {
            lua_getfield(state, index, "metadata");
            if (lua_istable(state, -1)) {
                lua_pushnil(state);
                if (lua_next(state, -2))
                    luaL_error(state, "item has no metadata schema");
            } else if (!lua_isnil(state, -1))
                luaL_error(state, "metadata must be a table");
            lua_pop(state, 1);
        }
        return;
    }
    lua_getfield(state, index, "metadata");
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        return;
    }
    luaL_checktype(state, -1, LUA_TTABLE);
    int input = lua_gettop(state);
    Metadata *value = lua_newuserdata(state, sizeof(*value));
    *value = (Metadata){0};
    luaL_setmetatable(state, "midless.MetadataValue");
    lua_pushnil(state);
    while (lua_next(state, input)) {
        LuaMetadata_Set(state, schema, value, -2, -1);
        lua_pop(state, 1);
    }
    if (value->size > ITEM_METADATA_BYTES)
        luaL_error(state, "item metadata supports up to 64 encoded bytes");
    stack->metadataSize = value->size;
    stack->metadataVersion = value->version;
    if (value->size)
        memcpy(stack->metadata, value->data, value->size);
    Metadata_Free(value);
    lua_pop(state, 2);
}
void LuaMetadata_PushItem(lua_State *state, ItemStack stack) {
    lua_newtable(state);
    int schema = stack.itemId < ITEM_LIMIT ? serverItemSchemas[stack.itemId] : -1;
    if (schema < 0 || (stack.metadataSize && stack.metadataVersion != serverMetadataSchemas[schema]->version))
        return;
    Metadata value = {stack.metadata, stack.metadataSize, stack.metadataVersion};
    for (int i = 0; i < serverMetadataSchemas[schema]->count; i++) {
        const char *name = serverMetadataSchemas[schema]->fields[i].name;
        lua_pushstring(state, name);
        LuaMetadata_Get(state, schema, &value, -1);
        lua_setfield(state, -3, name);
        lua_pop(state, 1);
    }
}
static int FreeWriter(lua_State *state) {
    BinaryWriter *out = lua_touserdata(state, 1);
    free(out->data);
    out->data = NULL;
    return 0;
}
static int FreeValue(lua_State *state) {
    Metadata_Free(lua_touserdata(state, 1));
    return 0;
}
static void Metatable(const char *name, const luaL_Reg *methods) {
    luaL_newmetatable(L, name);
    lua_newtable(L);
    luaL_setfuncs(L, methods, 0);
    lua_setfield(L, -2, "__index");
    lua_pushstring(L, name);
    lua_setfield(L, -2, "__metatable");
    lua_pop(L, 1);
}
void LuaMetadata_Init(void) {
    playerSchemaCount = 0;
    playerListenerCount = playerChangeCount = 0;
    notifyingPlayers = false;
    lua_pushcfunction(L, LuaMetadata_DefinePlayer);
    lua_pushliteral(L, "midless");
    lua_newtable(L);
    lua_newtable(L);
    lua_pushliteral(L, "hp");
    lua_setfield(L, -2, "name");
    lua_pushliteral(L, "uint");
    lua_setfield(L, -2, "type");
    lua_pushinteger(L, 16);
    lua_setfield(L, -2, "bits");
    lua_pushinteger(L, 20);
    lua_setfield(L, -2, "default");
    lua_rawseti(L, -2, 1);
    lua_call(L, 2, 0);
    for (int i = 0; i < ITEM_LIMIT; i++)
        serverItemSchemas[i] = -1;
    changeCount = 0;
    notifying = false;
    for (int i = 0; i < 256; i++) {
        serverBlockSchemas[i] = -1;
        timerCallbacks[i] = inventoryCallbacks[i] = LUA_NOREF;
    }
    const luaL_Reg block[] = {{"get_id", BlockId},
                              {"set_id", BlockSetId},
                              {"get_position", BlockPosition},
                              {"is_loaded", BlockLoaded},
                              {"get_metadata", BlockGet},
                              {"set_metadata", BlockSet},
                              {"reset_metadata", BlockReset},
                              {"get_inventory", BlockInventory},
                              {"start_timer", BlockStartTimer},
                              {"stop_timer", BlockStopTimer},
                              {"timer_started", BlockTimerStarted},
                              {"inventory_transaction", BlockTransaction},
                              {NULL, NULL}};
    const luaL_Reg inventory[] = {{"get_stack", InventoryGet},
                                  {"set_stack", InventorySet},
                                  {"add_item", InventoryAdd},
                                  {NULL, NULL}};
    Metatable(BLOCK_OBJECT, block);
    Metatable(INVENTORY_REF, inventory);
    luaL_newmetatable(L, "midless.MetadataWriter");
    lua_pushcfunction(L, FreeWriter);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);
    luaL_newmetatable(L, "midless.MetadataValue");
    lua_pushcfunction(L, FreeValue);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);
}
void LuaMetadata_Shutdown(void) {
    ServerBlockStates_Reset();
    for (int i = 0; i < playerListenerCount; i++)
        luaL_unref(L, LUA_REGISTRYINDEX, playerListeners[i].callback);
    playerListenerCount = 0;
    free(changes);
    changes = NULL;
    changeCount = changeCapacity = 0;
    for (int i = 0; i < 256; i++) {
        luaL_unref(L, LUA_REGISTRYINDEX, timerCallbacks[i]);
        luaL_unref(L, LUA_REGISTRYINDEX, inventoryCallbacks[i]);
    }
    for (int i = 0; i < serverMetadataSchemaCount; i++) {
        luaL_unref(L, LUA_REGISTRYINDEX, ruleReferences[i]);
    }
    ServerMetadata_Reset();
}

int LuaMetadata_DefinePlayer(lua_State *state) {
    size_t length;
    const char *name = luaL_checklstring(state, 1, &length);
    if (!length || length > 64)
        return luaL_error(state, "invalid player metadata namespace");
    for (size_t i = 0; i < length; i++)
        if (!((name[i] >= 'a' && name[i] <= 'z') || (name[i] >= '0' && name[i] <= '9') ||
              name[i] == '_' || name[i] == '-'))
            return luaL_error(
                state,
                "metadata namespace must use lowercase letters, digits, underscores or hyphens");
    for (int i = 0; i < playerSchemaCount; i++)
        if (!strcmp(playerSchemas[i].name, name))
            return luaL_error(state, "player metadata namespace is already registered");
    if (playerSchemaCount == PLAYER_METADATA_GROUPS)
        return luaL_error(state, "too many player metadata namespaces");
    luaL_checktype(state, 2, LUA_TTABLE);
    // Validate using the same field rules as block and entity metadata.
    MetadataSchema layout;
    ReadSchema(state, 2, 1, &layout);
    for (int i = 0; i < layout.count; i++)
        if (layout.fields[i].type == FIELD_INVENTORY)
            return luaL_error(state, "use player inventories instead of inventory metadata fields");
    int listenerCount = 0;
    for (int i = 0; i < layout.count; i++) {
        lua_rawgeti(state, 2, i + 1);
        lua_getfield(state, -1, "on_change");
        if (!lua_isnil(state, -1)) {
            luaL_checktype(state, -1, LUA_TFUNCTION);
            listenerCount++;
        }
        lua_pop(state, 2);
    }
    if (playerListenerCount + listenerCount > PLAYER_CHANGE_LIMIT)
        return luaL_error(state, "too many metadata listeners");
    lua_newtable(state);
    lua_pushvalue(state, 2);
    lua_setfield(state, -2, "metadata");
    int schema = LuaMetadata_Register(state, lua_gettop(state));
    lua_pop(state, 1);
    strcpy(playerSchemas[playerSchemaCount].name, name);
    playerSchemas[playerSchemaCount++].schema = schema;
    for (int i = 0; i < layout.count; i++) {
        lua_rawgeti(state, 2, i + 1);
        lua_getfield(state, -1, "on_change");
        if (!lua_isnil(state, -1)) {
            snprintf(playerListeners[playerListenerCount].key, 130, "%s:%s", name,
                     layout.fields[i].name);
            playerListeners[playerListenerCount++].callback = luaL_ref(state, LUA_REGISTRYINDEX);
        } else
            lua_pop(state, 1);
        lua_pop(state, 1);
    }
    return 0;
}
int LuaMetadata_Player(lua_State *state, Player *player, bool write, bool reset) {
    const char *key = luaL_checkstring(state, 2), *separator = strchr(key, ':');
    if (!separator || separator == key || !separator[1])
        return luaL_error(state, "expected namespace:field");
    size_t length = separator - key;
    int schema = -1;
    for (int i = 0; i < playerSchemaCount; i++)
        if (strlen(playerSchemas[i].name) == length &&
            !strncmp(playerSchemas[i].name, key, length)) {
            schema = playerSchemas[i].schema;
            break;
        }
    if (schema < 0)
        return luaL_error(state, "player metadata namespace is not registered");
    int index = 0;
    for (; index < player->metadataCount; index++)
        if (strlen(player->metadata[index].name) == length &&
            !strncmp(player->metadata[index].name, key, length))
            break;
    lua_pushstring(state, separator + 1);
    int field = lua_gettop(state);
    Metadata empty = {0};
    if (!write)
        return LuaMetadata_Get(
            state, schema, index < player->metadataCount ? &player->metadata[index].value : &empty,
            field);
    LuaMetadata_Get(state, schema,
                    index < player->metadataCount ? &player->metadata[index].value : &empty, field);
    int oldValue = lua_gettop(state);
    if (index == player->metadataCount) {
        if (index == PLAYER_METADATA_GROUPS)
            return luaL_error(state, "player metadata is full");
        // Reserve only after a successful write, so invalid values do not consume a namespace.
        LuaMetadata_Set(state, schema, &player->metadata[index].value, field, reset ? 0 : 3);
        memcpy(player->metadata[index].name, key, length);
        player->metadata[index].name[length] = 0;
        player->metadataCount++;
    } else
        LuaMetadata_Set(state, schema, &player->metadata[index].value, field, reset ? 0 : 3);
    LuaMetadata_Get(state, schema, &player->metadata[index].value, field);
    NotifyPlayer(player, key, oldValue, lua_gettop(state));
    return 0;
}

int LuaMetadata_RegisterPlayerChange(lua_State *state) {
    size_t length;
    const char *key = luaL_checklstring(state, 1, &length);
    const char *separator = strchr(key, ':');
    if (!length || length >= 130 || memchr(key, 0, length) || !separator || separator == key ||
        !separator[1])
        return luaL_error(state, "expected namespace:field");
    luaL_checktype(state, 2, LUA_TFUNCTION);
    if (playerListenerCount == PLAYER_CHANGE_LIMIT)
        return luaL_error(state, "too many metadata listeners");
    strcpy(playerListeners[playerListenerCount].key, key);
    lua_pushvalue(state, 2);
    playerListeners[playerListenerCount++].callback = luaL_ref(state, LUA_REGISTRYINDEX);
    return 0;
}
int LuaMetadata_RegisterHPChange(lua_State *state) {
    luaL_checktype(state, 1, LUA_TFUNCTION);
    lua_settop(state, 1);
    lua_pushliteral(state, "midless:hp");
    lua_insert(state, 1);
    return LuaMetadata_RegisterPlayerChange(state);
}
