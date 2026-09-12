/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_CHAT_H
#define MIDLESS_LUA_CHAT_H
#include "minilua.h"

int LuaChat_RegisterMessage(lua_State *state);
int LuaChat_Broadcast(lua_State *state);
int LuaChat_SendPlayerMessage(lua_State *state);
void LuaChat_Shutdown(void);
#endif
