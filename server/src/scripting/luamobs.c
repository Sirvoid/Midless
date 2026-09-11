#include <math.h>
#include <string.h>
#include "luamobs.h"
#include "luabindings.h"
#include "../mobs.h"
#include "../entityphysics.h"
#include "../worldquery.h"
#include "../world/world.h"

extern lua_State *L;
typedef struct MobDefinition {
    bool registered, lineOfSight, moveDuringRecovery;
    int brain, movement, attack;
    float interval, range, cooldown;
} MobDefinition;
static MobDefinition definitions[256];

static float Number(int table, const char *name, double fallback, double min, double max) {
    lua_getfield(L,table,name);
    double n = luaL_optnumber(L,-1,fallback);
    if (!isfinite(n) || n<min || n>max) luaL_error(L,"%s must be %f..%f",name,(double)min,(double)max);
    lua_pop(L,1); return n;
}
static bool Boolean(int table, const char *name, bool fallback) {
    lua_getfield(L,table,name);
    bool result = fallback;
    if (!lua_isnil(L,-1)) { luaL_checktype(L,-1,LUA_TBOOLEAN); result = lua_toboolean(L,-1); }
    lua_pop(L,1); return result;
}
static Vector3 Position(int index) {
    index = lua_absindex(L,index); luaL_checktype(L,index,LUA_TTABLE);
    const char *names[] = {"x","y","z"}; float values[3];
    for (int i = 0; i<3; i++) {
        lua_getfield(L,index,names[i]); double n = luaL_checknumber(L,-1);
        if (!isfinite(n) || fabs(n)>999900) luaL_error(L,"invalid mob position");
        values[i] = n; lua_pop(L,1);
    }
    return (Vector3){values[0],values[1],values[2]};
}
static void PushPosition(Vector3 p) {
    lua_createtable(L,0,3);
    lua_pushnumber(L,p.x); lua_setfield(L,-2,"x");
    lua_pushnumber(L,p.y); lua_setfield(L,-2,"y");
    lua_pushnumber(L,p.z); lua_setfield(L,-2,"z");
}
static void PushTarget(Entity *e) {
    if (!e) lua_pushnil(L);
    else if (e->ownerPlayerId>=0) LuaBindings_PushPlayer(serverWorld.players[e->ownerPlayerId]);
    else LuaEntities_Push(e);
}
static void PushIntent(MobState *s) {
    lua_createtable(L,0,3);
    if (s->hasGoal) { PushPosition(s->goal); lua_setfield(L,-2,"goal"); }
    Entity *target = ServerMobs_Target(s);
    PushTarget(target); lua_setfield(L,-2,"target");
    lua_pushboolean(L,s->attack && target); lua_setfield(L,-2,"attack");
}
static void ReadIntent(MobState *s) {
    s->hasGoal = s->attack = false; s->targetId = -1;
    if (lua_isnil(L,-1)) return;
    luaL_checktype(L,-1,LUA_TTABLE);
    int table = lua_gettop(L);
    lua_getfield(L,table,"goal");
    if (!lua_isnil(L,-1)) { s->goal = Position(-1); s->hasGoal = true; }
    lua_pop(L,1);
    lua_getfield(L,table,"target");
    if (!lua_isnil(L,-1)) {
        Entity *target = LuaEntities_Test(L,-1);
        if (!target) target = LuaBindings_TestPlayerEntity(L,-1);
        if (target) { s->targetId = target->id; s->targetGeneration = target->generation; }
        else if (!luaL_testudata(L,-1,"midless.Entity") && !luaL_testudata(L,-1,"midless.Player"))
            luaL_error(L,"intent.target must be an entity or player handle");
    }
    lua_pop(L,1);
    s->attack = Boolean(table,"attack",false);
}
int LuaMobs_Register(void) {
    const char *name = luaL_checkstring(L,1);
    luaL_checktype(L,2,LUA_TTABLE);
    lua_settop(L,2);
    MobDefinition d = {.registered=true};
    const char *sections[] = {"brain","movement","attack"};
    const char *callbacks[] = {"update","update","perform"};
    for (int i = 0; i<3; i++) {
        lua_getfield(L,2,sections[i]); luaL_checktype(L,-1,LUA_TTABLE);
        lua_getfield(L,-1,callbacks[i]); luaL_checktype(L,-1,LUA_TFUNCTION); lua_pop(L,1);
        int table = lua_gettop(L);
        if (i==0) d.interval = Number(table,"interval",0.2,1.0/60,60);
        if (i==1) d.moveDuringRecovery = Boolean(table,"during_recovery",false);
        if (i==2) {
            d.range = Number(table,"range",1.8f,0,128);
            d.cooldown = Number(table,"cooldown",1,1.0/60,60);
            d.lineOfSight = Boolean(table,"requires_line_of_sight",true);
        }
        lua_pop(L,1);
    }
    lua_getfield(L,2,"on_step");
    if (!lua_isnil(L,-1)) return luaL_error(L,"mobs use brain.update and movement.update instead of on_step");
    lua_pop(L,1);
    // Entity registration owns lifecycle, body, health and explicit persistence.
    // Take callback references only after it succeeds.
    // Copy before filling defaults; registration must not modify the caller's table.
    lua_newtable(L);
    lua_pushnil(L);
    while (lua_next(L,2)) { lua_pushvalue(L,-2); lua_pushvalue(L,-2); lua_rawset(L,3); lua_pop(L,1); }
    lua_replace(L,2);
    lua_getfield(L,2,"model");
    if (lua_isnil(L,-1)) { lua_pushinteger(L,0); lua_setfield(L,2,"model"); }
    lua_pop(L,1);
    lua_getfield(L,2,"save");
    if (lua_isnil(L,-1)) { lua_pushboolean(L,false); lua_setfield(L,2,"save"); }
    lua_pop(L,1);
    lua_getfield(L,2,"body");
    if (lua_isnil(L,-1)) {
        lua_newtable(L); lua_pushboolean(L,true); lua_setfield(L,-2,"enabled"); lua_setfield(L,2,"body");
    }
    lua_pop(L,1);
    LuaEntities_Register();
    int id = LuaEntities_Find(name);
    int *refs[] = {&d.brain,&d.movement,&d.attack};
    for (int i = 0; i<3; i++) {
        lua_getfield(L,2,sections[i]); lua_getfield(L,-1,callbacks[i]);
        *refs[i] = luaL_ref(L,LUA_REGISTRYINDEX); lua_pop(L,1);
    }
    definitions[id] = d;
    return 0;
}
static void Callback(Entity *e, int ref) {
    lua_rawgeti(L,LUA_REGISTRYINDEX,ref);
    lua_rawgeti(L,LUA_REGISTRYINDEX,e->scriptRef);
}
static bool Alive(Entity *e) { return e->active && !e->pendingRemoval && !e->dead; }
static int Update(lua_State *state) {
    (void)state;
    Entity *e = LuaEntities_Check(L,1);
    float dt = lua_tonumber(L,2);
    MobDefinition *d = &definitions[e->definitionId];
    MobState *s = ServerMobs_State(e);
    if (!s) return luaL_error(L,"cannot allocate mob state");
    if (s->wasRecovering && !e->recovering) s->brainElapsed = d->interval;
    s->wasRecovering = e->recovering;
    if (!s->initialized || s->brainElapsed>=d->interval) {
        s->initialized = true;
        float elapsed = s->brainElapsed; s->brainElapsed = 0;
        Callback(e,d->brain); lua_pushnumber(L,elapsed); lua_call(L,2,1);
        if (!Alive(e)) return 0;
        ReadIntent(s); lua_pop(L,1);
    }
    if (e->recovering && !d->moveDuringRecovery) {
        s->pathCount = 0; s->hasWaypoint = false; s->replan = s->lookAhead = 0;
        e->moveEnabled = false;
    } else {
        if (d->moveDuringRecovery) e->recovering = false;
        Callback(e,d->movement); lua_pushnumber(L,dt); PushIntent(s); lua_call(L,3,0);
        if (!Alive(e)) return 0;
    }
    Entity *target = ServerMobs_Target(s);
    if (!s->attack || !target || target==e || s->cooldown>0 || s->retry>0) return 0;
    float x=target->position.x-e->position.x, y=target->position.y-e->position.y, z=target->position.z-e->position.z;
    if (x*x+y*y+z*z>d->range*d->range) return 0;
    if (d->lineOfSight) {
        Vector3 a=e->position, b=target->position;
        a.y += (e->body.localBounds.min.y+e->body.localBounds.max.y)*0.5f;
        b.y += (target->body.localBounds.min.y+target->body.localBounds.max.y)*0.5f;
        if (ServerQuery_Raycast(a,b,false,-1).type!=HIT_NOTHING) return 0;
    }
    // Both native handle types offer the same health operation.
    PushTarget(target); lua_getfield(L,-1,"get_hp"); lua_pushvalue(L,-2); lua_call(L,1,1);
    bool living = lua_tointeger(L,-1)>0; lua_pop(L,2);
    if (!living) return 0;
    Callback(e,d->attack); PushTarget(target); lua_call(L,2,1);
    if (!Alive(e)) return 0;
    luaL_checktype(L,-1,LUA_TBOOLEAN);
    if (lua_toboolean(L,-1)) s->cooldown = d->cooldown;
    else s->retry = d->interval;
    return 0;
}
void LuaMobs_Physics(Entity *e, float dt) {
    ServerMobs_Tick(e,dt);
    if (e->definitionId<0 || e->definitionId>=256 || !definitions[e->definitionId].registered || e->dead) return;
    int top = lua_gettop(L);
    lua_pushcfunction(L,Update); LuaEntities_Push(e); lua_pushnumber(L,dt);
    if (lua_pcall(L,2,0,0)!=LUA_OK) {
        TraceLog(LOG_WARNING,"Mob %s: %s",LuaEntities_Name(e->definitionId),lua_tostring(L,-1));
        ServerWorld_RemoveEntity(e->id);
    }
    lua_settop(L,top);
}
void LuaMobs_Reset(void) {
    for (int i=0; i<256; i++) if (definitions[i].registered) {
        luaL_unref(L,LUA_REGISTRYINDEX,definitions[i].brain);
        luaL_unref(L,LUA_REGISTRYINDEX,definitions[i].movement);
        luaL_unref(L,LUA_REGISTRYINDEX,definitions[i].attack);
    }
    memset(definitions,0,sizeof(definitions));
}
int LuaMobs_Follow(lua_State *state) {
    Entity *e = LuaEntities_Check(state,1);
    Vector3 goal; bool hasGoal = !lua_isnoneornil(L,2);
    if (hasGoal) goal = Position(2);
    if (lua_isnoneornil(L,3)) { lua_settop(L,2); lua_newtable(L); }
    luaL_checktype(L,3,LUA_TTABLE);
    float speed = Number(3,"speed",2,0,20), acceleration = Number(3,"acceleration",32,0,100);
    if (acceleration<=0) return luaL_error(L,"acceleration must be greater than zero");
    float jump = Number(3,"jump",7,0,20);
    if (!e->body.enabled) return luaL_error(L,"ground movement requires an enabled body");
    lua_pushboolean(L,ServerMobs_Follow(e,hasGoal ? &goal : NULL,speed,acceleration,jump)); return 1;
}
int LuaMobs_Wander(lua_State *state) {
    Entity *e = LuaEntities_Check(state,1);
    double radius = luaL_optnumber(L,2,6);
    if (!isfinite(radius) || radius<1 || radius>16) return luaL_error(L,"wander radius must be 1..16");
    Vector3 goal;
    if (ServerMobs_Wander(e,radius,&goal)) PushPosition(goal); else lua_pushnil(L);
    return 1;
}
int LuaMobs_Steer(lua_State *state) {
    Entity *e = LuaEntities_Check(state,1);
    Vector3 goal = Position(2), delta = {goal.x-e->position.x,goal.y-e->position.y,goal.z-e->position.z};
    float speed = luaL_checknumber(L,3), acceleration = luaL_optnumber(L,4,32);
    if (!ServerPhysics_Move(e,delta,speed,acceleration)) return luaL_error(L,"invalid steering speed, acceleration or body");
    float distance = sqrtf(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);
    // Slow near the destination to avoid repeatedly overshooting it.
    float scale = distance>0.001f ? fminf(speed,sqrtf(2*acceleration*distance))/distance : 0;
    e->moveVelocity = (Vector3){delta.x*scale,delta.y*scale,delta.z*scale};
    e->move3D = true;
    return 0;
}
int LuaMobs_Teleport(lua_State *state) {
    Entity *e = LuaEntities_Check(state,1); Vector3 goal = Position(2);
    bool ground = false;
    if (!lua_isnoneornil(L,3)) { luaL_checktype(L,3,LUA_TBOOLEAN); ground = lua_toboolean(L,3); }
    bool clear = ground ? ServerQuery_CanWalk(e->body,goal,goal) : ServerQuery_Clear(e->body,goal);
    if (clear) ServerWorld_TeleportEntity(e->id,goal,e->rotation);
    lua_pushboolean(L,clear); return 1;
}
