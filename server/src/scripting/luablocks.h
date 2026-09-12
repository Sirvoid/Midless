/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_LUA_BLOCKS_H
#define MIDLESS_LUA_BLOCKS_H

int LuaBlocks_SetBlock(void);
int LuaBlocks_SetBlocks(void);
int LuaBlocks_DefineBlock(void);
int LuaBlocks_RegisterBlockUpdate(void);
void LuaBlocks_Init(void);
void LuaBlocks_Shutdown(void);
#endif
