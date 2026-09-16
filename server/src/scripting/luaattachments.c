/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "luaattachments.h"
#include "luaentities.h"
#include "luaplayers.h"
#include "../attachments.h"
#include "../world/world.h"
#include <math.h>

static Entity *Object(lua_State *L, int index) {
    Entity *e = LuaPlayers_TestEntity(L, index);
    return e ? e : LuaEntities_Check(L, index);
}
static void PushObject(lua_State *L, Entity *e) {
    if (!e)
        lua_pushnil(L);
    else if (e->ownerPlayerId >= 0)
        LuaPlayers_Push(serverWorld.players[e->ownerPlayerId]);
    else
        LuaEntities_Push(e);
}
static Vector3 Vector(lua_State *L, int index, float limit) {
    index = lua_absindex(L, index);
    luaL_checktype(L, index, LUA_TTABLE);
    const char *names[] = {"x", "y", "z"};
    float v[3];
    for (int i = 0; i < 3; i++) {
        lua_getfield(L, index, names[i]);
        double n = luaL_checknumber(L, -1);
        if (!isfinite(n) || fabs(n) > limit) luaL_error(L, "invalid attachment coordinate");
        v[i] = n;
        lua_pop(L, 1);
    }
    return (Vector3){v[0], v[1], v[2]};
}
static void PushVector(lua_State *L, Vector3 v) {
    lua_newtable(L);
    lua_pushnumber(L, v.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, v.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, v.z);
    lua_setfield(L, -2, "z");
}
int LuaAttachment_Attach(lua_State *L) {
    Entity *e = Object(L, 1), *parent = Object(L, 2);
    Attachment a = {0};
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua_getfield(L, 3, "offset");
        if (!lua_isnil(L, -1)) a.offset = Vector(L, -1, 64);
        lua_pop(L, 1);
        lua_getfield(L, 3, "rotation");
        if (!lua_isnil(L, -1)) a.rotation = Vector(L, -1, 100);
        lua_pop(L, 1);
        lua_getfield(L, 3, "inherit_rotation");
        if (!lua_isnil(L, -1)) luaL_checktype(L, -1, LUA_TBOOLEAN);
        a.inheritRotation = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }
    if (!ServerAttachment_Set(e, parent, a))
        return luaL_error(L, "cannot attach: invalid object, cycle, or unloading entity");
    return 0;
}
int LuaAttachment_Detach(lua_State *L) {
    Entity *e = Object(L, 1);
    Vector3 position;
    bool hasPosition = false, inherit = false;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua_getfield(L, 2, "position");
        if (!lua_isnil(L, -1)) {
            position = Vector(L, -1, 1000000);
            hasPosition = true;
        }
        lua_pop(L, 1);
        lua_getfield(L, 2, "inherit_velocity");
        if (!lua_isnil(L, -1)) luaL_checktype(L, -1, LUA_TBOOLEAN);
        inherit = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }
    ServerAttachment_Detach(e, hasPosition ? &position : NULL, inherit);
    return 0;
}
int LuaAttachment_Get(lua_State *L) {
    Entity *e = Object(L, 1);
    PushObject(L, ServerAttachment_Parent(e));
    lua_newtable(L);
    PushVector(L, e->attachment.offset);
    lua_setfield(L, -2, "offset");
    PushVector(L, e->attachment.rotation);
    lua_setfield(L, -2, "rotation");
    lua_pushboolean(L, e->attachment.inheritRotation);
    lua_setfield(L, -2, "inherit_rotation");
    return 2;
}
int LuaAttachment_Children(lua_State *L) {
    Entity *e = Object(L, 1);
    int count = 0;
    lua_newtable(L);
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        Entity *child = &serverWorld.entities[i];
        if (child->active && !child->pendingRemoval && ServerAttachment_Parent(child) == e) {
            PushObject(L, child);
            lua_rawseti(L, -2, ++count);
        }
    }
    return 1;
}
int LuaAttachment_Controller(lua_State *L) {
    Player *p = ServerControl_Player(Object(L, 1));
    if (p)
        LuaPlayers_Push(p);
    else
        lua_pushnil(L);
    return 1;
}
int LuaAttachment_SetController(lua_State *L) {
    Entity *e = Object(L, 1);
    Player *p = NULL;
    if (!lua_isnoneornil(L, 2)) {
        Entity *pe = LuaPlayers_TestEntity(L, 2);
        if (!pe) return luaL_error(L, "controller must be a connected player");
        p = serverWorld.players[pe->ownerPlayerId];
    }
    if (!ServerControl_Set(e, p)) return luaL_error(L, "cannot assign control to this entity");
    return 0;
}
int LuaAttachment_Controlled(lua_State *L) {
    Player *p = LuaPlayers_Check();
    Entity *e = p->controlledEntity ? &serverWorld.entities[p->controlledEntity - 1] : NULL;
    PushObject(L, e && ServerControl_Player(e) == p ? e : NULL);
    return 1;
}
