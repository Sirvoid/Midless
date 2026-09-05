/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SERVER_LUA_BINDINGS_H
#define MIDLESS_SERVER_LUA_BINDINGS_H


void LuaBindings_Init(void);
void LuaBindings_Shutdown(void);
void LuaBindings_InvokeReady(void);
void LuaBindings_InvokeBlockUpdate(Vector3 position, unsigned short blockId, unsigned short previousBlockId);
void LuaBindings_InvokeChatMessage(const char *name, const char *message);

#endif
