/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "luablockphysics.h"
#include "luaengine.h"
#include "luavalues.h"
#include "luaitems.h"
#include "luametadata.h"
#include "../blockstates.h"
#include "../metadatainternal.h"
#include <math.h>
#include <string.h>

static int callbacks[256], maxLevels[256];
extern lua_State *L;

static void FluidMetadata(lua_State *state, int table) {
    lua_getfield(state, table, "metadata");
    int original = lua_gettop(state), count = 0;
    if (!lua_isnil(state, original)) {
        luaL_checktype(state, original, LUA_TTABLE);
        count = lua_rawlen(state, original);
    }
    lua_createtable(state, count + 3, 0);
    int metadata = lua_gettop(state);
    const char *names[] = {"source", "level", "falling"};
    for (int i = 0; i < 3; i++) {
        lua_newtable(state);
        lua_pushstring(state, names[i]);
        lua_setfield(state, -2, "name");
        lua_pushstring(state, i == 1 ? "uint" : "bool");
        lua_setfield(state, -2, "type");
        if (i == 1) {
            lua_pushinteger(state, 4);
            lua_setfield(state, -2, "bits");
        }
        if (i == 0) {
            lua_pushboolean(state, true);
            lua_setfield(state, -2, "default");
        }
        lua_rawseti(state, metadata, i + 1);
    }
    for (int i = 1; i <= count; i++) {
        lua_rawgeti(state, original, i);
        luaL_checktype(state, -1, LUA_TTABLE);
        lua_getfield(state, -1, "name");
        const char *name = luaL_checkstring(state, -1);
        for (int field = 0; field < 3; field++)
            if (!strcmp(name, names[field]))
                luaL_error(state, "fluid metadata field '%s' is automatic", name);
        lua_pop(state, 1);
        lua_rawseti(state, metadata, i + 3);
    }
    lua_setfield(state, table, "metadata");
    lua_pop(state, 1);
}
BlockPhysicsDefinition LuaBlockPhysics_Read(lua_State *state, int table, BlockDefinition *block) {
    BlockPhysicsDefinition definition = {0};
    lua_getfield(state, table, "on_physics");
    definition.callback = !lua_isnil(state, -1);
    if (definition.callback)
        luaL_checktype(state, -1, LUA_TFUNCTION);
    lua_pop(state, 1);
    lua_getfield(state, table, "physics");
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        if (definition.callback)
            luaL_error(state, "on_physics requires a physics table");
        return definition;
    }
    luaL_checktype(state, -1, LUA_TTABLE);
    int physics = lua_gettop(state);
    lua_getfield(state, physics, "type");
    const char *type = lua_isnil(state, -1) ? "custom" : luaL_checkstring(state, -1);
    if (!strcmp(type, "falling"))
        definition.type = BLOCK_PHYSICS_FALLING;
    else if (!strcmp(type, "fluid"))
        definition.type = BLOCK_PHYSICS_FLUID;
    else if (!strcmp(type, "custom") && definition.callback)
        definition.type = BLOCK_PHYSICS_CUSTOM;
    else
        luaL_error(state, "physics.type must be falling, fluid, or custom (with on_physics)");
    lua_pop(state, 1);
    lua_getfield(state, physics, "interval");
    double interval = lua_isnil(state, -1) ? (definition.type == BLOCK_PHYSICS_FLUID ? 0.2 : 0.05)
                                           : luaL_checknumber(state, -1);
    if (!isfinite(interval) || interval < 0.05 || interval > 86400)
        luaL_error(state, "physics.interval must be between 0.05 and 86400 seconds");
    definition.interval = interval;
    lua_pop(state, 1);
    definition.maxLevel = LuaValues_IntField(physics, "max_level", 7, 1, 15);
    lua_getfield(state, physics, "renewable_sources");
    if (!lua_isnil(state, -1))
        luaL_checktype(state, -1, LUA_TBOOLEAN);
    definition.renewableSources = lua_toboolean(state, -1);
    lua_pop(state, 1);
    lua_getfield(state, physics, "replaceable");
    if (lua_isnil(state, -1)) {
        definition.replaceable[0] = true;
        if (definition.type == BLOCK_PHYSICS_FALLING)
            definition.replaceable[5] = true;
    } else {
        luaL_checktype(state, -1, LUA_TTABLE);
        for (int i = 1; i <= lua_rawlen(state, -1); i++) {
            lua_rawgeti(state, -1, i);
            definition.replaceable[LuaItems_Id(state, -1, true, false)] = true;
            lua_pop(state, 1);
        }
    }
    lua_pop(state, 2);
    if (definition.type == BLOCK_PHYSICS_FLUID) {
        const char *models[] = {"boxes",  "models",          "variants",       "state_fields",
                                "bounds", "collision_boxes", "selection_boxes"};
        for (int i = 0; i < 7; i++) {
            lua_getfield(state, table, models[i]);
            if (!lua_isnil(state, -1))
                luaL_error(state, "fluid models are automatic; omit '%s'", models[i]);
            lua_pop(state, 1);
        }
        if (block->modelType != BLOCK_MODEL_SOLID)
            luaL_error(state, "fluids require a solid box model");
        lua_getfield(state, table, "render");
        if (lua_isnil(state, -1))
            block->renderType = BLOCK_RENDER_TRANSLUCENT;
        lua_pop(state, 1);
        block->colliderType = BLOCK_COLLIDER_LIQUID;
        FluidMetadata(state, table);
    }
    return definition;
}
void LuaBlockPhysics_Define(lua_State *state, int id, int table) {
    luaL_unref(state, LUA_REGISTRYINDEX, callbacks[id]);
    lua_getfield(state, table, "on_physics");
    callbacks[id] = luaL_ref(state, LUA_REGISTRYINDEX);
    lua_getfield(state, table, "physics");
    if (!lua_isnil(state, -1)) {
        int physics = lua_gettop(state);
        lua_getfield(state, physics, "type");
        if (lua_isstring(state, -1) && !strcmp(lua_tostring(state, -1), "fluid"))
            maxLevels[id] = LuaValues_IntField(physics, "max_level", 7, 1, 15);
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
}
void ScriptHooks_BlockPhysics(Vector3 position, int id) {
    if (!luaRunning || callbacks[id] < 0)
        return;
    int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks[id]);
    LuaValues_PushPosition(position);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK)
        TraceLog(LOG_WARNING, "Block on_physics: %s", lua_tostring(L, -1));
    lua_settop(L, top);
}
void LuaBlockPhysics_Init(void) {
    for (int i = 0; i < 256; i++) {
        callbacks[i] = LUA_NOREF;
        maxLevels[i] = 0;
    }
}
void LuaBlockPhysics_Shutdown(void) {
    for (int i = 0; i < 256; i++) {
        luaL_unref(L, LUA_REGISTRYINDEX, callbacks[i]);
        callbacks[i] = LUA_NOREF;
    }
    ServerBlockPhysics_Reset();
}
int LuaBlockPhysics_Move(lua_State *state) {
    Vector3 from = LuaValues_ReadPosition(1, true), to = LuaValues_ReadPosition(2, true);
    lua_pushboolean(state, ServerBlockPhysics_Move(from, to));
    return 1;
}
int LuaBlockPhysics_Schedule(lua_State *state) {
    Vector3 pos = LuaValues_ReadPosition(1, true);
    double delay = lua_gettop(state) < 2 ? 0.05 : luaL_checknumber(state, 2);
    if (!isfinite(delay) || delay < 0.05 || delay > 86400)
        return luaL_error(state, "physics delay must be between 0.05 and 86400 seconds");
    lua_pushboolean(state, ServerBlockPhysics_Schedule(pos, delay));
    return 1;
}
void LuaBlockPhysics_CheckField(lua_State *state, int id, int key, int value) {
    if (!maxLevels[id] || !value)
        return;
    const char *name = luaL_checkstring(state, key);
    if (strcmp(name, "level"))
        return;
    lua_Integer level = luaL_checkinteger(state, value);
    if (level < 0 || level > maxLevels[id])
        luaL_error(state, "fluid level exceeds physics.max_level");
}
void LuaBlockPhysics_ReadState(lua_State *state, int id, int table, Metadata *value) {
    if (id < 0 || id >= 256)
        luaL_error(state, "block type has no metadata schema");
    table = lua_absindex(state, table);
    luaL_checktype(state, table, LUA_TTABLE);
    lua_pushnil(state);
    while (lua_next(state, table)) {
        LuaBlockPhysics_CheckField(state, id, lua_gettop(state) - 1, lua_gettop(state));
        LuaMetadata_Set(state, serverBlockSchemas[id], value, lua_gettop(state) - 1,
                        lua_gettop(state));
        lua_pop(state, 1);
    }
}
int LuaBlockPhysics_SetState(lua_State *state) {
    Vector3 p = LuaValues_ReadPosition(1, true);
    int index;
    Chunk *chunk = ServerBlockPhysics_Find(p, &index);
    if (!chunk || chunk->savePending) {
        lua_pushboolean(state, false);
        return 1;
    }
    Metadata *value = lua_newuserdata(state, sizeof(*value));
    *value = (Metadata){0};
    luaL_setmetatable(state, "midless.MetadataValue");
    Metadata *existing = ChunkMetadata_Get(chunk, index);
    if (existing && !Metadata_Copy(value, existing))
        return luaL_error(state, "out of memory");
    LuaBlockPhysics_ReadState(state, chunk->data[index], 2, value);
    chunk->states[index] = ServerBlockStates_Resolve(chunk, index);
    if (!ChunkMetadata_Set(chunk, index, value))
        return luaL_error(state, "out of memory");
    Metadata_Free(value);
    ServerBlockStates_Changed(chunk, index);
    ServerBlockPhysics_Changed(p);
    lua_pushboolean(state, true);
    return 1;
}
