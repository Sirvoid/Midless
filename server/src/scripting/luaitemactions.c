/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "luaplayers.h"
#include "luaitems.h"
#include "luaitemactions.h"
#include "luaengine.h"
#include "luaentities.h"
#include "luametadata.h"
#include "../items.h"
#include "../serverinventory.h"
#include <string.h>
extern lua_State *L;

static int uses[ITEM_LIMIT], levels[ITEM_LIMIT], drops[256], placements[256];
static int attacks[ITEM_LIMIT], damages[ITEM_LIMIT];
static struct {
    int level;
    char group[65];
} requirements[256];
void LuaItemActions_Init(void) {
    for (int i = 0; i < ITEM_LIMIT; i++) {
        uses[i] = levels[i] = LUA_NOREF;
        attacks[i] = LUA_NOREF;
        damages[i] = 1;
    }
    for (int i = 0; i < 256; i++)
        drops[i] = placements[i] = LUA_NOREF;
    memset(requirements, 0, sizeof(requirements));
}
void LuaItemActions_Shutdown(void) {
    for (int i = 0; i < ITEM_LIMIT; i++) {
        luaL_unref(L, LUA_REGISTRYINDEX, uses[i]);
        luaL_unref(L, LUA_REGISTRYINDEX, attacks[i]);
        luaL_unref(L, LUA_REGISTRYINDEX, levels[i]);
    }
    for (int i = 0; i < 256; i++) {
        luaL_unref(L, LUA_REGISTRYINDEX, drops[i]);
        luaL_unref(L, LUA_REGISTRYINDEX, placements[i]);
    }
}
void LuaItemActions_Define(int id, int table, bool block) {
    lua_getfield(L, table, "damage");
    lua_Integer damage = lua_isnil(L, -1) ? 1 : luaL_checkinteger(L, -1);
    if (damage < 0 || damage > 65535)
        luaL_error(L, "damage must be 0..65535");
    lua_pop(L, 1);
    lua_getfield(L, table, "on_attack");
    if (!lua_isnil(L, -1))
        luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_pop(L, 1);
    lua_getfield(L, table, "on_use");
    if (!lua_isnil(L, -1))
        luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_pop(L, 1);
    if (block) {
        lua_getfield(L, table, "on_place");
        if (!lua_isnil(L, -1))
            luaL_checktype(L, -1, LUA_TFUNCTION);
        luaL_unref(L, LUA_REGISTRYINDEX, placements[id]);
        placements[id] = luaL_ref(L, LUA_REGISTRYINDEX);
        lua_getfield(L, table, "harvest_level");
        int level = lua_isnil(L, -1) ? 0 : luaL_checkinteger(L, -1);
        lua_pop(L, 1);
        if (level < 0 || level > 255)
            luaL_error(L, "harvest_level must be 0..255");
        lua_getfield(L, table, "dig_group");
        const char *group = lua_isnil(L, -1) ? "" : luaL_checkstring(L, -1);
        if (strlen(group) > 64 || (level && !group[0]))
            luaL_error(L, "harvest_level requires a dig_group");
        requirements[id].level = level;
        strcpy(requirements[id].group, group);
        lua_pop(L, 1);
        lua_getfield(L, table, "drops");
        if (!lua_isnil(L, -1) && !lua_istable(L, -1) && !lua_isfunction(L, -1))
            luaL_error(L, "drops must be a list or a function");
        luaL_unref(L, LUA_REGISTRYINDEX, drops[id]);
        drops[id] = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    lua_getfield(L, table, "harvest_levels");
    if (!lua_isnil(L, -1)) {
        luaL_checktype(L, -1, LUA_TTABLE);
        lua_newtable(L);
        lua_pushnil(L);
        while (lua_next(L, -3)) {
            if (lua_type(L, -2) != LUA_TSTRING)
                luaL_error(L, "harvest_levels keys must be group names");
            int level = luaL_checkinteger(L, -1);
            if (level < 0 || level > 255)
                luaL_error(L, "harvest levels must be 0..255");
            lua_pushvalue(L, -2);
            lua_pushinteger(L, level);
            lua_settable(L, -5);
            lua_pop(L, 1);
        }
        lua_remove(L, -2);
    }
    luaL_unref(L, LUA_REGISTRYINDEX, levels[id]);
    levels[id] = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_getfield(L, table, "on_use");
    luaL_unref(L, LUA_REGISTRYINDEX, uses[id]);
    uses[id] = luaL_ref(L, LUA_REGISTRYINDEX);
    damages[id] = damage;
    lua_getfield(L, table, "on_attack");
    luaL_unref(L, LUA_REGISTRYINDEX, attacks[id]);
    attacks[id] = luaL_ref(L, LUA_REGISTRYINDEX);
}

// Run damage, the tool callback and stack decoding inside one protected Lua call.
static int Attack(lua_State *state) {
    Player *player = lua_touserdata(state, lua_upvalueindex(1));
    int slot = INVENTORY_STORAGE_SLOTS + player->inventory.selectedHotbar;
    ItemStack tool = player->inventory.slots[slot];
    uint32_t revision = player->inventoryRevision;
    int damage = tool.count && tool.itemId < ITEM_LIMIT ? damages[tool.itemId] : 1;
    if (!damage)
        return 0;
    lua_getfield(state, 1, "entity");
    Entity *entity = LuaEntities_Check(state, -1);
    if (entity->ownerPlayerId >= 0 || entity->definitionId < 0 || !entity->maxHp || entity->dead)
        return 0;
    lua_getfield(state, -1, "damage");
    lua_pushvalue(state, -2);
    lua_pushinteger(state, damage);
    lua_createtable(state, 0, 3);
    LuaPlayers_Push(player);
    lua_setfield(state, -2, "attacker");
    lua_pushliteral(state, "melee");
    lua_setfield(state, -2, "cause");
    lua_createtable(state, 0, 2);
    lua_pushnumber(state, 8);
    lua_setfield(state, -2, "horizontal");
    lua_pushnumber(state, 4);
    lua_setfield(state, -2, "upward");
    lua_setfield(state, -2, "knockback");
    lua_call(state, 3, 1);
    int lost = lua_tointeger(state, -1);
    lua_pop(state, 1);
    if (lost <= 0 || !tool.count || tool.itemId >= ITEM_LIMIT || attacks[tool.itemId] < 0)
        return 0;
    lua_rawgeti(state, LUA_REGISTRYINDEX, attacks[tool.itemId]);
    LuaPlayers_Push(player);
    lua_pushvalue(state, 1); // Keep the original hit, even after a killing blow.
    LuaItems_PushStack(state, tool);
    lua_pushinteger(state, lost);
    lua_call(state, 4, 1);
    if (lua_isnil(state, -1))
        return 0;
    ItemStack replacement = {0};
    if (!(lua_isboolean(state, -1) && !lua_toboolean(state, -1))) {
        LuaItems_ReadStack(state, -1, &replacement);
        if (replacement.count && !ServerItems_IsDefined(replacement.itemId))
            return luaL_error(state, "attack returned an undefined item");
    }
    // Inventory edits during damage/death/attack callbacks take precedence.
    ItemStack current = player->inventory.slots[slot];
    if (player->disconnected || LuaPlayers_IsLeaving(player) ||
        player->inventoryRevision != revision || current.count != tool.count ||
        !ItemStack_Matches(current, tool))
        return 0;
    player->inventory.slots[slot] = replacement;
    player->inventoryRevision++;
    ServerInventory_UpdateHeldBlock(player);
    ServerInventory_Send(player);
    return 0;
}

void LuaItemActions_Attack(Player *player, int hitIndex) {
    int top = lua_gettop(L);
    hitIndex = lua_absindex(L, hitIndex);
    lua_pushlightuserdata(L, player);
    lua_pushcclosure(L, Attack, 1);
    lua_pushvalue(L, hitIndex);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK)
        TraceLog(LOG_WARNING, "Item attack: %s", lua_tostring(L, -1));
    lua_settop(L, top);
}
static void PushPosition(Vector3 position) {
    lua_createtable(L, 0, 3);
    lua_pushnumber(L, position.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, position.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, position.z);
    lua_setfield(L, -2, "z");
}
bool ScriptHooks_ItemActionsUse(Player *player, const InventoryAction *block, Entity *entity) {
    ItemStack stack = *Inventory_GetSelected(&player->inventory);
    if (!luaRunning || !stack.count || stack.itemId >= ITEM_LIMIT || uses[stack.itemId] < 0)
        return false;
    int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, uses[stack.itemId]);
    LuaPlayers_Push(player);
    LuaItems_PushStack(L, stack);
    lua_createtable(L, 0, 3);
    lua_pushstring(L, entity ? "entity" : block ? "block" : "nothing");
    lua_setfield(L, -2, "type");
    if (entity) {
        LuaEntities_Push(entity);
        lua_setfield(L, -2, "entity");
    } else if (block) {
        LuaMetadata_PushBlock(L, (Vector3){block->x, block->y, block->z});
        lua_setfield(L, -2, "block");
        const Vector3 normals[] = {{-1, 0, 0}, {1, 0, 0},  {0, -1, 0},
                                   {0, 1, 0},  {0, 0, -1}, {0, 0, 1}};
        PushPosition(normals[block->face]);
        lua_setfield(L, -2, "normal");
    }
    bool handled = true;
    if (lua_pcall(L, 3, 1, 0) != LUA_OK)
        TraceLog(LOG_WARNING, "Item on_use: %s", lua_tostring(L, -1));
    else
        handled = lua_toboolean(L, -1);
    lua_settop(L, top);
    return handled;
}
void ScriptHooks_ItemActionsPlaced(Player *player, Vector3 position, int blockId) {
    if (!luaRunning || blockId < 1 || blockId > 255 || placements[blockId] < 0)
        return;
    int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, placements[blockId]);
    LuaPlayers_Push(player);
    LuaMetadata_PushBlock(L, position);
    if (lua_pcall(L, 2, 0, 0) != LUA_OK)
        TraceLog(LOG_WARNING, "Block on_place: %s", lua_tostring(L, -1));
    lua_settop(L, top);
}

