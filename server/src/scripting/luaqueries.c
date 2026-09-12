/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "luaplayers.h"
#include "../lighting.h"
#include <math.h>
#include "luaqueries.h"
#include "luaentities.h"
#include "luametadata.h"
#include "../entityphysics.h"
#include "../worldquery.h"
#include "../world/world.h"

extern lua_State *L;
static int attackCallbacks[64], attackCount;
static Vector3 ReadPosition(int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    const char *names[] = {"x", "y", "z"};
    float values[3];
    for (int i = 0; i < 3; i++) {
        lua_getfield(L, index, names[i]);
        double value = luaL_checknumber(L, -1);
        if (!isfinite(value) || fabs(value) > 999900)
            luaL_error(L, "query position is outside the supported range");
        values[i] = value;
        lua_pop(L, 1);
    }
    return (Vector3){values[0], values[1], values[2]};
}
static void PushPosition(Vector3 p) {
    lua_createtable(L, 0, 3);
    lua_pushnumber(L, p.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, p.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, p.z);
    lua_setfield(L, -2, "z");
}
static void PushHit(WorldHit hit) {
    lua_createtable(L, 0, 6);
    const char *type = "nothing";
    if (hit.type == HIT_BLOCK) {
        type = "block";
        LuaMetadata_PushBlock(L, hit.block);
        lua_setfield(L, -2, "block");
    } else if (hit.type == HIT_UNLOADED)
        type = "unloaded";
    else if (hit.type == HIT_ENTITY) {
        Entity *e = &serverWorld.entities[hit.entityId];
        if (e->ownerPlayerId >= 0) {
            Player *player = serverWorld.players[e->ownerPlayerId];
            if (player && !player->disconnected) {
                type = "player";
                LuaPlayers_Push(player);
                lua_setfield(L, -2, "player");
            }
        } else {
            type = "entity";
            LuaEntities_Push(e);
            lua_setfield(L, -2, "entity");
        }
    }
    lua_pushstring(L, type);
    lua_setfield(L, -2, "type");
    PushPosition(hit.position);
    lua_setfield(L, -2, "position");
    PushPosition(hit.normal);
    lua_setfield(L, -2, "normal");
    lua_pushnumber(L, hit.distance);
    lua_setfield(L, -2, "distance");
}
int LuaQueries_Raycast(lua_State *state) {
    (void)state;
    Vector3 from = ReadPosition(1), to = ReadPosition(2);
    if (hypotf(hypotf(to.x - from.x, to.z - from.z), to.y - from.y) > 128)
        return luaL_error(L, "raycast is limited to 128 blocks");
    bool entities = true;
    int ignore = -1;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua_getfield(L, 3, "entities");
        if (!lua_isnil(L, -1)) {
            luaL_checktype(L, -1, LUA_TBOOLEAN);
            entities = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);
        lua_getfield(L, 3, "ignore");
        if (!lua_isnil(L, -1))
            ignore = LuaEntities_Check(L, -1)->id;
        lua_pop(L, 1);
        lua_getfield(L, 3, "ignore_player");
        if (!lua_isnil(L, -1)) {
            lua_Integer id = luaL_checkinteger(L, -1);
            if (id < 0 || id >= WORLD_MAX_PLAYERS)
                return luaL_error(L, "invalid player id");
            if (serverWorld.players && serverWorld.players[id])
                ignore = serverWorld.players[id]->entityId;
        }
        lua_pop(L, 1);
    }
    PushHit(ServerQuery_Raycast(from, to, entities, ignore));
    return 1;
}
static int Nearby(bool players) {
    Vector3 p = ReadPosition(1);
    double radius = luaL_checknumber(L, 2);
    if (!isfinite(radius) || radius < 0 || radius > 128)
        return luaL_error(L, "query radius must be 0..128");
    BoundingBox area = {{p.x - radius, p.y - radius, p.z - radius},
                        {p.x + radius, p.y + radius, p.z + radius}};
    int ids[WORLD_MAX_ENTITIES], count = ServerPhysics_QueryEntities(area, ids, WORLD_MAX_ENTITIES),
                                 added = 0;
    lua_newtable(L);
    for (int i = 0; i < count; i++) {
        Entity *e = &serverWorld.entities[ids[i]];
        float x = e->position.x - p.x, y = e->position.y - p.y, z = e->position.z - p.z;
        if (x * x + y * y + z * z > radius * radius)
            continue;
        if (players) {
            if (e->ownerPlayerId < 0 || !serverWorld.players)
                continue;
            Player *player = serverWorld.players[e->ownerPlayerId];
            if (!player || player->disconnected || !player->movementReady)
                continue;
            LuaPlayers_Push(player);
        } else {
            if (e->ownerPlayerId >= 0 || e->definitionId < 0)
                continue;
            LuaEntities_Push(e);
        }
        lua_rawseti(L, -2, ++added);
    }
    return 1;
}
int LuaQueries_Entities(lua_State *state) {
    (void)state;
    return Nearby(false);
}
int LuaQueries_Players(lua_State *state) {
    (void)state;
    return Nearby(true);
}
int LuaQueries_Light(lua_State *state) {
    (void)state;
    Vector3 pos = ReadPosition(1);
    int block, sky, level;
    if (!ServerLighting_Get(pos, &block, &sky, &level)) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 3);
    lua_pushinteger(L, block);
    lua_setfield(L, -2, "block");
    lua_pushinteger(L, sky);
    lua_setfield(L, -2, "sky");
    lua_pushinteger(L, level);
    lua_setfield(L, -2, "level");
    return 1;
}
int LuaQueries_NearestPlayer(lua_State *state) {
    (void)state;
    Vector3 pos = ReadPosition(1);
    double radius = luaL_checknumber(L, 2);
    if (!isfinite(radius) || radius < 0 || radius > 128)
        return luaL_error(L, "query radius must be 0..128");
    Player *nearest = NULL;
    double best = radius * radius;
    for (int i = 0; serverWorld.players && i < WORLD_MAX_PLAYERS; i++) {
        Player *p = serverWorld.players[i];
        if (!p || p->disconnected || !p->movementReady || p->entityId < 0 ||
            p->entityId >= WORLD_MAX_ENTITIES)
            continue;
        Entity *e = &serverWorld.entities[p->entityId];
        if (!e->active || e->pendingRemoval)
            continue;
        double x = e->position.x - pos.x, y = e->position.y - pos.y, z = e->position.z - pos.z;
        double distance = x * x + y * y + z * z;
        if (distance > best || (nearest && distance == best))
            continue;
        LuaPlayers_Push(p);
        lua_getfield(L, -1, "get_hp");
        lua_pushvalue(L, -2);
        lua_call(L, 1, 1);
        bool living = lua_tointeger(L, -1) > 0;
        lua_pop(L, 2);
        if (living) {
            nearest = p;
            best = distance;
        }
    }
    LuaPlayers_Push(nearest);
    return 1;
}
int LuaQueries_FindPath(lua_State *state) {
    (void)state;
    Entity *e = LuaEntities_Check(L, 1);
    Vector3 goal = ReadPosition(2), path[512];
    int count = ServerQuery_FindPath(e->body, e->position, goal, path, 512);
    if (!count) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, count, 0);
    for (int i = 0; i < count; i++) {
        PushPosition(path[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}
int LuaQueries_CanWalk(lua_State *state) {
    (void)state;
    Entity *e = LuaEntities_Check(L, 1);
    lua_pushboolean(L, ServerQuery_CanWalk(e->body, e->position, ReadPosition(2)));
    return 1;
}
int LuaQueries_RegisterAttack(lua_State *state) {
    (void)state;
    luaL_checktype(L, 1, LUA_TFUNCTION);
    if (attackCount == 64)
        return luaL_error(L, "too many attack callbacks");
    lua_pushvalue(L, 1);
    attackCallbacks[attackCount++] = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}
void ScriptHooks_QueriesAttack(Player *player) {
    double now = GetTime();
    if (!attackCount || !player->movementReady || now < player->nextMeleeAttack ||
        player->entityId < 0)
        return;
    player->nextMeleeAttack = now + 0.4;
    Entity *e = &serverWorld.entities[player->entityId];
    Vector3 from = e->position;
    from.y += 1.5f;
    float yaw = e->rotation.y, pitch = e->rotation.x, horizontal = cosf(pitch);
    Vector3 to = {from.x + sinf(yaw) * horizontal * 4.5f, from.y - sinf(pitch) * 4.5f,
                  from.z + cosf(yaw) * horizontal * 4.5f};
    WorldHit hit = ServerQuery_Raycast(from, to, true, e->id);
    uint64_t generation =
        hit.type == HIT_ENTITY ? serverWorld.entities[hit.entityId].generation : 0;
    int count = attackCount, top = lua_gettop(L);
    for (int i = 0; i < count; i++) {
        if (hit.type == HIT_ENTITY) {
            Entity *target = &serverWorld.entities[hit.entityId];
            if (!target->active || target->pendingRemoval || target->generation != generation)
                break;
        }
        lua_rawgeti(L, LUA_REGISTRYINDEX, attackCallbacks[i]);
        LuaPlayers_Push(player);
        PushHit(hit);
        bool handled = false;
        if (lua_pcall(L, 2, 1, 0) != LUA_OK)
            TraceLog(LOG_WARNING, "Player attack: %s", lua_tostring(L, -1));
        else
            handled = lua_toboolean(L, -1);
        lua_settop(L, top);
        if (handled)
            break;
    }
}
void LuaQueries_Reset(void) {
    for (int i = 0; i < attackCount; i++)
        luaL_unref(L, LUA_REGISTRYINDEX, attackCallbacks[i]);
    attackCount = 0;
}
