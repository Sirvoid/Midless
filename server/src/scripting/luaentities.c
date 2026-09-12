#include "../entityregistry.h"
#include "../scripthooks.h"
#include "luaentitytexture.h"
#include "luatextcolors.h"
#include "minilua.h"
#include <math.h>
#include <string.h>
#include "luaentities.h"
#include "luamobs.h"
#include "luadamage.h"
#include "../mobs.h"
#include "../entitytexture.h"
#include "../world/textures.h"
#include "luamodels.h"
#include "luametadata.h"
#include "../world/world.h"
#include "../entityphysics.h"
#include "../textcolors.h"

extern lua_State *L;
#define ENTITY_HANDLE "midless.Entity"
#define MAX_DEFINITIONS 256

typedef struct Handle {
    int id;
    uint64_t generation;
} Handle;
typedef struct EntityCallbacks {
    int spawn, step, remove, load, unload, damage, death;
} EntityCallbacks;
static EntityCallbacks callbacks[MAX_DEFINITIONS];
static int instances[WORLD_MAX_ENTITIES];

Entity *LuaEntities_Test(lua_State *state, int index) {
    Handle *h = luaL_testudata(state, index, ENTITY_HANDLE);
    Entity *e = h && serverWorld.entities && h->id >= 0 && h->id < WORLD_MAX_ENTITIES
                    ? &serverWorld.entities[h->id]
                    : NULL;
    return e && e->active && !e->pendingRemoval && e->generation == h->generation ? e : NULL;
}

