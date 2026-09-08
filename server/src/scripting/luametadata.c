#include "luametadata.h"
#include "binarydata.h"
#include "luaentities.h"
#include "../world/world.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <limits.h>

extern lua_State *L;
#define MAX_SCHEMAS 512
#define MAX_FIELDS 64
#define BLOCK_OBJECT "midless.Block"
#define INVENTORY_REF "midless.MetadataInventory"

typedef enum FieldType { FIELD_UINT, FIELD_INT, FIELD_BOOL, FIELD_FLOAT, FIELD_STRING, FIELD_INVENTORY } FieldType;
typedef struct Field {
    char name[65];
    FieldType type;
    int bits, limit;
    double defaultNumber;
    char defaultString[257];
} Field;
typedef struct Schema {
    uint16_t version;
    int count;
    Field fields[MAX_FIELDS];
    Metadata defaults;
} Schema;
static bool BuildDefaults(Schema *schema);
static Schema *schemas[MAX_SCHEMAS];
static int schemaCount, blockSchemas[256];

static int Integer(lua_State *state, int index, int min, int max) {
    lua_Integer value = luaL_checkinteger(state, index);
    if (value < min || value > max) luaL_error(state, "integer is outside the schema range");
    return (int)value;
}
static int IntegerField(lua_State *state, int index, const char *name, int fallback, int min, int max) {
    lua_getfield(state, index, name);
    int value = lua_isnil(state, -1) ? fallback : Integer(state, -1, min, max);
    lua_pop(state, 1);
    return value;
}
static void ReadSchema(lua_State *state, int index, int version, Schema *layout) {
    index = lua_absindex(state, index);
    luaL_checktype(state, index, LUA_TTABLE);
    *layout = (Schema){.version = version};
    size_t count = lua_rawlen(state, index);
    if (count > MAX_FIELDS) luaL_error(state, "metadata supports at most 64 fields");
    layout->count = count;
    for (int i = 0; i < layout->count; i++) {
        lua_rawgeti(state, index, i + 1);
        luaL_checktype(state, -1, LUA_TTABLE);
        int entry = lua_gettop(state);
        Field *field = &layout->fields[i];
        lua_getfield(state, entry, "name");
        size_t length;
        const char *name = luaL_checklstring(state, -1, &length);
        if (!length || length > 64 || memchr(name, 0, length)) luaL_error(state, "invalid metadata field name");
        memcpy(field->name, name, length + 1);
        for (int j = 0; j < i; j++) if (!strcmp(layout->fields[j].name, name)) luaL_error(state, "duplicate metadata field");
        lua_pop(state, 1);
        lua_getfield(state, entry, "type");
        const char *type = luaL_checkstring(state, -1);
        const char *types[] = {"uint", "int", "bool", "float", "string", "inventory"};
        int kind = 0;
        while (kind < 6 && strcmp(type, types[kind])) kind++;
        if (kind == 6) luaL_error(state, "unknown metadata field type");
        field->type = kind;
        lua_pop(state, 1);
        field->bits = kind == FIELD_BOOL ? 1 : IntegerField(state, entry, "bits", 16, 1, 32);
        field->limit = kind == FIELD_INVENTORY ? IntegerField(state, entry, "slots", 27, 1, 255) :
            IntegerField(state, entry, "max_length", 256, 0, 4096);
        lua_getfield(state, entry, "default");
        if (!lua_isnil(state, -1)) {
            if (kind == FIELD_STRING) {
                const char *value = luaL_checklstring(state, -1, &length);
                if (length > 256 || length > (size_t)field->limit || memchr(value, 0, length)) luaL_error(state, "invalid string default");
                memcpy(field->defaultString, value, length + 1);
            } else if (kind == FIELD_BOOL) {
                luaL_checktype(state, -1, LUA_TBOOLEAN);
                field->defaultNumber = lua_toboolean(state, -1);
            } else if (kind == FIELD_INVENTORY) {
                luaL_error(state, "inventory default is always empty");
            } else {
                double value = luaL_checknumber(state, -1);
                double min = kind == FIELD_INT ? -ldexp(1, field->bits - 1) : 0;
                double max = kind == FIELD_INT ? ldexp(1, field->bits - 1) - 1 : ldexp(1, field->bits) - 1;
                if (!isfinite(value) || (kind != FIELD_FLOAT && (value != floor(value) || value < min || value > max)) ||
                    (kind == FIELD_FLOAT && !isfinite((float)value))) luaL_error(state, "invalid metadata default");
                field->defaultNumber = kind == FIELD_FLOAT ? (float)value : value;
            }
        }
        lua_pop(state, 2);
    }
}

