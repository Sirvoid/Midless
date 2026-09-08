#include "minilua.h"
#include <math.h>
#include <string.h>
#include "luaentities.h"
#include "luamodels.h"
#include "luametadata.h"
#include "../world/world.h"
#include "../entityphysics.h"

extern lua_State *L;
#define ENTITY_HANDLE "midless.Entity"
#define MAX_DEFINITIONS 256

typedef struct Definition {
    EntityBody body;
    char name[65];
    int model, spawn, step, remove, load, metadata;
    bool save;
} Definition;
typedef struct Handle { int id; uint64_t generation; } Handle;
static Definition definitions[MAX_DEFINITIONS];
static int definitionCount;

Entity *LuaEntities_Check(lua_State *state, int index) {
    Handle *h = luaL_checkudata(state, index, ENTITY_HANDLE);
    Entity *e = serverWorld.entities && h->id >= 0 && h->id < WORLD_MAX_ENTITIES
        ? &serverWorld.entities[h->id] : NULL;
    if (!e || !e->active || e->pendingRemoval || e->generation != h->generation)
        luaL_error(state, "entity has been removed");
    return e;
}
static Entity *Check(lua_State *state) { return LuaEntities_Check(state, 1); }
static void PushHandle(Entity *e) {
    Handle *h = lua_newuserdata(L, sizeof(*h));
    *h = (Handle){e->id, e->generation};
    luaL_setmetatable(L, ENTITY_HANDLE);
}
static Vector3 ReadVector(lua_State *state, int index, float limit) {
    index = lua_absindex(state, index);
    luaL_checktype(state, index, LUA_TTABLE);
    float values[3];
    const char *names[] = {"x", "y", "z"};
    for (int i = 0; i < 3; i++) {
        lua_getfield(state, index, names[i]);
        double value = luaL_checknumber(state, -1);
        if (!isfinite(value) || fabs(value) > limit) luaL_error(state, "coordinate is outside the supported range");
        values[i] = value;
        lua_pop(state, 1);
    }
    return (Vector3){values[0], values[1], values[2]};
}
static int PushVector(lua_State *state, Vector3 v) {
    lua_createtable(state, 0, 3);
    lua_pushnumber(state, v.x); lua_setfield(state, -2, "x");
    lua_pushnumber(state, v.y); lua_setfield(state, -2, "y");
    lua_pushnumber(state, v.z); lua_setfield(state, -2, "z");
    return 1;
}
static int GetPosition(lua_State *state) { return PushVector(state, Check(state)->position); }
static int GetRotation(lua_State *state) {
    Vector3 r = Check(state)->rotation;
    return PushVector(state, r);
}
static int SetPosition(lua_State *state) {
    Entity *e = Check(state);
    if (e->body.enabled) return luaL_error(state, "use teleport or velocity for a physics entity");
    Vector3 p = ReadVector(state, 2, 33554430.0f);
    ServerWorld_TeleportEntity(e->id, p, e->rotation);
    return 0;
}
static int SetRotation(lua_State *state) {
    Entity *e = Check(state);
    Vector3 r = ReadVector(state, 2, 1000000.0f);
    e->rotation = r;
    e->dirty = true;
    return 0;
}
static EntityBody ReadBody(lua_State *state, int index) {
    index = lua_absindex(state, index);
    luaL_checktype(state, index, LUA_TTABLE);
    EntityBody body = EntityBody_Default();
    body.enabled = true;
    lua_getfield(state, index, "enabled");
    if (!lua_isnil(state, -1)) {
        luaL_checktype(state, -1, LUA_TBOOLEAN);
        body.enabled = lua_toboolean(state, -1);
    }
    lua_pop(state, 1);
    lua_getfield(state, index, "min");
    if (!lua_isnil(state, -1)) body.localBounds.min = ReadVector(state, -1, 4);
    lua_pop(state, 1);
    lua_getfield(state, index, "max");
    if (!lua_isnil(state, -1)) body.localBounds.max = ReadVector(state, -1, 4);
    lua_pop(state, 1);
    const char *names[] = {"gravity_scale", "ground_friction", "restitution"};
    float *values[] = {&body.gravityScale, &body.groundFriction, &body.restitution};
    for (int i = 0; i < 3; i++) {
        lua_getfield(state, index, names[i]);
        if (!lua_isnil(state, -1)) *values[i] = luaL_checknumber(state, -1);
        lua_pop(state, 1);
    }
    if (!EntityBody_Validate(&body)) luaL_error(state, "invalid entity body bounds or physics parameters");
    return body;
}
static int SetBody(lua_State *state) {
    Entity *entity = Check(state);
    if (!ServerPhysics_SetBody(entity, ReadBody(state, 2))) return luaL_error(state, "cannot change this entity body");
    return 0;
}
static int GetVelocity(lua_State *state) { return PushVector(state, Check(state)->body.velocity); }
static int SetVelocity(lua_State *state) {
    if (!ServerPhysics_SetVelocity(Check(state), ReadVector(state, 2, 100)))
        return luaL_error(state, "velocity requires an enabled non-player body");
    return 0;
}
static int ApplyImpulse(lua_State *state) {
    if (!ServerPhysics_ApplyImpulse(Check(state), ReadVector(state, 2, 100)))
        return luaL_error(state, "impulse requires an enabled body and resulting velocity within 100 blocks/s");
    return 0;
}
static int IsGrounded(lua_State *state) { lua_pushboolean(state, Check(state)->body.grounded); return 1; }
static int Teleport(lua_State *state) {
    Entity *entity = Check(state);
    ServerWorld_TeleportEntity(entity->id, ReadVector(state, 2, 1000000), entity->rotation);
    return 0;
}
static int Remove(lua_State *state) { ServerWorld_RemoveEntity(Check(state)->id); return 0; }
static int GetId(lua_State *state) { lua_pushinteger(state, Check(state)->id); return 1; }
static int GetMetadata(lua_State *state) {
    Entity *e = Check(state);
    return LuaMetadata_Get(state, definitions[e->definitionId].metadata, &e->metadata, 2);
}
static int SetMetadata(lua_State *state) {
    Entity *e = Check(state);
    return LuaMetadata_Set(state, definitions[e->definitionId].metadata, &e->metadata, 2, 3);
}
static int ResetMetadata(lua_State *state) {
    Entity *e = Check(state);
    return LuaMetadata_Set(state, definitions[e->definitionId].metadata, &e->metadata, 2, 0);
}
static int GetInventory(lua_State *state) {
    Entity *e = Check(state);
    return LuaMetadata_Inventory(state, definitions[e->definitionId].metadata, 1, 2);
}
static int SetModel(lua_State *state) {
    Entity *e = Check(state);
    int model = LuaModels_Resolve(2, false);
    if (model < 0 || model > 255 || !ServerWorld_SetEntityModel(e->id, (int)model))
        return luaL_error(state, "model is not defined");
    return 0;
}
static int IsValid(lua_State *state) {
    Handle *h = luaL_checkudata(state, 1, ENTITY_HANDLE);
    Entity *e = serverWorld.entities && h->id >= 0 && h->id < WORLD_MAX_ENTITIES ? &serverWorld.entities[h->id] : NULL;
    lua_pushboolean(state, e && e->active && !e->pendingRemoval && e->generation == h->generation);
    return 1;
}
static bool Call(Entity *e, int ref, float dt, bool step) {
    if (ref == LUA_NOREF) return true;
    int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, e->scriptRef);
    if (step) lua_pushnumber(L, dt);
    bool ok = lua_pcall(L, step ? 2 : 1, 0, 0) == LUA_OK;
    if (!ok) TraceLog(LOG_WARNING, "Entity %s (%i): %s", definitions[e->definitionId].name, e->id, lua_tostring(L, -1));
    lua_settop(L, top);
    return ok;
}
int LuaEntities_Register(void) {
    size_t length;
    const char *name = luaL_checklstring(L, 1, &length);
    if (!length || length > 64 || memchr(name, 0, length)) return luaL_error(L, "invalid entity name");
    luaL_checktype(L, 2, LUA_TTABLE);
    if (definitionCount == MAX_DEFINITIONS) return luaL_error(L, "entity registry is full");
    for (int i = 0; i < definitionCount; i++)
        if (!strcmp(name, definitions[i].name)) return luaL_error(L, "entity name is already registered");
    lua_getfield(L, 2, "model");
    int model = LuaModels_Resolve(-1, false);
    if (model < 0 || model > 255 || (model && !serverWorld.modelDefinitions[model]))
        return luaL_error(L, "model is not defined");
    lua_pop(L, 1);
    const char *callbacks[] = {"on_spawn", "on_step", "on_remove", "on_load"};
    // Validate all callbacks before taking registry references.
    for (int i = 0; i < 4; i++) {
        lua_getfield(L, 2, callbacks[i]);
        if (!lua_isnil(L, -1)) luaL_checktype(L, -1, LUA_TFUNCTION);
        lua_pop(L, 1);
    }
    Definition d = {.model = model, .save = true};
    lua_getfield(L, 2, "save");
    if (!lua_isnil(L, -1)) {
        luaL_checktype(L, -1, LUA_TBOOLEAN);
        d.save = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);
    d.body = EntityBody_Default();
    lua_getfield(L, 2, "body");
    if (!lua_isnil(L, -1)) d.body = ReadBody(L, -1);
    lua_pop(L, 1);
    memcpy(d.name, name, length + 1);
    d.metadata = LuaMetadata_Register(L, 2);
    int refs[4];
    for (int i = 0; i < 4; i++) {
        lua_getfield(L, 2, callbacks[i]);
        if (lua_isnil(L, -1)) { lua_pop(L, 1); refs[i] = LUA_NOREF; }
        else refs[i] = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    d.spawn = refs[0]; d.step = refs[1]; d.remove = refs[2]; d.load = refs[3];
    definitions[definitionCount++] = d;
    return 0;
}
int LuaEntities_Spawn(void) {
    const char *name = luaL_checkstring(L, 1);
    Vector3 position = ReadVector(L, 2, 33554430.0f);
    int definition = -1;
    for (int i = 0; i < definitionCount; i++) if (!strcmp(name, definitions[i].name)) definition = i;
    if (definition < 0) return luaL_error(L, "entity is not registered");
    int model = definitions[definition].model;
    if (model && !serverWorld.modelDefinitions[model]) return luaL_error(L, "entity model has been removed");
    int id = ServerWorld_AddEntity(2, model, position, -1);
    if (id < 0) return luaL_error(L, "cannot spawn entity: world unavailable or entity limit reached");
    Entity *e = &serverWorld.entities[id];
    e->definitionId = definition;
    if (!ServerPhysics_SetBody(e, definitions[definition].body)) {
        e->definitionId = -1;
        ServerWorld_RemoveEntity(id);
        return luaL_error(L, "physics entities must spawn within one million blocks of the origin");
    }
    lua_newtable(L);
    PushHandle(e); lua_setfield(L, -2, "object");
    e->scriptRef = luaL_ref(L, LUA_REGISTRYINDEX);
    if (!Call(e, definitions[definition].spawn, 0, false)) {
        ServerWorld_RemoveEntity(id);
        return luaL_error(L, "entity on_spawn failed");
    }
    PushHandle(e);
    return 1;
}
void LuaEntities_Step(Entity *e, float dt) {
    if (e->definitionId < 0) return;
    if (!Call(e, definitions[e->definitionId].step, dt, true)) ServerWorld_RemoveEntity(e->id);
}
void LuaEntities_Remove(Entity *e) {
    if (e->definitionId < 0) return;
    Call(e, definitions[e->definitionId].remove, 0, false);
    LuaEntities_Detach(e);
}
void LuaEntities_Detach(Entity *e) {
    if (e->definitionId < 0) return;
    luaL_unref(L, LUA_REGISTRYINDEX, e->scriptRef);
    e->scriptRef = LUA_NOREF;
}
const char *LuaEntities_Name(int definition) {
    return definition >= 0 && definition < definitionCount ? definitions[definition].name : NULL;
}
int LuaEntities_Find(const char *name) {
    for (int i = 0; i < definitionCount; i++) if (!strcmp(name, definitions[i].name)) return i;
    return -1;
}
int LuaEntities_MetadataSchema(int definition) {
    return definition >= 0 && definition < definitionCount ? definitions[definition].metadata : -1;
}
bool LuaEntities_ShouldSave(int definition) {
    return definition >= 0 && definition < definitionCount && definitions[definition].save;
}
int LuaEntities_Restore(int definition, Vector3 position) {
    if (definition < 0 || definition >= definitionCount) return -1;
    int id = ServerWorld_AddEntity(2, definitions[definition].model, position, -1);
    if (id < 0) return -1;
    Entity *e = &serverWorld.entities[id];
    e->definitionId = definition;
    e->body = definitions[definition].body;
    lua_newtable(L);
    PushHandle(e); lua_setfield(L, -2, "object");
    e->scriptRef = luaL_ref(L, LUA_REGISTRYINDEX);
    return id;
}
void LuaEntities_Loaded(Entity *e) {
    if (e->definitionId >= 0 && !Call(e, definitions[e->definitionId].load, 0, false))
        TraceLog(LOG_WARNING, "Entity on_load failed; saved entity retained");
}
void LuaEntities_Init(void) {
    static const luaL_Reg methods[] = {
        {"get_id", GetId}, {"is_valid", IsValid}, {"get_position", GetPosition},
        {"get_metadata", GetMetadata}, {"set_metadata", SetMetadata},
        {"reset_metadata", ResetMetadata}, {"get_inventory", GetInventory},
        {"set_position", SetPosition}, {"get_rotation", GetRotation},
        {"set_rotation", SetRotation}, {"set_model", SetModel}, {"remove", Remove},
        {"set_body", SetBody}, {"get_velocity", GetVelocity}, {"set_velocity", SetVelocity},
        {"apply_impulse", ApplyImpulse}, {"is_grounded", IsGrounded}, {"teleport", Teleport}, {NULL, NULL}
    };
    luaL_newmetatable(L, ENTITY_HANDLE);
    lua_newtable(L); luaL_setfuncs(L, methods, 0); lua_setfield(L, -2, "__index");
    lua_pushliteral(L, ENTITY_HANDLE); lua_setfield(L, -2, "__metatable");
    lua_pop(L, 1);
}
void LuaEntities_Shutdown(void) {
    for (int i = 0; i < definitionCount; i++) {
        luaL_unref(L, LUA_REGISTRYINDEX, definitions[i].spawn);
        luaL_unref(L, LUA_REGISTRYINDEX, definitions[i].step);
        luaL_unref(L, LUA_REGISTRYINDEX, definitions[i].remove);
        luaL_unref(L, LUA_REGISTRYINDEX, definitions[i].load);
    }
    definitionCount = 0;
}