Entity *LuaEntities_Check(lua_State *state, int index) {
    Handle *h = luaL_checkudata(state, index, ENTITY_HANDLE);
    Entity *e = serverWorld.entities && h->id >= 0 && h->id < WORLD_MAX_ENTITIES
                    ? &serverWorld.entities[h->id]
                    : NULL;
    if (!e || !e->active || e->pendingRemoval || e->generation != h->generation)
        luaL_error(state, "entity has been removed");
    return e;
}
static Entity *Check(lua_State *state) {
    return LuaEntities_Check(state, 1);
}
static int SetTexture(lua_State *state) {
    return LuaEntityTexture_Set(state, Check(state));
}
static int GetTexture(lua_State *state) {
    return LuaEntityTexture_Get(state, Check(state));
}
static int SetNametag(lua_State *state) {
    return LuaNametag_Set(state, Check(state));
}
void LuaEntities_Push(Entity *e) {
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
        if (!isfinite(value) || fabs(value) > limit)
            luaL_error(state, "coordinate is outside the supported range");
        values[i] = value;
        lua_pop(state, 1);
    }
    return (Vector3){values[0], values[1], values[2]};
}
static int PushVector(lua_State *state, Vector3 v) {
    lua_createtable(state, 0, 3);
    lua_pushnumber(state, v.x);
    lua_setfield(state, -2, "x");
    lua_pushnumber(state, v.y);
    lua_setfield(state, -2, "y");
    lua_pushnumber(state, v.z);
    lua_setfield(state, -2, "z");
    return 1;
}
static int GetPosition(lua_State *state) {
    return PushVector(state, Check(state)->position);
}
static int GetRotation(lua_State *state) {
    Vector3 r = Check(state)->rotation;
    return PushVector(state, r);
}
static int SetPosition(lua_State *state) {
    Entity *e = Check(state);
    if (e->body.enabled)
        return luaL_error(state, "use teleport or velocity for a physics entity");
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
    if (!lua_isnil(state, -1))
        body.localBounds.min = ReadVector(state, -1, 4);
    lua_pop(state, 1);
    lua_getfield(state, index, "max");
    if (!lua_isnil(state, -1))
        body.localBounds.max = ReadVector(state, -1, 4);
    lua_pop(state, 1);
    const char *names[] = {"gravity_scale", "ground_friction", "restitution"};
    float *values[] = {&body.gravityScale, &body.groundFriction, &body.restitution};
    for (int i = 0; i < 3; i++) {
        lua_getfield(state, index, names[i]);
        if (!lua_isnil(state, -1))
            *values[i] = luaL_checknumber(state, -1);
        lua_pop(state, 1);
    }
    if (!EntityBody_Validate(&body))
        luaL_error(state, "invalid entity body bounds or physics parameters");
    return body;
}
static int SetBody(lua_State *state) {
    Entity *entity = Check(state);
    if (!ServerPhysics_SetBody(entity, ReadBody(state, 2)))
        return luaL_error(state, "cannot change this entity body");
    return 0;
}
static int GetVelocity(lua_State *state) {
    return PushVector(state, Check(state)->body.velocity);
}
static int SetVelocity(lua_State *state) {
    if (!ServerPhysics_SetVelocity(Check(state), ReadVector(state, 2, 100)))
        return luaL_error(state, "velocity requires an enabled non-player body");
    return 0;
}
static int ApplyImpulse(lua_State *state) {
    if (!ServerPhysics_ApplyImpulse(Check(state), ReadVector(state, 2, 100)))
        return luaL_error(
            state, "impulse requires an enabled body and resulting velocity within 100 blocks/s");
    return 0;
}
static int IsGrounded(lua_State *state) {
    lua_pushboolean(state, Check(state)->body.grounded);
    return 1;
}
static int GetName(lua_State *state) {
    lua_pushstring(state, ServerEntities_Name(Check(state)->definitionId));
    return 1;
}
static int GetHP(lua_State *state) {
    lua_pushinteger(state, Check(state)->hp);
    return 1;
}
static int SetHP(lua_State *state) {
    Entity *e = Check(state);
    lua_Integer hp = luaL_checkinteger(state, 2);
    if (e->dead || e->damageBusy || hp < 1 || hp > e->maxHp)
        return luaL_error(state, "hp must be 1..max_hp; use damage for death");
    e->hp = hp;
    return 0;
}
static int Move(lua_State *state) {
    Entity *e = Check(state);
    Vector3 direction = ReadVector(state, 2, 1000000);
    float speed = luaL_checknumber(state, 3), acceleration = luaL_optnumber(state, 4, 32);
    if (!ServerPhysics_Move(e, direction, speed, acceleration))
        return luaL_error(state,
                          "move requires a physics body, speed 0..20 and acceleration >0..100");
    return 0;
}
static int Jump(lua_State *state) {
    lua_pushboolean(state, ServerPhysics_Jump(Check(state), luaL_optnumber(state, 2, 7)));
    return 1;
}
static int IsRecovering(lua_State *state) {
    lua_pushboolean(state, Check(state)->recovering);
    return 1;
}
static int AdjustDamage(Entity *e, int amount, void *context) {
    lua_State *state = context;
    EntityCallbacks *d = &callbacks[e->definitionId];
    if (d->damage >= 0) {
        lua_rawgeti(state, LUA_REGISTRYINDEX, d->damage);
        lua_rawgeti(state, LUA_REGISTRYINDEX, instances[e->id]);
        lua_pushinteger(state, amount);
        lua_pushvalue(state, 3);
        if (lua_pcall(state, 3, 1, 0) != LUA_OK) {
            TraceLog(LOG_WARNING, "Entity on_damage: %s", lua_tostring(state, -1));
            amount = 0;
        } else if (lua_isboolean(state, -1) && !lua_toboolean(state, -1))
            amount = 0;
        else if (!lua_isnil(state, -1)) {
            if (!lua_isinteger(state, -1) || lua_tointeger(state, -1) < 0 ||
                lua_tointeger(state, -1) > 65535) {
                TraceLog(LOG_WARNING, "Entity on_damage must return nil, false or damage 0..65535");
                amount = 0;
            } else
                amount = lua_tointeger(state, -1);
        }
        lua_pop(state, 1);
    }
    return amount;
}

static void NotifyDeath(Entity *e, void *context) {
    lua_State *state = context;
    EntityCallbacks *d = &callbacks[e->definitionId];
    if (d->death >= 0) {
        lua_rawgeti(state, LUA_REGISTRYINDEX, d->death);
        lua_rawgeti(state, LUA_REGISTRYINDEX, instances[e->id]);
        lua_pushvalue(state, 3);
        if (lua_pcall(state, 2, 0, 0) != LUA_OK) {
            TraceLog(LOG_WARNING, "Entity on_death: %s", lua_tostring(state, -1));
            lua_pop(state, 1);
        }
    }
}

