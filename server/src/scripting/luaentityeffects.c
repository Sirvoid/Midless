/**
 * Copyright (c) 2026 Sirvoid
 * Released under the MIT License. https://opensource.org/licenses/MIT
 */
#include "luaentityeffects.h"
#include "../entityeffects.h"
#include <math.h>

int LuaEntityEffects_Flash(lua_State *state, Entity *entity) {
    unsigned char rgb[3];
    for (int i = 0; i < 3; i++) {
        lua_Integer value = luaL_checkinteger(state, i + 2);
        luaL_argcheck(state, value >= 0 && value <= 255, i + 2, "color must be 0..255");
        rgb[i] = (unsigned char)value;
    }
    double duration = luaL_checknumber(state, 5);
    luaL_argcheck(state, isfinite(duration) && duration >= 0 && duration <= 60,
                  5, "duration must be 0..60 seconds");
    Color color = {rgb[0], rgb[1], rgb[2], 255};
    ServerEntityEffects_Flash(entity, color, (float)duration);
    return 0;
}

int LuaEntityEffects_SetPose(lua_State *state, Entity *entity) {
    static const char *const poses[] = {"stand", "sit", NULL};
    EntityPose pose = (EntityPose)luaL_checkoption(state, 2, NULL, poses);
    if (entity->type == ENTITY_TYPE_DROPPED_ITEM)
        return luaL_error(state, "dropped items do not support poses");
    if (entity->pose != pose) {
        entity->pose = pose;
        entity->poseDirty = true;
    }
    return 0;
}
int LuaEntityEffects_GetPose(lua_State *state, Entity *entity) {
    lua_pushstring(state, entity->pose == ENTITY_POSE_SIT ? "sit" : "stand");
    return 1;
}