typedef struct DropOutput {
    ItemStack *stacks;
    int capacity, count;
} DropOutput;
static int ReadDrops(lua_State *state) {
    DropOutput *out = lua_touserdata(state, lua_upvalueindex(1));
    luaL_checktype(state, 1, LUA_TTABLE);
    size_t count = lua_rawlen(state, 1);
    if (count > (size_t)out->capacity)
        return luaL_error(state, "too many block drops");
    lua_pushnil(state);
    while (lua_next(state, 1)) {
        if (!lua_isinteger(state, -2) || lua_tointeger(state, -2) < 1 ||
            (size_t)lua_tointeger(state, -2) > count)
            return luaL_error(state, "drops must be a list of stacks");
        lua_pop(state, 1);
    }
    for (size_t i = 0; i < count; i++) {
        lua_rawgeti(state, 1, i + 1);
        if (lua_type(state, -1) == LUA_TSTRING) {
            out->stacks[i] = (ItemStack){LuaItems_Id(state, -1, false, false), 1};
        } else
            LuaItems_ReadStack(state, -1, &out->stacks[i]);
        if (!out->stacks[i].itemId || !out->stacks[i].count ||
            !ServerItems_IsDefined(out->stacks[i].itemId))
            return luaL_error(state, "invalid block drop");
        lua_pop(state, 1);
    }
    out->count = count;
    return 0;
}
int ScriptHooks_ItemActionsDrops(Player *player, Vector3 position, int block, ItemStack tool,
                                 ItemStack *stacks, int capacity) {
    int level = 0;
    if (tool.count && tool.itemId < ITEM_LIMIT && levels[tool.itemId] >= 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, levels[tool.itemId]);
        lua_getfield(L, -1, requirements[block].group);
        level = lua_tointeger(L, -1);
        lua_pop(L, 2);
    }
    if (level < requirements[block].level)
        return 0;
    if (drops[block] < 0) {
        if (capacity < 1)
            return -1;
        stacks[0] = (ItemStack){block, 1};
        return 1;
    }
    int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, drops[block]);
    if (lua_isfunction(L, -1)) {
        LuaPlayers_Push(player);
        LuaMetadata_PushBlock(L, position);
        LuaItems_PushStack(L, tool);
        if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
            TraceLog(LOG_WARNING, "Block drops: %s", lua_tostring(L, -1));
            lua_settop(L, top);
            return -1;
        }
    }
    DropOutput out = {stacks, capacity, 0};
    lua_pushlightuserdata(L, &out);
    lua_pushcclosure(L, ReadDrops, 1);
    lua_insert(L, -2);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        TraceLog(LOG_WARNING, "Block drops: %s", lua_tostring(L, -1));
        lua_settop(L, top);
        return -1;
    }
    lua_settop(L, top);
    return out.count;
}