int LuaMetadata_Register(lua_State *state, int definition) {
    definition = lua_absindex(state, definition);
    lua_getfield(state, definition, "metadata");
    if (lua_isnil(state, -1)) { lua_pop(state, 1); return -1; }
    if (schemaCount == MAX_SCHEMAS) return luaL_error(state, "metadata schema registry is full");
    Schema layout;
    int version = IntegerField(state, definition, "metadata_version", 1, 1, 65535);
    ReadSchema(state, -1, version, &layout);
    lua_pop(state, 1);
    Schema *schema = calloc(1, sizeof(*schema));
    if (!schema) return luaL_error(state, "out of memory");
    *schema = layout;
    if (!BuildDefaults(schema)) {
        free(schema);
        return luaL_error(state, "metadata defaults exceed the size limit or memory is exhausted");
    }
    schemas[schemaCount] = schema;
    return schemaCount++;
}
void LuaMetadata_DefineBlock(int blockId, int definition) {
    if (blockSchemas[blockId] >= 0) luaL_error(L, "block metadata schema is already registered");
    blockSchemas[blockId] = LuaMetadata_Register(L, definition);
}

// Numeric fields share bytes. Variable-size fields start at the next byte.
typedef struct Bits { uint8_t byte; int count; } Bits;
static void WriteBits(BinaryWriter *out, Bits *bits, uint32_t value, int count) {
    for (int i = 0; i < count; i++) {
        bits->byte |= ((value >> i) & 1) << bits->count;
        if (++bits->count == 8) { Binary_U8(out, bits->byte); *bits = (Bits){0}; }
    }
}
static void FlushBits(BinaryWriter *out, Bits *bits) {
    if (bits->count) Binary_U8(out, bits->byte);
    *bits = (Bits){0};
}
static uint32_t ReadBits(BinaryReader *in, Bits *bits, int count) {
    uint32_t value = 0;
    for (int i = 0; i < count; i++) {
        if (!bits->count) { bits->byte = Binary_ReadU8(in); bits->count = 8; }
        value |= (uint32_t)(bits->byte & 1) << i;
        bits->byte >>= 1; bits->count--;
    }
    return value;
}
static void PushDefault(lua_State *state, const Field *field) {
    if (field->type == FIELD_STRING) lua_pushstring(state, field->defaultString);
    else if (field->type == FIELD_INVENTORY) lua_newtable(state);
    else if (field->type == FIELD_BOOL) lua_pushboolean(state, field->defaultNumber != 0);
    else lua_pushnumber(state, field->defaultNumber);
}
// Read just one field. Validation uses the same reader without creating Lua values.
static bool ReadField(lua_State *state, const Field *field, BinaryReader *in, Bits *bits) {
    if (field->type <= FIELD_BOOL) {
        uint32_t number = ReadBits(in, bits, field->bits);
        if (state) {
            if (field->type == FIELD_BOOL) lua_pushboolean(state, number);
            else if (field->type == FIELD_INT) {
                int64_t signedValue = number;
                if (number & (1u << (field->bits - 1))) signedValue -= (int64_t)1 << field->bits;
                lua_pushinteger(state, signedValue);
            } else lua_pushinteger(state, number);
        }
    } else {
        if (bits->byte) return false;
        *bits = (Bits){0};
        if (field->type == FIELD_FLOAT) {
            float number = Binary_ReadFloat(in);
            if (!isfinite(number)) return false;
            if (state) lua_pushnumber(state, number);
        } else if (field->type == FIELD_STRING) {
            uint32_t size = Binary_ReadVarUInt(in);
            const uint8_t *text = Binary_Read(in, size);
            if (in->failed || size > (unsigned)field->limit) return false;
            if (state) lua_pushlstring(state, (const char *)text, size);
        } else {
            int count = Binary_ReadU8(in), previous = -1;
            if (count > field->limit) return false;
            if (state) lua_newtable(state);
            for (int slot = 0; slot < count; slot++) {
                int index = Binary_ReadU8(in);
                int id = Binary_ReadU16(in), amount = Binary_ReadU8(in);
                if (index <= previous || index >= field->limit || !id || !amount || amount > Item_GetMaxStack(id)) return false;
                previous = index;
                if (state) {
                    lua_newtable(state);
                    lua_pushinteger(state, id); lua_setfield(state, -2, "id");
                    lua_pushinteger(state, amount); lua_setfield(state, -2, "count");
                    lua_rawseti(state, -2, index + 1);
                }
            }
        }
    }
    return !in->failed;
}
static void WriteDefault(BinaryWriter *out, Bits *bits, const Field *field) {
    if (field->type <= FIELD_BOOL) WriteBits(out, bits, (uint32_t)(int64_t)field->defaultNumber, field->bits);
    else {
        FlushBits(out, bits);
        if (field->type == FIELD_FLOAT) Binary_Float(out, field->defaultNumber);
        else if (field->type == FIELD_STRING) {
            size_t size = strlen(field->defaultString);
            Binary_VarUInt(out, size); Binary_Write(out, field->defaultString, size);
        } else Binary_U8(out, 0);
    }
}
static bool BuildDefaults(Schema *schema) {
    BinaryWriter out = {0};
    Bits bits = {0};
    for (int i = 0; i < schema->count; i++) WriteDefault(&out, &bits, &schema->fields[i]);
    FlushBits(&out, &bits);
    if (out.failed || out.size > 65535) { free(out.data); return false; }
    schema->defaults = (Metadata){out.data, out.size, schema->version};
    return true;
}
static void WriteField(lua_State *state, const Field *field, int input, BinaryWriter *out) {
    if (!input || lua_isnil(state, input)) {
        Bits bits = {0}; WriteDefault(out, &bits, field); FlushBits(out, &bits);
        return;
    }
    if (field->type <= FIELD_BOOL) {
        double number;
        if (field->type == FIELD_BOOL) {
            luaL_checktype(state, input, LUA_TBOOLEAN);
            number = lua_toboolean(state, input);
        } else number = luaL_checknumber(state, input);
        double min = field->type == FIELD_INT ? -ldexp(1, field->bits - 1) : 0;
        double max = field->type == FIELD_INT ? ldexp(1, field->bits - 1) - 1 : ldexp(1, field->bits) - 1;
        if (!isfinite(number) || number != floor(number) || number < min || number > max)
            luaL_error(state, "metadata '%s' is out of range", field->name);
        Bits bits = {0}; WriteBits(out, &bits, (uint32_t)(int64_t)number, field->bits); FlushBits(out, &bits);
    } else if (field->type == FIELD_FLOAT) {
        float number = luaL_checknumber(state, input);
        if (!isfinite(number)) luaL_error(state, "metadata float must be finite");
        Binary_Float(out, number);
    } else if (field->type == FIELD_STRING) {
        size_t size;
        const char *text = luaL_checklstring(state, input, &size);
        if (size > (size_t)field->limit) luaL_error(state, "metadata string is too long");
        Binary_VarUInt(out, size); Binary_Write(out, text, size);
    } else {
        luaL_checktype(state, input, LUA_TTABLE);
        ItemStack stacks[255] = {0};
        int count = 0;
        lua_pushnil(state);
        while (lua_next(state, input)) {
            int slot = Integer(state, -2, 1, field->limit) - 1;
            luaL_checktype(state, -1, LUA_TTABLE);
            lua_getfield(state, -1, "id"); int id = Integer(state, -1, 1, 65535); lua_pop(state, 1);
            lua_getfield(state, -1, "count"); int amount = Integer(state, -1, 1, Item_GetMaxStack(id)); lua_pop(state, 1);
            stacks[slot] = (ItemStack){id, amount}; count++;
            lua_pop(state, 1);
        }
        Binary_U8(out, count);
        for (int slot = 0; slot < field->limit; slot++) if (stacks[slot].count) {
            Binary_U8(out, slot); Binary_U16(out, stacks[slot].itemId); Binary_U8(out, stacks[slot].count);
        }
    }
}
static Schema *GetSchema(lua_State *state, int id) {
    if (id < 0 || id >= schemaCount) luaL_error(state, "no metadata schema is registered");
    return schemas[id];
}
static const Field *FindField(lua_State *state, Schema *schema, int key) {
    const char *name = luaL_checkstring(state, key);
    for (int i = 0; i < schema->count; i++)
        if (!strcmp(name, schema->fields[i].name)) return &schema->fields[i];
    luaL_error(state, "unknown metadata field '%s'", name);
    return NULL;
}
bool LuaMetadata_CanRead(int id, const Metadata *value) {
    return !value->size || (id >= 0 && id < schemaCount && value->version == schemas[id]->version);
}
bool LuaMetadata_Validate(int id, const Metadata *value) {
    // Unknown layouts stay opaque. Known layouts are checked without invoking Lua.
    if (!value->size || !LuaMetadata_CanRead(id, value)) return true;
    Schema *schema = schemas[id];
    BinaryReader in = {value->data, value->size};
    Bits bits = {0};
    for (int i = 0; i < schema->count; i++)
        if (!ReadField(NULL, &schema->fields[i], &in, &bits)) return false;
    return Binary_End(&in) && !bits.byte;
}
bool LuaMetadata_ValidateChunk(Chunk *chunk) {
    for (int i = 0; i < chunk->metadataCount; i++) {
        BlockMetadata *record = &chunk->metadata[i];
        int type = chunk->data[record->index];
        if (type < 256 && !LuaMetadata_Validate(blockSchemas[type], &record->value)) return false;
    }
    return true;
}
int LuaMetadata_CollectBlockItems(Vector3 position, ItemStack *stacks, int capacity) {
    Vector3 chunkPosition = {floorf(position.x / 16), floorf(position.y / 16), floorf(position.z / 16)};
    Chunk *chunk = ServerWorld_GetChunkAt(chunkPosition);
    if (!chunk) return -1;
    int index = ChunkData_PositionToIndex((int)(position.x - chunkPosition.x * 16),
        (int)(position.y - chunkPosition.y * 16), (int)(position.z - chunkPosition.z * 16));
    Metadata *value = ChunkMetadata_Get(chunk, index);
    if (!value || !value->size) return 0; // Inventories default to empty.
    int id = chunk->data[index] < 256 ? blockSchemas[chunk->data[index]] : -1;
    if (!LuaMetadata_CanRead(id, value)) return -1;
    Schema *schema = schemas[id];
    BinaryReader in = {value->data, value->size};
    Bits bits = {0};
    int total = 0;
    for (int i = 0; i < schema->count; i++) {
        const Field *field = &schema->fields[i];
        BinaryReader inventory = in;
        if (!ReadField(NULL, field, &in, &bits)) return -1;
        if (field->type != FIELD_INVENTORY) continue;
        int count = Binary_ReadU8(&inventory);
        if (count > capacity - total) return -1;
        for (int slot = 0; slot < count; slot++) {
            Binary_ReadU8(&inventory);
            uint16_t itemId = Binary_ReadU16(&inventory);
            uint8_t amount = Binary_ReadU8(&inventory);
            stacks[total++] = (ItemStack){itemId, amount};
        }
    }
    return Binary_End(&in) && !bits.byte ? total : -1;
}
// Find a field's byte/bit location without decoding unrelated values or inventories.
static bool SkipField(const Field *field, BinaryReader *in, Bits *bits) {
    if (field->type <= FIELD_BOOL) ReadBits(in, bits, field->bits);
    else {
        *bits = (Bits){0};
        if (field->type == FIELD_FLOAT) Binary_Read(in, 4);
        else if (field->type == FIELD_STRING) {
            uint32_t length = Binary_ReadVarUInt(in); Binary_Read(in, length);
        } else {
            unsigned count = Binary_ReadU8(in); Binary_Read(in, count * 4);
        }
    }
    return !in->failed;
}
static BinaryReader Locate(lua_State *state, Schema *schema, const Metadata *value, const Field *field, Bits *bits) {
    if (value->size && value->version != schema->version) luaL_error(state, "metadata schema version does not match");
    const Metadata *source = value->size ? value : &schema->defaults;
    BinaryReader in = {source->data, source->size};
    for (const Field *previous = schema->fields; previous != field; previous++)
        if (!SkipField(previous, &in, bits)) luaL_error(state, "invalid metadata payload");
    if (field->type > FIELD_BOOL) *bits = (Bits){0};
    return in;
}
int LuaMetadata_Get(lua_State *state, int id, Metadata *value, int key) {
    Schema *schema = GetSchema(state, id);
    const Field *field = FindField(state, schema, key);
    if (!value->size) { PushDefault(state, field); return 1; }
    Bits bits = {0};
    BinaryReader in = Locate(state, schema, value, field, &bits);
    if (!ReadField(state, field, &in, &bits)) return luaL_error(state, "invalid metadata payload");
    return 1;
}
static BinaryWriter *NewWriter(lua_State *state) {
    BinaryWriter *out = lua_newuserdata(state, sizeof(*out));
    *out = (BinaryWriter){0};
    luaL_setmetatable(state, "midless.MetadataWriter");
    return out;
}
int LuaMetadata_Set(lua_State *state, int id, Metadata *value, int key, int input) {
    if (input) input = lua_absindex(state, input);
    Schema *schema = GetSchema(state, id);
    const Field *field = FindField(state, schema, key);
    Bits bits = {0};
    BinaryReader in = Locate(state, schema, value, field, &bits);
    size_t startBit = in.offset * 8 - bits.count;
    size_t start = in.offset;
    if (!SkipField(field, &in, &bits)) return luaL_error(state, "invalid metadata payload");
    BinaryWriter *replacement = NewWriter(state);
    WriteField(state, field, input, replacement);
    BinaryWriter *out = NewWriter(state);
    if (field->type <= FIELD_BOOL) {
        Binary_Write(out, in.data, in.size);
        if (!out->failed && !replacement->failed) for (int bit = 0; bit < field->bits; bit++) {
            size_t position = startBit + bit;
            uint8_t mask = 1u << (position % 8);
            out->data[position / 8] = (out->data[position / 8] & ~mask) |
                (((replacement->data[bit / 8] >> (bit % 8)) & 1) ? mask : 0);
        }
    } else {
        Binary_Write(out, in.data, start);
        Binary_Write(out, replacement->data, replacement->size);
        Binary_Write(out, in.data + in.offset, in.size - in.offset);
    }
    if (out->failed || replacement->failed || out->size > 65535) return luaL_error(state, "metadata exceeds the size limit or memory is exhausted");
    Metadata_Free(value);
    value->version = schema->version;
    bool defaults = out->size == schema->defaults.size && !memcmp(out->data, schema->defaults.data, out->size);
    if (!defaults) { value->data = out->data; value->size = out->size; out->data = NULL; }
    free(out->data); out->data = NULL;
    free(replacement->data); replacement->data = NULL;
    lua_pop(state, 2);
    return 0;
}

