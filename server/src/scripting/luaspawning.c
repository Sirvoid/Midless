#include "luaspawning.h"
#include "luaentities.h"
#include "luaitems.h"
#include "../scripthooks.h"
#include "../entityregistry.h"
#include "../world/world.h"
#include <string.h>
#include <math.h>

extern lua_State *L;
static int callbacks[SPAWN_RULE_LIMIT];
static bool callbacksInitialized;

static double Number(int table, const char *field, double fallback, double min, double max,
                     bool integer) {
    lua_getfield(L, table, field);
    double value = luaL_optnumber(L, -1, fallback);
    lua_pop(L, 1);
    if (!isfinite(value) || value < min || value > max || (integer && floor(value) != value))
        luaL_error(L, "invalid spawn field '%s'", field);
    return value;
}

static void Name(int index, char out[65]) {
    size_t size;
    const char *value = luaL_checklstring(L, index, &size);
    if (!size || size > 64 || memchr(value, 0, size))
        luaL_error(L, "invalid spawn name");
    memcpy(out, value, size + 1);
}

static bool Section(const char *name) {
    lua_getfield(L, 2, name);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return false;
    }
    luaL_checktype(L, -1, LUA_TTABLE);
    return true;
}

int LuaSpawning_Register(void) {
    if (!callbacksInitialized)
        ScriptHooks_ResetSpawnFilters();
    SpawnRule r = {.interval = 5,
                   .attempts = 8,
                   .chance = 0.25f,
                   .minDistance = 24,
                   .maxDistance = 64,
                   .localLimit = 8,
                   .globalLimit = 64,
                   .localRadius = 64,
                   .verticalRange = 16,
                   .avoidLiquids = true};
    Name(1, r.name);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_getfield(L, 2, "entity");
    r.definition = ServerEntities_Find(luaL_checkstring(L, -1));
    lua_pop(L, 1);
    if (r.definition < 0 || !ServerEntities_Body(r.definition).enabled)
        return luaL_error(L, "spawn entity must be registered with an enabled body");
    strcpy(r.group, ServerEntities_Group(r.definition));
    r.interval = Number(2, "interval", 5, 0.1, 86400, false);
    r.attempts = Number(2, "attempts", 8, 1, 64, true);
    r.chance = Number(2, "chance", 0.25, 0, 1, false);
    if (Section("distance")) {
        int t = lua_gettop(L);
        r.minDistance = Number(t, "min", 24, 0, 1024, false);
        r.maxDistance = Number(t, "max", 64, 1, 1024, false);
        lua_pop(L, 1);
    }
    if (r.minDistance >= r.maxDistance)
        return luaL_error(L, "spawn distance min must be less than max");
    if (Section("placement")) {
        int t = lua_gettop(L);
        lua_getfield(L, t, "type");
        if (strcmp(luaL_optstring(L, -1, "ground"), "ground"))
            return luaL_error(L, "only ground spawning is supported");
        lua_pop(L, 1);
        r.verticalRange = Number(t, "vertical_range", 16, 1, 64, true);
        lua_getfield(L, t, "avoid_liquids");
        if (!lua_isnil(L, -1)) {
            luaL_checktype(L, -1, LUA_TBOOLEAN);
            r.avoidLiquids = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);
        lua_getfield(L, t, "ground_blocks");
        if (!lua_isnil(L, -1)) {
            luaL_checktype(L, -1, LUA_TTABLE);
            r.filterGround = true;
            size_t count = lua_rawlen(L, -1);
            if (!count || count > 256)
                return luaL_error(L, "ground_blocks must contain 1 to 256 blocks");
            for (size_t i = 1; i <= count; i++) {
                lua_rawgeti(L, -1, i);
                int id = LuaItems_Id(L, -1, true, false);
                if (id < 0 || id > 255)
                    return luaL_error(L, "invalid spawn ground block");
                r.groundBlocks[id] = true;
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 2);
    }
    if (Section("population")) {
        int t = lua_gettop(L);
        lua_getfield(L, t, "group");
        if (!lua_isnil(L, -1))
            Name(-1, r.group);
        lua_pop(L, 1);
        r.localLimit = Number(t, "local_limit", 8, 1, WORLD_MAX_ENTITIES, true);
        r.globalLimit = Number(t, "global_limit", 64, 1, WORLD_MAX_ENTITIES, true);
        r.localRadius = Number(t, "local_radius", 64, 1, 2048, false);
        lua_pop(L, 1);
    }
    if (r.group[0] && strcmp(r.group, ServerEntities_Group(r.definition)))
        return luaL_error(L, "spawn population group must match entity population_group");
    lua_getfield(L, 2, "can_spawn");
    if (!lua_isnil(L, -1))
        luaL_checktype(L, -1, LUA_TFUNCTION);
    int id;
    const char *error = ServerSpawning_Register(&r, &id);
    if (error)
        return luaL_error(L, "%s", error);
    callbacks[id] = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

bool ScriptHooks_SpawnFilter(int id, const SpawnRule *r, Vector3 p, int playerId) {
    if (!callbacksInitialized || id < 0 || id >= SPAWN_RULE_LIMIT || callbacks[id] < 0)
        return true;
    int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks[id]);
    lua_createtable(L, 0, 3);
    lua_pushnumber(L, p.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, p.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, p.z);
    lua_setfield(L, -2, "z");
    lua_createtable(L, 0, 2);
    lua_pushstring(L, r->name);
    lua_setfield(L, -2, "rule");
    lua_pushinteger(L, playerId);
    lua_setfield(L, -2, "player_id");
    bool ok = lua_pcall(L, 2, 1, 0) == LUA_OK;
    if (!ok)
        TraceLog(LOG_WARNING, "Spawn filter %s: %s", r->name, lua_tostring(L, -1));
    bool allow = ok && lua_isboolean(L, -1) && lua_toboolean(L, -1);
    lua_settop(L, top);
    return allow;
}

void ScriptHooks_ResetSpawnFilters(void) {
    for (int i = 0; i < SPAWN_RULE_LIMIT; i++) {
        if (callbacksInitialized && callbacks[i] >= 0)
            luaL_unref(L, LUA_REGISTRYINDEX, callbacks[i]);
        callbacks[i] = LUA_NOREF;
    }
    callbacksInitialized = true;
}
