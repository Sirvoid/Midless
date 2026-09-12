/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_METADATA_H
#define MIDLESS_LUA_METADATA_H
#include "../scripthooks.h"
#include "minilua.h"
#include "../metadata.h"
#include "../world/chunk/chunkmetadata.h"
#include "inventory.h"
#include "raylib.h"
#include "itemdefinition.h"
void LuaMetadata_ItemBar(int id, int definition, ItemBar *bar);

// Schema registry IDs are runtime-only. Payloads store the declared version.
int LuaMetadata_Register(lua_State *state, int definition);
void LuaMetadata_DefineItem(int id, int definition);
void LuaMetadata_ReadItem(lua_State *state, int index, ItemStack *stack);
void LuaMetadata_PushItem(lua_State *state, ItemStack stack);
void LuaMetadata_DefineBlock(int blockId, int definition);
void LuaMetadata_Init(void);
void LuaMetadata_Shutdown(void);
int LuaMetadata_GetBlock(void);
int LuaMetadata_Get(lua_State *state, int schema, Metadata *value, int key);
int LuaMetadata_Set(lua_State *state, int schema, Metadata *value, int key, int input);
int LuaMetadata_Inventory(lua_State *state, int schema, int owner, int key);
void LuaMetadata_PushBlock(lua_State *state, Vector3 position);
Vector3 LuaMetadata_CheckBlock(lua_State *state, int index);
int LuaMetadata_CheckBlockInventory(lua_State *state, int index, Vector3 position, char field[65]);
struct Player;
int LuaMetadata_DefinePlayer(lua_State *state);
int LuaMetadata_Player(lua_State *state, struct Player *player, bool write, bool reset);
int LuaMetadata_RegisterPlayerChange(lua_State *state);
int LuaMetadata_RegisterHPChange(lua_State *state);
#endif
