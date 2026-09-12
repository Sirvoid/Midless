/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_ENTITYTEXTURE_H
#define MIDLESS_LUA_ENTITYTEXTURE_H
#include "minilua.h"
#include "../entity.h"

int LuaEntityTexture_Set(lua_State *state, Entity *entity);
int LuaEntityTexture_Get(lua_State *state, Entity *entity);
#endif
