/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_CHAT_H
#define MIDLESS_LUA_CHAT_H

int LuaChat_RegisterMessage(void);
int LuaChat_Broadcast(void);
int LuaChat_SendPlayerMessage(void);
void LuaChat_Shutdown(void);
#endif
