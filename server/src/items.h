#ifndef MIDLESS_SERVER_ITEMS_H
#define MIDLESS_SERVER_ITEMS_H
#include "itemdefinition.h"
#include "inventory.h"
struct Player;
extern ItemDefinition serverItems[ITEM_LIMIT];
bool ServerItems_Init(void);
bool ServerItems_Ready(void);
int ServerItems_Find(const char *name);
const char *ServerItems_Reserve(const char *name, bool block, int *result);
const char *ServerItems_ReserveLegacy(int id, bool block);
void ServerItems_Define(int id, const ItemDefinition *definition);
void ServerItems_DefineBlock(int id);
bool ServerItems_IsDefined(int id);
void ServerItems_Send(struct Player *player);
#endif
