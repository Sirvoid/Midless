/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_INVENTORY_H
#define MIDLESS_LUA_INVENTORY_H
#include "../scripthooks.h"
#include "minilua.h"
#include "../player.h"
void LuaInventory_Init(void);
void LuaInventory_Shutdown(void);
int LuaInventory_Define(lua_State *state);
int LuaInventory_DefineScreen(lua_State *state);
int LuaInventory_Get(lua_State *state, Player *player);
int LuaInventory_Show(lua_State *state, Player *player);
int LuaInventory_Close(lua_State *state, Player *player);
#endif
