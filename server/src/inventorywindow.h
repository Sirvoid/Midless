#ifndef MIDLESS_INVENTORY_WINDOW_H
#define MIDLESS_INVENTORY_WINDOW_H
#include "inventoryview.h"
#include "raylib.h"

typedef struct InventoryWindow {
    InventoryView view;
    Vector3 position;
    uint16_t blockId;
    char field[65];
} InventoryWindow;

struct Player;
bool InventoryWindow_Open(struct Player *player, const InventoryWindow *window);
bool InventoryWindow_Send(struct Player *player);
void InventoryWindow_Action(struct Player *player, const InventoryAction *action);
void InventoryWindow_Update(void);
void InventoryWindow_Invalidate(Vector3 position);
void InventoryWindow_UnloadChunk(Vector3 position);
bool InventoryWindow_Close(struct Player *player);
#endif
