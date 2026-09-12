/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_LIFECYCLE_H
#define MIDLESS_LUA_LIFECYCLE_H

int LuaLifecycle_RegisterReady(void);
int LuaLifecycle_RegisterStep(void);
int LuaLifecycle_Sleep(void);
void LuaLifecycle_Init(void);
void LuaLifecycle_Shutdown(void);
#endif