static int Damage(lua_State *state) {
    Entity *e = Check(state);
    lua_Integer amount = luaL_checkinteger(state, 2);
    if (amount < 0 || amount > 65535)
        return luaL_error(state, "damage must be 0..65535");
    if (lua_isnoneornil(state, 3)) {
        lua_settop(state, 2);
        lua_newtable(state);
    }
    luaL_checktype(state, 3, LUA_TTABLE);
    Vector3 impulse = LuaDamage_Impulse(state, 3, e);
    EntityDamageHooks hooks = {AdjustDamage, NotifyDeath, state};
    int applied = ServerEntities_Damage(e, amount, impulse, &hooks);
    lua_pushinteger(state, applied);
    return 1;
}
static int Teleport(lua_State *state) {
    Entity *entity = Check(state);
    ServerWorld_TeleportEntity(entity->id, ReadVector(state, 2, 1000000), entity->rotation);
    return 0;
}
static int Remove(lua_State *state) {
    ServerWorld_RemoveEntity(Check(state)->id);
    return 0;
}
static int GetId(lua_State *state) {
    lua_pushinteger(state, Check(state)->id);
    return 1;
}
static int GetMetadata(lua_State *state) {
    Entity *e = Check(state);
    return LuaMetadata_Get(state, ServerEntities_Get(e->definitionId)->metadata, &e->metadata, 2);
}
static int SetMetadata(lua_State *state) {
    Entity *e = Check(state);
    return LuaMetadata_Set(state, ServerEntities_Get(e->definitionId)->metadata, &e->metadata, 2,
                           3);
}
static int ResetMetadata(lua_State *state) {
    Entity *e = Check(state);
    return LuaMetadata_Set(state, ServerEntities_Get(e->definitionId)->metadata, &e->metadata, 2,
                           0);
}
static int GetInventory(lua_State *state) {
    Entity *e = Check(state);
    return LuaMetadata_Inventory(state, ServerEntities_Get(e->definitionId)->metadata, 1, 2);
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
    Entity *e = serverWorld.entities && h->id >= 0 && h->id < WORLD_MAX_ENTITIES
                    ? &serverWorld.entities[h->id]
                    : NULL;
    lua_pushboolean(state, e && e->active && !e->pendingRemoval && e->generation == h->generation);
    return 1;
}
static bool Call(Entity *e, int ref, float dt, bool step) {
    if (ref == LUA_NOREF)
        return true;
    int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, instances[e->id]);
    if (step)
        lua_pushnumber(L, dt);
    bool ok = lua_pcall(L, step ? 2 : 1, 0, 0) == LUA_OK;
    if (!ok)
        TraceLog(LOG_WARNING, "Entity %s (%i): %s", ServerEntities_Get(e->definitionId)->name,
                 e->id, lua_tostring(L, -1));
    lua_settop(L, top);
    return ok;
}
int LuaEntities_Register(void) {
    size_t length;
    const char *name = luaL_checklstring(L, 1, &length);
    if (!length || length > 64 || memchr(name, 0, length))
        return luaL_error(L, "invalid entity name");
    luaL_checktype(L, 2, LUA_TTABLE);
    if (ServerEntities_Count() == MAX_DEFINITIONS)
        return luaL_error(L, "entity registry is full");
    for (int i = 0; i < ServerEntities_Count(); i++)
        if (!strcmp(name, ServerEntities_Get(i)->name))
            return luaL_error(L, "entity name is already registered");
    lua_getfield(L, 2, "model");
    int model = LuaModels_Resolve(-1, false);
    if (model < 0 || model > 255 || (model && !serverWorld.modelDefinitions[model]))
        return luaL_error(L, "model is not defined");
    lua_pop(L, 1);
    const char *callbackNames[] = {"on_spawn",  "on_step",   "on_remove", "on_load",
                                   "on_unload", "on_damage", "on_death"};
    // Validate all callbacks before taking registry references.
    for (int i = 0; i < 7; i++) {
        lua_getfield(L, 2, callbackNames[i]);
        if (!lua_isnil(L, -1))
            luaL_checktype(L, -1, LUA_TFUNCTION);
        lua_pop(L, 1);
    }
    EntityDefinition d = {.model = model, .save = true};
    lua_getfield(L, 2, "texture");
    if (!lua_isnil(L, -1)) {
        size_t length;
        const char *name = luaL_checklstring(L, -1, &length);
        if (!length || length > 64 || memchr(name, 0, length) || ServerTextures_Find(name) < 0)
            return luaL_error(L, "entity texture is not registered");
        strcpy(d.texture, name);
    }
    lua_pop(L, 1);
    lua_getfield(L, 2, "hp");
    lua_Integer hp = luaL_optinteger(L, -1, 0);
    if (hp < 0 || hp > 65535)
        return luaL_error(L, "entity hp must be 0..65535 (0 disables damage)");
    d.hp = hp;
    lua_pop(L, 1);
    lua_getfield(L, 2, "population_group");
    if (!lua_isnil(L, -1)) {
        size_t size;
        const char *group = luaL_checklstring(L, -1, &size);
        if (!size || size > 64 || memchr(group, 0, size))
            return luaL_error(L, "invalid population group");
        memcpy(d.group, group, size + 1);
    }
    lua_pop(L, 1);
    lua_getfield(L, 2, "despawn");
    if (!lua_isnil(L, -1)) {
        luaL_checktype(L, -1, LUA_TTABLE);
        lua_getfield(L, -1, "distance");
        d.despawnDistance = luaL_checknumber(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, -1, "delay");
        d.despawnDelay = luaL_optnumber(L, -1, 30);
        lua_pop(L, 1);
        if (!isfinite(d.despawnDistance) || d.despawnDistance <= 0 || d.despawnDistance > 1000000 ||
            !isfinite(d.despawnDelay) || d.despawnDelay < 0 || d.despawnDelay > 86400)
            return luaL_error(L, "invalid despawn distance or delay");
    }
    lua_pop(L, 1);
    lua_getfield(L, 2, "save");
    if (!lua_isnil(L, -1)) {
        luaL_checktype(L, -1, LUA_TBOOLEAN);
        d.save = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);
    d.body = EntityBody_Default();
    lua_getfield(L, 2, "body");
    if (!lua_isnil(L, -1))
        d.body = ReadBody(L, -1);
    lua_pop(L, 1);
    memcpy(d.name, name, length + 1);
    d.metadata = LuaMetadata_Register(L, 2);
    int refs[7];
    for (int i = 0; i < 7; i++) {
        lua_getfield(L, 2, callbackNames[i]);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            refs[i] = LUA_NOREF;
        } else
            refs[i] = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    callbacks[ServerEntities_Count()].spawn = refs[0];
    callbacks[ServerEntities_Count()].step = refs[1];
    callbacks[ServerEntities_Count()].remove = refs[2];
    callbacks[ServerEntities_Count()].load = refs[3];
    callbacks[ServerEntities_Count()].unload = refs[4];
    callbacks[ServerEntities_Count()].damage = refs[5];
    callbacks[ServerEntities_Count()].death = refs[6];
    ServerEntities_Register(&d);
    return 0;
}
int LuaEntities_Spawn(void) {
    const char *name = luaL_checkstring(L, 1);
    Vector3 position = ReadVector(L, 2, 33554430.0f);
    int definition = -1;
    for (int i = 0; i < ServerEntities_Count(); i++)
        if (!strcmp(name, ServerEntities_Get(i)->name))
            definition = i;
    if (definition < 0)
        return luaL_error(L, "entity is not registered");
    int id = ServerEntities_Create(definition, position, true);
    if (id < 0)
        return luaL_error(
            L, "cannot spawn entity: invalid model, body position, or entity limit reached");
    Entity *e = &serverWorld.entities[id];
    lua_newtable(L);
    LuaEntities_Push(e);
    lua_setfield(L, -2, "object");
    instances[e->id] = luaL_ref(L, LUA_REGISTRYINDEX);
    if (!Call(e, callbacks[definition].spawn, 0, false)) {
        ServerWorld_RemoveEntity(id);
        return luaL_error(L, "entity on_spawn failed");
    }
    LuaEntities_Push(e);
    return 1;
}
bool ScriptHooks_EntitiesTrySpawn(int definition, Vector3 position) {
    const char *name = ServerEntities_Name(definition);
    if (!name)
        return false;
    int top = lua_gettop(L);
    lua_pushcfunction(L, (lua_CFunction)LuaEntities_Spawn);
    lua_pushstring(L, name);
    PushVector(L, position);
    bool ok = lua_pcall(L, 2, 1, 0) == LUA_OK;
    if (!ok)
        TraceLog(LOG_WARNING, "Natural spawn failed: %s", lua_tostring(L, -1));
    lua_settop(L, top);
    return ok;
}
void ScriptHooks_EntitiesStep(Entity *e, float dt) {
    if (e->definitionId < 0)
        return;
    if (!Call(e, callbacks[e->definitionId].step, dt, true))
        ServerWorld_RemoveEntity(e->id);
}
void ScriptHooks_EntitiesRemove(Entity *e) {
    if (e->definitionId < 0)
        return;
    Call(e, callbacks[e->definitionId].remove, 0, false);
    ScriptHooks_EntitiesDetach(e);
}
void ScriptHooks_EntitiesDetach(Entity *e) {
    ServerMobs_Detach(e);
    if (e->definitionId < 0)
        return;
    luaL_unref(L, LUA_REGISTRYINDEX, instances[e->id]);
    instances[e->id] = LUA_NOREF;
}
void ScriptHooks_EntitiesUnload(Entity *e) {
    if (e->definitionId < 0)
        return;
    Call(e, callbacks[e->definitionId].unload, 0, false);
    ScriptHooks_EntitiesDetach(e);
}
int ScriptHooks_EntitiesRestore(int definition, Vector3 position) {
    int id = ServerEntities_Create(definition, position, false);
    if (id < 0)
        return -1;
    Entity *e = &serverWorld.entities[id];
    lua_newtable(L);
    LuaEntities_Push(e);
    lua_setfield(L, -2, "object");
    instances[e->id] = luaL_ref(L, LUA_REGISTRYINDEX);
    return id;
}
void ScriptHooks_EntitiesLoaded(Entity *e) {
    if (e->definitionId >= 0 && !Call(e, callbacks[e->definitionId].load, 0, false))
        TraceLog(LOG_WARNING, "Entity on_load failed; saved entity retained");
}
void LuaEntities_Init(void) {
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++)
        instances[i] = LUA_NOREF;
    static const luaL_Reg methods[] = {{"set_texture", SetTexture},
                                       {"get_texture", GetTexture},
                                       {"follow_ground_path", LuaMobs_Follow},
                                       {"wander_goal", LuaMobs_Wander},
                                       {"steer_toward", LuaMobs_Steer},
                                       {"try_teleport", LuaMobs_Teleport},
                                       {"get_name", GetName},
                                       {"get_hp", GetHP},
                                       {"set_hp", SetHP},
                                       {"damage", Damage},
                                       {"set_move_direction", Move},
                                       {"jump", Jump},
                                       {"is_recovering", IsRecovering},
                                       {"set_nametag", SetNametag},
                                       {"get_id", GetId},
                                       {"is_valid", IsValid},
                                       {"get_position", GetPosition},
                                       {"get_metadata", GetMetadata},
                                       {"set_metadata", SetMetadata},
                                       {"reset_metadata", ResetMetadata},
                                       {"get_inventory", GetInventory},
                                       {"set_position", SetPosition},
                                       {"get_rotation", GetRotation},
                                       {"set_rotation", SetRotation},
                                       {"set_model", SetModel},
                                       {"remove", Remove},
                                       {"set_body", SetBody},
                                       {"get_velocity", GetVelocity},
                                       {"set_velocity", SetVelocity},
                                       {"apply_impulse", ApplyImpulse},
                                       {"is_grounded", IsGrounded},
                                       {"teleport", Teleport},
                                       {NULL, NULL}};
    luaL_newmetatable(L, ENTITY_HANDLE);
    lua_newtable(L);
    luaL_setfuncs(L, methods, 0);
    lua_setfield(L, -2, "__index");
    lua_pushliteral(L, ENTITY_HANDLE);
    lua_setfield(L, -2, "__metatable");
    lua_pop(L, 1);
}
void LuaEntities_Shutdown(void) {
    for (int i = 0; i < ServerEntities_Count(); i++) {
        luaL_unref(L, LUA_REGISTRYINDEX, callbacks[i].spawn);
        luaL_unref(L, LUA_REGISTRYINDEX, callbacks[i].step);
        luaL_unref(L, LUA_REGISTRYINDEX, callbacks[i].remove);
        luaL_unref(L, LUA_REGISTRYINDEX, callbacks[i].load);
        luaL_unref(L, LUA_REGISTRYINDEX, callbacks[i].unload);
        luaL_unref(L, LUA_REGISTRYINDEX, callbacks[i].damage);
        luaL_unref(L, LUA_REGISTRYINDEX, callbacks[i].death);
    }
    ServerEntities_Reset();
}

int LuaEntities_Instance(const Entity *entity) {
    return instances[entity->id];
}
