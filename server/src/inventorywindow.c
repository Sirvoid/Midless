#include "inventorywindow.h"
#include "player.h"
#include "world/world.h"
#include "scripting/luametadata.h"
#include "serverinventory.h"
#include "inventoryprotocol.h"
#include "networkhandler.h"
#include "packet.h"
#include "droppeditems.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool SamePosition(Vector3 a, Vector3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
static bool Read(InventoryWindow *w, ItemStack *slots) {
    Vector3 chunk = {floorf(w->position.x / 16), floorf(w->position.y / 16), floorf(w->position.z / 16)};
    return ServerWorld_GetChunkAt(chunk) && ServerWorld_GetBlock(w->position) == w->blockId &&
        LuaMetadata_BlockInventory(w->position, w->field, slots, w->view.slotCount, false);
}
static bool Reach(Player *p, InventoryWindow *w) {
    if (p->entityId < 0 || p->entityId >= WORLD_MAX_ENTITIES) return false;
    Vector3 position = serverWorld.entities[p->entityId].position;
    float x = position.x - w->position.x - 0.5f;
    float y = position.y + 1.5f - w->position.y - 0.5f;
    float z = position.z - w->position.z - 0.5f;
    return x*x + y*y + z*z <= 64;
}
static bool Write(InventoryWindow *w, ItemStack *slots) {
    return LuaMetadata_BlockInventory(w->position, w->field, slots, w->view.slotCount, true);
}
static bool SameSlots(const ItemStack *a, const ItemStack *b, int count) {
    for (int i = 0; i < count; i++) if (a[i].itemId != b[i].itemId || a[i].count != b[i].count) return false;
    return true;
}

bool InventoryWindow_Open(Player *p, const InventoryWindow *window) {
    InventoryWindow next = *window;
    if (p->disconnected || p->inventory.cursor.count || p->nextInventorySession == UINT32_MAX) return false;
    next.view.session = p->nextInventorySession + 1;
    if (!InventoryView_Validate(&next.view) || !Reach(p, &next) || !Read(&next, next.view.slots)) return false;
    p->nextInventorySession++;
    p->inventoryWindow = next;
    p->inventory.open = true;
    p->inventoryRevision++;
    ServerInventory_Send(p);
    return true;
}

bool InventoryWindow_Send(Player *p) {
    InventoryWindow *w = &p->inventoryWindow;
    if (!w->view.session) return false;
    // Include player slots, cursor, and container in one authoritative snapshot.
    uint8_t state[INVENTORY_STATE_PACKET_SIZE];
    InventoryProtocol_WriteState(state, &p->inventory, p->inventoryRevision, p->inventorySequence);
    BinaryWriter out = {0};
    Binary_U8(&out, PACKET_INVENTORY_VIEW);
    Binary_Write(&out, state, sizeof(state));
    InventoryView_Write(&out, &w->view);
    if (!out.failed) {
        serverPacketLastDynamicLength = out.size;
        ServerNetwork_Send(p, out.data);
    } else free(out.data);
    return true;
}

bool InventoryWindow_Close(Player *p) {
    InventoryWindow *w = &p->inventoryWindow;
    ItemStack *cursor = &p->inventory.cursor;
    if (cursor->count) {
        int moved = Inventory_AddPartial(&p->inventory, cursor->itemId, cursor->count);
        cursor->count -= moved;
        if (!cursor->count) *cursor = (ItemStack){0};
    }
    if (cursor->count && !ServerDrops_Throw(p, false)) return false;
    w->view.session = 0;
    Inventory_Close(&p->inventory);
    return true;
}

static void Revoke(Player *p) {
    // If no space or drop entity is available, retain the cursor in the player
    // inventory screen. It must never keep access to an invalid container.
    InventoryWindow_Close(p);
    p->inventoryWindow.view.session = 0;
    p->inventoryRevision++;
    ServerInventory_UpdateHeldBlock(p);
    ServerInventory_Send(p);
}

void InventoryWindow_Action(Player *p, const InventoryAction *a) {
    InventoryWindow *w = &p->inventoryWindow;
    if (!w->view.session || (uint32_t)a->x != w->view.session || a->y < 0 || a->y > 1) return;
    if (!Reach(p, w)) { Revoke(p); return; }
    ItemStack slots[INVENTORY_VIEW_SLOTS];
    if (!Read(w, slots)) { Revoke(p); return; }
    if (a->slot >= (a->y ? w->view.slotCount : INVENTORY_SLOT_COUNT)) return;
    Inventory next = p->inventory;
    ItemStack *source = a->y ? &slots[a->slot] : &next.slots[a->slot];
    if (a->type == INVENTORY_VIEW_SHIFT) {
        if (next.cursor.count) return;
        Inventory_Transfer(source, a->y ? next.slots : slots, a->y ? INVENTORY_SLOT_COUNT : w->view.slotCount);
    } else if (a->type == INVENTORY_VIEW_LEFT || a->type == INVENTORY_VIEW_RIGHT) {
        Inventory_ClickStack(source, &next.cursor, a->type == INVENTORY_VIEW_RIGHT);
    } else return;
    next.cursorOrigin = INVENTORY_NO_SLOT;
    // Commit metadata first. Failed allocation/encoding leaves both owners intact.
    if (!Write(w, slots)) return;
    p->inventory = next;
    memcpy(w->view.slots, slots, w->view.slotCount * sizeof(ItemStack));
}

void InventoryWindow_Update(void) {
    if (!serverWorld.players) return;
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *p = serverWorld.players[i];
        if (!p || p->disconnected || !p->inventoryWindow.view.session) continue;
        InventoryWindow *w = &p->inventoryWindow;
        ItemStack slots[INVENTORY_VIEW_SLOTS];
        if (!Reach(p, w) || !Read(w, slots)) { Revoke(p); continue; }
        if (!SameSlots(slots, w->view.slots, w->view.slotCount)) {
            memcpy(w->view.slots, slots, sizeof(ItemStack) * w->view.slotCount);
            p->inventoryRevision++;
            ServerInventory_Send(p);
        }
    }
}
void InventoryWindow_Invalidate(Vector3 position) {
    if (!serverWorld.players) return;
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *p = serverWorld.players[i];
        if (p && p->inventoryWindow.view.session && SamePosition(p->inventoryWindow.position, position)) Revoke(p);
    }
}
void InventoryWindow_UnloadChunk(Vector3 position) {
    if (!serverWorld.players) return;
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *p = serverWorld.players[i];
        if (!p || !p->inventoryWindow.view.session) continue;
        Vector3 block = p->inventoryWindow.position;
        Vector3 chunk = {floorf(block.x / 16), floorf(block.y / 16), floorf(block.z / 16)};
        if (SamePosition(chunk, position)) Revoke(p);
    }
}
