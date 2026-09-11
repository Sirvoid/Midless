#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "spawnmanager.h"
#include "world/world.h"
#include "scripting/luaentities.h"
#include "items.h"

extern lua_State *L;
static SpawnRule rules[SPAWN_RULE_LIMIT];
static int ruleCount, nextRule;

static double Number(int table, const char *field, double fallback, double min, double max, bool integer) {
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
    if (!size || size > 64 || memchr(value, 0, size)) luaL_error(L, "invalid spawn name");
    memcpy(out, value, size + 1);
}
static bool Section(const char *name) {
    lua_getfield(L, 2, name);
    if (lua_isnil(L, -1)) { lua_pop(L, 1); return false; }
    luaL_checktype(L, -1, LUA_TTABLE);
    return true;
}
int LuaSpawning_Register(void) {
    SpawnRule r = {.callback = LUA_NOREF, .interval = 5, .attempts = 8, .chance = 0.25f,
        .minDistance = 24, .maxDistance = 64, .localLimit = 8, .globalLimit = 64,
        .localRadius = 64, .verticalRange = 16, .avoidLiquids = true};
    Name(1, r.name);
    luaL_checktype(L, 2, LUA_TTABLE);
    if (ruleCount == SPAWN_RULE_LIMIT) return luaL_error(L, "spawn registry is full");
    for (int i = 0; i < ruleCount; i++) if (!strcmp(r.name, rules[i].name))
        return luaL_error(L, "spawn rule already registered");
    lua_getfield(L, 2, "entity");
    r.definition = LuaEntities_Find(luaL_checkstring(L, -1)); lua_pop(L, 1);
    if (r.definition < 0 || !LuaEntities_Body(r.definition).enabled)
        return luaL_error(L, "spawn entity must be registered with an enabled body");
    strcpy(r.group, LuaEntities_Group(r.definition));
    r.interval = Number(2, "interval", 5, 0.1, 86400, false);
    r.attempts = Number(2, "attempts", 8, 1, 64, true);
    r.chance = Number(2, "chance", 0.25, 0, 1, false);
    if (Section("distance")) {
        int t = lua_gettop(L);
        r.minDistance = Number(t, "min", 24, 0, 1024, false);
        r.maxDistance = Number(t, "max", 64, 1, 1024, false);
        lua_pop(L, 1);
    }
    if (r.minDistance >= r.maxDistance) return luaL_error(L, "spawn distance min must be less than max");
    if (Section("placement")) {
        int t = lua_gettop(L);
        lua_getfield(L, t, "type");
        if (strcmp(luaL_optstring(L, -1, "ground"), "ground")) return luaL_error(L, "only ground spawning is supported");
        lua_pop(L, 1);
        r.verticalRange = Number(t, "vertical_range", 16, 1, 64, true);
        lua_getfield(L, t, "avoid_liquids");
        if (!lua_isnil(L, -1)) { luaL_checktype(L, -1, LUA_TBOOLEAN); r.avoidLiquids = lua_toboolean(L, -1); }
        lua_pop(L, 1);
        lua_getfield(L, t, "ground_blocks");
        if (!lua_isnil(L, -1)) {
            luaL_checktype(L, -1, LUA_TTABLE);
            r.filterGround = true;
            size_t count = lua_rawlen(L, -1);
            if (!count || count > 256) return luaL_error(L, "ground_blocks must contain 1 to 256 blocks");
            for (size_t i = 1; i <= count; i++) {
                lua_rawgeti(L, -1, i);
                int id = ServerItems_Id(L, -1, true, false);
                if (id < 0 || id > 255) return luaL_error(L, "invalid spawn ground block");
                r.groundBlocks[id] = true;
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 2);
    }
    if (Section("population")) {
        int t = lua_gettop(L);
        lua_getfield(L, t, "group");
        if (!lua_isnil(L, -1)) Name(-1, r.group);
        lua_pop(L, 1);
        r.localLimit = Number(t, "local_limit", 8, 1, WORLD_MAX_ENTITIES, true);
        r.globalLimit = Number(t, "global_limit", 64, 1, WORLD_MAX_ENTITIES, true);
        r.localRadius = Number(t, "local_radius", 64, 1, 2048, false);
        lua_pop(L, 1);
    }
    if (r.group[0] && strcmp(r.group, LuaEntities_Group(r.definition)))
        return luaL_error(L, "spawn population group must match entity population_group");
    // One group has one cap policy, preventing rules with larger caps bypassing it.
    for (int i = 0; i < ruleCount; i++) {
        SpawnRule *other = &rules[i];
        bool same = r.group[0] ? !strcmp(r.group, other->group) :
            !other->group[0] && r.definition == other->definition;
        if (same && (r.localLimit != other->localLimit || r.globalLimit != other->globalLimit || r.localRadius != other->localRadius))
            return luaL_error(L, "spawn rules sharing a population must use identical caps and radius");
    }
    lua_getfield(L, 2, "can_spawn");
    if (!lua_isnil(L, -1)) { luaL_checktype(L, -1, LUA_TFUNCTION); r.callback = luaL_ref(L, LUA_REGISTRYINDEX); }
    else lua_pop(L, 1);
    rules[ruleCount++] = r;
    return 0;
}
static Entity *PlayerEntity(int id) {
    Player *p = serverWorld.players[id];
    if (!p || p->disconnected || !p->movementReady || p->entityId < 0 || p->entityId >= WORLD_MAX_ENTITIES) return NULL;
    Entity *e = &serverWorld.entities[p->entityId];
    return e->active && !e->pendingRemoval ? e : NULL;
}
static float DistanceSquared(Vector3 a, Vector3 b) {
    float x = a.x-b.x, y = a.y-b.y, z = a.z-b.z;
    return x*x+y*y+z*z;
}
static float Nearest(Vector3 position) {
    float nearest = INFINITY;
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Entity *e = PlayerEntity(i);
        if (e) nearest = fminf(nearest, DistanceSquared(position, e->position));
    }
    return nearest;
}
static bool Allowed(const SpawnRule *r, Vector3 p) {
    float nearest = Nearest(p);
    if (nearest < r->minDistance*r->minDistance || nearest > r->maxDistance*r->maxDistance) return false;
    int local = 0, global = 0, free = 0;
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        Entity *e = &serverWorld.entities[i];
        if (!e->active) { free++; continue; }
        if (e->pendingRemoval || e->definitionId < 0) continue;
        bool matches = r->group[0] ? !strcmp(r->group, LuaEntities_Group(e->definitionId)) : e->definitionId == r->definition;
        if (!matches) continue;
        global++;
        if (DistanceSquared(e->position, p) <= r->localRadius*r->localRadius) local++;
    }
    return free > 0 && local < r->localLimit && global < r->globalLimit;
}
static double Random(void) { return (double)rand() / ((double)RAND_MAX + 1); }
static bool Filter(const SpawnRule *r, Vector3 p, int playerId) {
    if (r->callback == LUA_NOREF) return true;
    int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, r->callback);
    lua_createtable(L, 0, 3);
    lua_pushnumber(L, p.x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, p.y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, p.z); lua_setfield(L, -2, "z");
    lua_createtable(L, 0, 2);
    lua_pushstring(L, r->name); lua_setfield(L, -2, "rule");
    lua_pushinteger(L, playerId); lua_setfield(L, -2, "player_id");
    bool ok = lua_pcall(L, 2, 1, 0) == LUA_OK;
    if (!ok) TraceLog(LOG_WARNING, "Spawn filter %s: %s", r->name, lua_tostring(L, -1));
    bool allow = ok && lua_isboolean(L, -1) && lua_toboolean(L, -1);
    lua_settop(L, top);
    return allow;
}
void ServerSpawning_Update(float dt) {
    if (!serverWorld.entities || !serverWorld.players || !isfinite(dt) || dt <= 0) return;
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        Entity *e = &serverWorld.entities[i];
        if (e->active && !e->pendingRemoval && e->definitionId >= 0 &&
            LuaEntities_Despawn(e, dt, Nearest(e->position))) ServerWorld_RemoveEntity(i);
    }
    int count = ruleCount; // Rules added by callbacks start on the next update.
    for (int i = 0; i < count; i++) {
        SpawnRule *r = &rules[i];
        r->elapsed += dt;
        if (r->elapsed < r->interval) continue;
        r->elapsed = fmod(r->elapsed, r->interval);
        for (int p = 0; p < WORLD_MAX_PLAYERS; p++)
            if (!r->remaining[p] && PlayerEntity(p)) r->remaining[p] = r->attempts;
    }
    // At most four bounded column searches per tick, rotating rules and players.
    for (int budget = 0; budget < 4 && count; budget++) {
        SpawnRule *r = NULL;
        int playerId = -1;
        for (int n = 0; n < count && playerId < 0; n++) {
            r = &rules[nextRule++ % count];
            nextRule %= count;
            for (int p = 0; p < WORLD_MAX_PLAYERS; p++) {
                int id = r->nextPlayer++ % WORLD_MAX_PLAYERS;
                r->nextPlayer %= WORLD_MAX_PLAYERS;
                if (!r->remaining[id]) continue;
                if (!PlayerEntity(id)) { r->remaining[id] = 0; continue; }
                r->remaining[id]--; playerId = id; break;
            }
        }
        if (playerId < 0) break;
        Entity *player = PlayerEntity(playerId);
        double angle = Random()*6.283185307179586;
        double radius = sqrt(r->minDistance*r->minDistance + Random()*
            (r->maxDistance*r->maxDistance-r->minDistance*r->minDistance));
        Vector3 column = {player->position.x + cos(angle)*radius, player->position.y, player->position.z + sin(angle)*radius};
        Vector3 position;
        EntityBody body = LuaEntities_Body(r->definition);
        if (!ServerSpawnPlacement_Find(r, body, column, &position) || !Allowed(r, position) || Random() >= r->chance) continue;
        if (!Filter(r, position, playerId)) continue;
        if (Allowed(r, position) && ServerSpawnPlacement_Valid(r, body, position))
            LuaEntities_TrySpawn(r->definition, position);
    }
}
void ServerSpawning_Reset(void) {
    for (int i = 0; i < ruleCount; i++) luaL_unref(L, LUA_REGISTRYINDEX, rules[i].callback);
    memset(rules, 0, sizeof rules);
    ruleCount = nextRule = 0;
}
