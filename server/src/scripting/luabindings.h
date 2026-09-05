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
void LuaBindings_InvokeStep(float delta);
void LuaBindings_InvokePlayerJoin(int playerId);
void LuaBindings_InvokePlayerLeave(int playerId);
void LuaBindings_InvokePlayerClick(int playerId, int button);
void LuaBindings_InvokeBlockUpdate(Vector3 position, unsigned short blockId, unsigned short previousBlockId);
void LuaBindings_InvokeChatMessage(int playerId, const char *message);

#endif
