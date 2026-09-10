#ifndef MIDLESS_LUA_ITEM_ACTIONS_H
#define MIDLESS_LUA_ITEM_ACTIONS_H
#include "../player.h"
#include "../entity.h"
void LuaItemActions_Init(void);
void LuaItemActions_Shutdown(void);
void LuaItemActions_Define(int id, int table, bool block);
bool LuaItemActions_Use(Player *player, const InventoryAction *block, Entity *entity);
void LuaItemActions_Placed(Player *player, Vector3 position, int blockId);
int LuaItemActions_Drops(Player *player, Vector3 position, int block, ItemStack tool, ItemStack *stacks, int capacity);
#endif
