#ifndef MIDLESS_CLIENT_ITEMS_H
#define MIDLESS_CLIENT_ITEMS_H
#include "itemdefinition.h"
#include "raylib.h"
void ClientItems_Define(int id, const ItemDefinition *definition);
void ClientItems_Reset(void);
bool ClientItems_Bar(ItemStack stack, float *fraction);
const char *ClientItems_Name(int id);
void ClientItems_Draw(int id, Rectangle bounds);
bool ClientItems_Draw3D(int id, Matrix transform, float brightness, bool thirdPerson);
#endif
