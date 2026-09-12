/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_ITEM_ACTIONS_H
#define MIDLESS_LUA_ITEM_ACTIONS_H
#include "../scripthooks.h"
#include "../player.h"
#include "../entity.h"
void LuaItemActions_Init(void);
void LuaItemActions_Shutdown(void);
void LuaItemActions_Define(int id, int table, bool block);
#endif