static Chunk *ResolveBlock(lua_State *state, int *index) {
    Vector3 *position = luaL_checkudata(state, 1, BLOCK_OBJECT);
    Vector3 chunkPosition = {floorf(position->x / 16), floorf(position->y / 16), floorf(position->z / 16)};
    Chunk *chunk = ServerWorld_GetChunkAt(chunkPosition);
    if (!chunk) luaL_error(state, "block chunk is not loaded");
    *index = ChunkData_PositionToIndex((int)(position->x - chunkPosition.x * 16),
        (int)(position->y - chunkPosition.y * 16), (int)(position->z - chunkPosition.z * 16));
    if (chunk->data[*index] >= 256) luaL_error(state, "block type has no metadata schema");
    return chunk;
}
static int BlockGet(lua_State *state) {
    int index; Chunk *chunk = ResolveBlock(state, &index);
    Metadata empty = {0}, *value = ChunkMetadata_Get(chunk, index);
    return LuaMetadata_Get(state, blockSchemas[chunk->data[index]], value ? value : &empty, 2);
}
static int BlockWrite(lua_State *state, bool reset) {
    int index; Chunk *chunk = ResolveBlock(state, &index);
    Metadata *existing = ChunkMetadata_Get(chunk, index);
    Metadata *value = existing;
    if (!value) {
        value = lua_newuserdata(state, sizeof(*value));
        *value = (Metadata){0};
        luaL_setmetatable(state, "midless.MetadataValue");
    }
    // The setter commits only after encoding succeeds, so existing data needs no backup.
    LuaMetadata_Set(state, blockSchemas[chunk->data[index]], value, 2, reset ? 0 : 3);
    if (existing) {
        if (!value->size) ChunkMetadata_Clear(chunk, index);
    } else {
        if (!ChunkMetadata_Set(chunk, index, value)) return luaL_error(state, "out of memory");
        Metadata_Free(value);
    }
    return 0;
}
static int BlockSet(lua_State *state) { return BlockWrite(state, false); }
static int BlockReset(lua_State *state) { return BlockWrite(state, true); }
static int BlockId(lua_State *state) { int index; Chunk *chunk = ResolveBlock(state, &index); lua_pushinteger(state, chunk->data[index]); return 1; }
static int BlockPosition(lua_State *state) {
    Vector3 *position = luaL_checkudata(state, 1, BLOCK_OBJECT);
    lua_createtable(state, 0, 3);
    lua_pushinteger(state, position->x); lua_setfield(state, -2, "x");
    lua_pushinteger(state, position->y); lua_setfield(state, -2, "y");
    lua_pushinteger(state, position->z); lua_setfield(state, -2, "z");
    return 1;
}
static int BlockLoaded(lua_State *state) {
    Vector3 *position = luaL_checkudata(state, 1, BLOCK_OBJECT);
    Vector3 chunkPosition = {floorf(position->x / 16), floorf(position->y / 16), floorf(position->z / 16)};
    lua_pushboolean(state, ServerWorld_GetChunkAt(chunkPosition) != NULL);
    return 1;
}
static int BlockSetId(lua_State *state) {
    int index;
    ResolveBlock(state, &index);
    int id = Integer(state, 2, 0, 255);
    if (!ServerWorld_IsBlockDefined(id)) return luaL_error(state, "block ID is not defined");
    Vector3 *position = luaL_checkudata(state, 1, BLOCK_OBJECT);
    ServerWorld_SetBlock(*position, id, true, false, true);
    return 0;
}

