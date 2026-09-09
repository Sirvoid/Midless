#ifndef MIDLESS_SERVER_ITEMS_H
#define MIDLESS_SERVER_ITEMS_H
#include "itemdefinition.h"
#include "minilua.h"
#include "inventory.h"
struct Player;
extern ItemDefinition serverItems[ITEM_LIMIT];
bool ServerItems_Init(void);
bool ServerItems_Ready(void);
int ServerItems_Id(lua_State *state, int index, bool block, bool reserve);
int ServerItems_Declare(lua_State *state, bool block);
void ServerItems_PushId(lua_State *state, int id);
int ServerItems_Define(void);
void ServerItems_DefineBlock(int id);
bool ServerItems_IsDefined(int id);
void ServerItems_Send(struct Player *player);
void ServerItems_ReadStack(lua_State *state, int index, ItemStack *stack);
void ServerItems_PushStack(lua_State *state, ItemStack stack);
#endif
