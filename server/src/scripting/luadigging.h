#ifndef MIDLESS_LUA_DIGGING_H
#define MIDLESS_LUA_DIGGING_H
#include "../player.h"
void LuaDigging_Init(void);
void LuaDigging_Shutdown(void);
void LuaDigging_Define(int id, int table, bool block);
int LuaDigging_Register(void);
double LuaDigging_Time(Player *player, Vector3 position, int block, ItemStack stack);
void LuaDigging_Finished(Player *player, Vector3 position, ItemStack stack);
#endif
