/**
 * Copyright (c) 2021-2022 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SERVER_LUA_BINDINGS_H
#define MIDLESS_SERVER_LUA_BINDINGS_H
#include "../scripthooks.h"
#include <stdbool.h>
#include "raylib.h"
#include "minilua.h"
struct Entity;
struct Entity *LuaBindings_TestPlayerEntity(lua_State *state, int index);
struct Player;
void LuaBindings_PushPlayer(struct Player *player);

#endif
