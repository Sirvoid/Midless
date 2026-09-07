#include <math.h>
#include "inventoryscreen.h"
#include "inventoryclient.h"
#include "blockitemrenderer.h"
#include "block.h"

static void DrawStack(ItemStack stack, Rectangle bounds) {
    if (!stack.count) return;
    BlockItemRenderer_Draw(stack.itemId, (Rectangle){bounds.x + 3, bounds.y + 3, bounds.width - 6, bounds.height - 6});
    int fontSize = bounds.width >= 40 ? 18 : 12;
    const char *count = TextFormat("%i", stack.count);
    int x = (int)(bounds.x + bounds.width - MeasureText(count, fontSize) - 4);
    int y = (int)(bounds.y + bounds.height - fontSize - 3);
    DrawText(count, x + 1, y + 1, fontSize, BLACK);
    DrawText(count, x, y, fontSize, WHITE);
}

void ClientInventory_Draw(bool showStorage) {
    const Inventory *inventory = ClientInventory_Get();
    float size = fminf(52, fminf((GetScreenWidth() - 32) / 9.0f, (GetScreenHeight() - 100) / 4.5f));
    if (size < 12) return;
    float left = (GetScreenWidth() - 9 * size) / 2;
    float top = showStorage ? (GetScreenHeight() - size * 4.35f) / 2 : GetScreenHeight() - size - 12;
    Vector2 mouse = GetMousePosition();
    int hoveredSlot = -1;
    Rectangle slots[INVENTORY_SLOT_COUNT] = {0};
    int firstSlot = showStorage ? 0 : INVENTORY_STORAGE_SLOTS;
    for (int index = firstSlot; index < INVENTORY_SLOT_COUNT; index++) {
        int column = index % INVENTORY_HOTBAR_SLOTS;
        float row = showStorage ? index / INVENTORY_HOTBAR_SLOTS : 0;
        if (showStorage && index >= INVENTORY_STORAGE_SLOTS) row += 0.35f;
        slots[index] = (Rectangle){left + column * size, top + row * size, size - 3, size - 3};
        if (showStorage && CheckCollisionPointRec(mouse, slots[index])) hoveredSlot = index;
    }

    if (hoveredSlot >= 0) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) ClientInventory_Click(hoveredSlot, false);
        else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) ClientInventory_Click(hoveredSlot, true);
    } else if (showStorage && inventory->cursor.count) {
        Rectangle panel = {left - 10, top - 35, size * 9 + 20, size * 4.35f + 45};
        if (!CheckCollisionPointRec(mouse, panel)) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) ClientInventory_Throw(false);
            else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) ClientInventory_Throw(true);
        }
    }
    if (showStorage) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0, 0, 0, 100});
        DrawRectangleRec((Rectangle){left - 10, top - 35, size * 9 + 20, size * 4.35f + 45}, (Color){18, 18, 18, 230});
        DrawText("Inventory", (int)left, (int)top - 26, 20, WHITE);
    }
    for (int index = firstSlot; index < INVENTORY_SLOT_COUNT; index++) {
        Rectangle bounds = slots[index];
        bool hovered = index == hoveredSlot;
        bool selected = index == INVENTORY_STORAGE_SLOTS + inventory->selectedHotbar;
        DrawRectangleRec(bounds, hovered ? (Color){100, 100, 100, 180} : (Color){0, 0, 0, 130});
        DrawRectangleLinesEx(bounds, selected ? 2 : 1, selected || hovered ? WHITE : (Color){150, 150, 150, 200});
        DrawStack(inventory->slots[index], bounds);
    }
    if (!showStorage) return;
    if (inventory->cursor.count) {
        DrawStack(inventory->cursor, (Rectangle){mouse.x - size / 2, mouse.y - size / 2, size, size});
    } else if (hoveredSlot >= 0 && inventory->slots[hoveredSlot].count) {
        const char *name = Block_GetDefinition(inventory->slots[hoveredSlot].itemId)->name;
        int width = MeasureText(name, 16);
        float x = fminf(mouse.x + 14, GetScreenWidth() - width - 12);
        DrawRectangle((int)x - 4, (int)mouse.y - 26, width + 8, 24, (Color){0, 0, 0, 220});
        DrawText(name, (int)x, (int)mouse.y - 22, 16, WHITE);
    }
    const char *hint = ClientInventory_CloseBlocked() ? "Cannot drop the held stack here right now." : "";
    DrawText(hint, (GetScreenWidth() - MeasureText(hint, 14)) / 2, (int)(top + size * 4.35f + 20), 14, WHITE);
}
