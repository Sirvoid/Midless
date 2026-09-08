#ifndef MIDLESS_LUA_INVENTORY_H
#define MIDLESS_LUA_INVENTORY_H
#include "minilua.h"
#include "../player.h"
void LuaInventory_Init(void);
int LuaInventory_Get(lua_State *state, Player *player);
int LuaInventory_Show(lua_State *state, Player *player);
int LuaInventory_Close(lua_State *state, Player *player);
#endif
