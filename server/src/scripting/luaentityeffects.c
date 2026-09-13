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
