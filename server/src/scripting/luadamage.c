#include <math.h>
#include "luadamage.h"
#include "luabindings.h"
#include "../world/world.h"

extern lua_State *L;
static int playerCallbacks[64], playerCallbackCount;

Vector3 LuaDamage_Impulse(lua_State *state, int context, Entity *target) {
    context = lua_absindex(state,context);
    lua_getfield(state,context,"cause");
    if (!lua_isnil(state,-1)) luaL_checktype(state,-1,LUA_TSTRING);
    lua_pop(state,1);
    Vector3 direction = {0};
    lua_getfield(state,context,"attacker");
    if (!lua_isnil(state,-1)) {
        Entity *attacker = LuaEntities_Test(state,-1);
        if (!attacker) attacker = LuaBindings_TestPlayerEntity(state,-1);
        if (attacker) direction = (Vector3){target->position.x-attacker->position.x,0,target->position.z-attacker->position.z};
        else if (!luaL_testudata(state,-1,"midless.Entity") && !luaL_testudata(state,-1,"midless.Player"))
            luaL_error(state,"damage attacker must be an entity or player handle");
    }
    lua_pop(state,1);
    lua_getfield(state,context,"direction");
    if (!lua_isnil(state,-1)) {
        luaL_checktype(state,-1,LUA_TTABLE);
        const char *names[] = {"x","y","z"}; float values[3];
        for (int i=0; i<3; i++) {
            lua_getfield(state,-1,names[i]); double n = luaL_checknumber(state,-1);
            if (!isfinite(n) || fabs(n)>1000000) luaL_error(state,"invalid damage direction");
            values[i] = n; lua_pop(state,1);
        }
        direction = (Vector3){values[0],0,values[2]};
    }
    lua_pop(state,1);
    float horizontal = 0, upward = 0;
    lua_getfield(state,context,"knockback");
    if (!lua_isnil(state,-1)) {
        luaL_checktype(state,-1,LUA_TTABLE);
        const char *names[] = {"horizontal","upward"}; float *values[] = {&horizontal,&upward};
        for (int i=0; i<2; i++) {
            lua_getfield(state,-1,names[i]); double n = luaL_optnumber(state,-1,0);
            if (!isfinite(n) || n<0 || n>20) luaL_error(state,"damage knockback must be 0..20");
            *values[i] = n; lua_pop(state,1);
        }
    }
    lua_pop(state,1);
    float length = hypotf(direction.x,direction.z);
    float scale = length>0.0001f ? horizontal/length : 0;
    return (Vector3){direction.x*scale,upward,direction.z*scale};
}
int LuaDamage_Result(lua_State *state, int index, int amount) {
    if (lua_isnil(state,index)) return amount;
    if (lua_isboolean(state,index) && !lua_toboolean(state,index)) return 0;
    if (lua_isinteger(state,index) && lua_tointeger(state,index)>=0 && lua_tointeger(state,index)<=65535)
        return lua_tointeger(state,index);
    TraceLog(LOG_WARNING,"Damage callback must return nil, false or damage 0..65535");
    return 0;
}
int LuaDamage_RegisterPlayer(void) {
    luaL_checktype(L,1,LUA_TFUNCTION);
    if (playerCallbackCount==64) return luaL_error(L,"too many player damage callbacks");
    lua_pushvalue(L,1); playerCallbacks[playerCallbackCount++] = luaL_ref(L,LUA_REGISTRYINDEX);
    return 0;
}
int LuaDamage_PlayerHooks(Entity *target, int context, int amount) {
    context = lua_absindex(L,context);
    int count = playerCallbackCount;
    for (int i=0; i<count && amount>0; i++) {
        lua_rawgeti(L,LUA_REGISTRYINDEX,playerCallbacks[i]);
        LuaBindings_PushPlayer(serverWorld.players[target->ownerPlayerId]);
        lua_pushinteger(L,amount); lua_pushvalue(L,context);
        if (lua_pcall(L,3,1,0)!=LUA_OK) {
            TraceLog(LOG_WARNING,"Player on_damage: %s",lua_tostring(L,-1)); amount = 0;
        } else amount = LuaDamage_Result(L,-1,amount);
        lua_pop(L,1);
    }
    return amount;
}
void LuaDamage_Reset(void) {
    for (int i=0; i<playerCallbackCount; i++) luaL_unref(L,LUA_REGISTRYINDEX,playerCallbacks[i]);
    playerCallbackCount = 0;
}
