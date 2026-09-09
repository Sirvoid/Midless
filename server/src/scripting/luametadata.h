#ifndef MIDLESS_LUA_METADATA_H
#define MIDLESS_LUA_METADATA_H
#include "minilua.h"
#include "../world/chunk/chunkmetadata.h"
#include "inventory.h"
#include "raylib.h"

// Schema registry IDs are runtime-only. Payloads store the declared version.
int LuaMetadata_Register(lua_State *state, int definition);
void LuaMetadata_DefineBlock(int blockId, int definition);
void LuaMetadata_Init(void);
void LuaMetadata_Shutdown(void);
bool LuaMetadata_Timer(Vector3 position, float dt);
void LuaMetadata_InventoryChanged(Vector3 position, const char *field);
void LuaMetadata_FlushChanges(void);
bool LuaMetadata_CanInsert(Vector3 position, const char *field, int slot, int item);
bool LuaMetadata_Progress(Vector3 position, const char *field, float *value);
int LuaMetadata_GetBlock(void);
int LuaMetadata_Get(lua_State *state, int schema, Metadata *value, int key);
int LuaMetadata_Set(lua_State *state, int schema, Metadata *value, int key, int input);
int LuaMetadata_Inventory(lua_State *state, int schema, int owner, int key);
bool LuaMetadata_Validate(int schema, const Metadata *value);
bool LuaMetadata_CanRead(int schema, const Metadata *value);
bool LuaMetadata_ValidateChunk(struct Chunk *chunk);
void LuaMetadata_PushBlock(lua_State *state, Vector3 position);
Vector3 LuaMetadata_CheckBlock(lua_State *state, int index);
int LuaMetadata_CheckBlockInventory(lua_State *state, int index, Vector3 position, char field[65]);
bool LuaMetadata_BlockInventory(Vector3 position, const char *field, ItemStack *slots, int count, bool write);
// Collect occupied stacks from every inventory field. -1 leaves the block intact.
int LuaMetadata_CollectBlockItems(Vector3 position, ItemStack *stacks, int capacity);
#endif