// Inventory references retain the safe owner handle, not a pointer into a chunk or entity.
static void PushInventory(lua_State *state, int owner, const char *field) {
    owner = lua_absindex(state, owner);
    lua_newuserdata(state, 1);
    lua_newtable(state);
    lua_pushvalue(state, owner); lua_setfield(state, -2, "owner");
    lua_pushstring(state, field); lua_setfield(state, -2, "field");
    lua_setuservalue(state, -2);
    luaL_setmetatable(state, INVENTORY_REF);
}
int LuaMetadata_Inventory(lua_State *state, int schema, int owner, int key) {
    const Field *field = FindField(state, GetSchema(state, schema), key);
    if (field->type != FIELD_INVENTORY) return luaL_error(state, "field is not an inventory");
    PushInventory(state, owner, field->name);
    lua_getuservalue(state, -1);
    lua_pushinteger(state, field->limit); lua_setfield(state, -2, "slots");
    lua_pop(state, 1);
    return 1;
}
static int BlockInventory(lua_State *state) {
    int index; Chunk *chunk = ResolveBlock(state, &index);
    return LuaMetadata_Inventory(state, blockSchemas[chunk->data[index]], 1, 2);
}
static int InventorySlot(lua_State *state) {
    luaL_checkudata(state, 1, INVENTORY_REF);
    lua_getuservalue(state, 1); lua_getfield(state, -1, "slots");
    int slots = lua_tointeger(state, -1); lua_pop(state, 2);
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
        int index; Chunk *chunk = ResolveBlock(state, &index);
        Metadata empty = {0}, *value = ChunkMetadata_Get(chunk, index);
        return LuaMetadata_Get(state, blockSchemas[chunk->data[index]], value ? value : &empty, key);
    }
    Entity *entity = LuaEntities_Check(state, 1);
    return LuaMetadata_Get(state, LuaEntities_MetadataSchema(entity->definitionId), &entity->metadata, key);
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
    lua_pushvalue(state, 3); lua_rawseti(state, -2, slot);
    int values = lua_gettop(state);
    if (luaL_testudata(state, 1, BLOCK_OBJECT)) {
        lua_pushvalue(state, key); lua_replace(state, 2);
        lua_pushvalue(state, values); lua_replace(state, 3);
        return BlockWrite(state, false);
    }
    Entity *entity = LuaEntities_Check(state, 1);
    return LuaMetadata_Set(state, LuaEntities_MetadataSchema(entity->definitionId), &entity->metadata, key, values);
}
static int InventoryAdd(lua_State *state) {
    luaL_checkudata(state, 1, INVENTORY_REF);
    luaL_checktype(state, 2, LUA_TTABLE);
    lua_getfield(state, 2, "id"); int id = Integer(state, -1, 1, 65535); lua_pop(state, 1);
    lua_getfield(state, 2, "count"); int count = Integer(state, -1, 1, INT_MAX); lua_pop(state, 1);
    lua_getuservalue(state, 1); lua_getfield(state, -1, "slots");
    int slots = lua_tointeger(state, -1); lua_pop(state, 2);
    int key = InventoryOwner(state);
    OwnerGet(state, key);
    int values = lua_gettop(state);
    ItemStack items[255] = {0};
    for (int i = 0; i < slots; i++) {
        lua_rawgeti(state, values, i + 1);
        if (!lua_isnil(state, -1)) {
            lua_getfield(state, -1, "id"); items[i].itemId = lua_tointeger(state, -1); lua_pop(state, 1);
            lua_getfield(state, -1, "count"); items[i].count = lua_tointeger(state, -1); lua_pop(state, 1);
        }
        lua_pop(state, 1);
    }
    if (Inventory_AddToSlots(items, slots, 0, id, count) != count) { lua_pushboolean(state, false); return 1; }
    for (int i = 0; i < slots; i++) if (items[i].count) {
        lua_createtable(state, 0, 2);
        lua_pushinteger(state, items[i].itemId); lua_setfield(state, -2, "id");
        lua_pushinteger(state, items[i].count); lua_setfield(state, -2, "count");
        lua_rawseti(state, values, i + 1);
    }
    if (luaL_testudata(state, 1, BLOCK_OBJECT)) {
        lua_pushvalue(state, key); lua_replace(state, 2);
        lua_pushvalue(state, values); lua_replace(state, 3);
        BlockWrite(state, false);
    } else {
        Entity *entity = LuaEntities_Check(state, 1);
        LuaMetadata_Set(state, LuaEntities_MetadataSchema(entity->definitionId), &entity->metadata, key, values);
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
        lua_getfield(L, 1, names[i]); double value = luaL_checknumber(L, -1); lua_pop(L, 1);
        if (!isfinite(value) || fabs(value) > 33554430) return luaL_error(L, "block coordinate out of range");
        *coordinates[i] = floor(value);
    }
    LuaMetadata_PushBlock(L, position);
    return 1;
}
void LuaMetadata_PushBlock(lua_State *state, Vector3 position) {
    Vector3 *handle = lua_newuserdata(state, sizeof(*handle)); *handle = position;
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
typedef struct InventoryRequest {
    Vector3 position;
    const char *field;
    ItemStack *slots;
    int count;
    bool write;
} InventoryRequest;
static int AccessBlockInventory(lua_State *state) {
    InventoryRequest *request = lua_touserdata(state, 1);
    LuaMetadata_PushBlock(state, request->position); lua_replace(state, 1);
    lua_pushstring(state, request->field);
    int index; Chunk *chunk = ResolveBlock(state, &index);
    const Field *field = FindField(state, GetSchema(state, blockSchemas[chunk->data[index]]), 2);
    if (field->type != FIELD_INVENTORY || field->limit != request->count)
        return luaL_error(state, "inventory schema changed");
    if (!request->write) {
        BlockGet(state);
        for (int i = 0; i < request->count; i++) {
            ItemStack *slot = &request->slots[i];
            *slot = (ItemStack){0};
            lua_rawgeti(state, -1, i + 1);
            if (!lua_isnil(state, -1)) {
                lua_getfield(state, -1, "id"); slot->itemId = lua_tointeger(state, -1); lua_pop(state, 1);
                lua_getfield(state, -1, "count"); slot->count = lua_tointeger(state, -1); lua_pop(state, 1);
            }
            lua_pop(state, 1);
        }
    } else {
        lua_newtable(state);
        for (int i = 0; i < request->count; i++) {
            ItemStack slot = request->slots[i];
            if (!slot.count) continue;
            lua_createtable(state, 0, 2);
            lua_pushinteger(state, slot.itemId); lua_setfield(state, -2, "id");
            lua_pushinteger(state, slot.count); lua_setfield(state, -2, "count");
            lua_rawseti(state, -2, i + 1);
        }
        BlockWrite(state, false);
    }
    return 0;
}
bool LuaMetadata_BlockInventory(Vector3 position, const char *field, ItemStack *slots, int count, bool write) {
    if (!L || count < 1 || count > 255) return false;
    InventoryRequest request = {position, field, slots, count, write};
    int top = lua_gettop(L);
    lua_pushcfunction(L, AccessBlockInventory); lua_pushlightuserdata(L, &request);
    bool ok = lua_pcall(L, 1, 0, 0) == LUA_OK;
    lua_settop(L, top);
    return ok;
}
static int FreeWriter(lua_State *state) { BinaryWriter *out = lua_touserdata(state, 1); free(out->data); out->data = NULL; return 0; }
static int FreeValue(lua_State *state) { Metadata_Free(lua_touserdata(state, 1)); return 0; }
static void Metatable(const char *name, const luaL_Reg *methods) {
    luaL_newmetatable(L, name);
    lua_newtable(L); luaL_setfuncs(L, methods, 0); lua_setfield(L, -2, "__index");
    lua_pushstring(L, name); lua_setfield(L, -2, "__metatable");
    lua_pop(L, 1);
}
void LuaMetadata_Init(void) {
    for (int i = 0; i < 256; i++) blockSchemas[i] = -1;
    const luaL_Reg block[] = {{"get_id", BlockId}, {"set_id", BlockSetId},
        {"get_position", BlockPosition}, {"is_loaded", BlockLoaded},
        {"get_metadata", BlockGet}, {"set_metadata", BlockSet},
        {"reset_metadata", BlockReset}, {"get_inventory", BlockInventory}, {NULL, NULL}};
    const luaL_Reg inventory[] = {{"get_stack", InventoryGet}, {"set_stack", InventorySet}, {"add_item", InventoryAdd}, {NULL, NULL}};
    Metatable(BLOCK_OBJECT, block); Metatable(INVENTORY_REF, inventory);
    luaL_newmetatable(L, "midless.MetadataWriter"); lua_pushcfunction(L, FreeWriter); lua_setfield(L, -2, "__gc"); lua_pop(L, 1);
    luaL_newmetatable(L, "midless.MetadataValue"); lua_pushcfunction(L, FreeValue); lua_setfield(L, -2, "__gc"); lua_pop(L, 1);
}
void LuaMetadata_Shutdown(void) {
    for (int i = 0; i < schemaCount; i++) {
        Metadata_Free(&schemas[i]->defaults);
        free(schemas[i]);
    }
    schemaCount = 0;
}
