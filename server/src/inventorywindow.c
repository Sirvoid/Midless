#include "inventorywindow.h"
#include "player.h"
#include "world/world.h"
#include "scripting/luametadata.h"
#include "serverinventory.h"
#include "inventoryprotocol.h"
#include "networkhandler.h"
#include "packet.h"
#include "droppeditems.h"
#include "crafting.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool SamePosition(Vector3 a, Vector3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
static bool Read(Player *p, InventoryWindow *w, ItemStack *slots) {
    Vector3 chunk = {floorf(w->position.x / 16), floorf(w->position.y / 16), floorf(w->position.z / 16)};
    if (w->block && (!ServerWorld_GetChunkAt(chunk) || ServerWorld_GetBlock(w->position) != w->blockId)) return false;
    for (int i = 1; i < w->view.bindingCount; i++) {
        int offset = InventoryView_Offset(&w->view, i), count = w->view.bindingSlots[i];
        if (w->bindings[i].block) {
            if (!LuaMetadata_BlockInventory(w->position, w->bindings[i].name, slots + offset, count, false)) return false;
        } else {
            NamedInventory *inventory = PlayerInventories_Get(p, w->bindings[i].name);
            if (!inventory || inventory->count != count) return false;
            memcpy(slots + offset, inventory->slots, count * sizeof(ItemStack));
        }
    }
    return true;
}
static bool Reach(Player *p, InventoryWindow *w) {
    if (!w->block) return true;
    if (p->entityId < 0 || p->entityId >= WORLD_MAX_ENTITIES) return false;
    Vector3 position = serverWorld.entities[p->entityId].position;
    float x = position.x - w->position.x - 0.5f;
    float y = position.y + 1.5f - w->position.y - 0.5f;
    float z = position.z - w->position.z - 0.5f;
    return x*x + y*y + z*z <= 64;
}
static bool Write(Player *p, InventoryWindow *w, ItemStack *slots) {
    NamedInventory *named[INVENTORY_VIEW_BINDINGS] = {0};
    for (int i = 1; i < w->view.bindingCount; i++) if (!w->bindings[i].block) {
        named[i] = PlayerInventories_Get(p, w->bindings[i].name);
        if (!named[i] || named[i]->count != w->view.bindingSlots[i]) return false;
    }
    // Screens bind at most one block inventory. Commit its fallible write first;
    // the remaining player inventories are plain memory copies.
    for (int i = 1; i < w->view.bindingCount; i++) if (w->bindings[i].block) {
        if (!LuaMetadata_BlockInventory(w->position, w->bindings[i].name,
            slots + InventoryView_Offset(&w->view, i), w->view.bindingSlots[i], true)) return false;
    }
    for (int i = 1; i < w->view.bindingCount; i++) if (named[i])
        memcpy(named[i]->slots, slots + InventoryView_Offset(&w->view, i), w->view.bindingSlots[i] * sizeof(ItemStack));
    return true;
}
static bool SameSlots(const ItemStack *a, const ItemStack *b, int count) {
    for (int i = 0; i < count; i++) if (a[i].itemId != b[i].itemId || a[i].count != b[i].count) return false;
    return true;
}
static bool RefreshPreviews(InventoryWindow *w) {
    bool changed = false;
    for (int i = 0; i < w->view.count; i++) {
        InventoryElement *e = &w->view.elements[i];
        if (!e->crafting) continue;
        CraftingMatch match;
        Crafting_Find(w->recipes[i], w->view.slots + InventoryView_Offset(&w->view, e->binding), e->columns, e->rows, &match);
        if (match.output.count && !ServerWorld_IsBlockDefined(match.output.itemId)) match.output = (ItemStack){0};
        if (e->preview.itemId != match.output.itemId || e->preview.count != match.output.count) changed = true;
        e->preview = match.output;
    }
    return changed;
}

bool InventoryWindow_Open(Player *p, const InventoryWindow *window) {
    InventoryWindow next = *window;
    if (p->disconnected || (window->block && p->inventory.cursor.count) || p->nextInventorySession == UINT32_MAX) return false;
    next.view.session = p->nextInventorySession + 1;
    if (!InventoryView_Validate(&next.view) || !Reach(p, &next) || !Read(p, &next, next.view.slots)) return false;
    p->nextInventorySession++;
    p->inventoryWindow = next;
    RefreshPreviews(&p->inventoryWindow);
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
    if (!w->view.session || (uint32_t)a->x != w->view.session) return;
    if (!Reach(p, w)) { Revoke(p); return; }
    ItemStack slots[INVENTORY_VIEW_SLOTS];
    if (!Read(p, w, slots)) { Revoke(p); return; }
    if (a->type == INVENTORY_CRAFT || a->type == INVENTORY_CRAFT_ALL) {
        if (a->slot >= w->view.count || a->y != 0) return;
        InventoryElement *e = &w->view.elements[a->slot];
        if (!e->crafting) return;
        ItemStack *inputs = slots + InventoryView_Offset(&w->view, e->binding);
        bool all = a->type == INVENTORY_CRAFT_ALL;
        Inventory next = p->inventory;
        if (all && next.cursor.count) return;
        int crafted = 0;
        int recipe = -1;
        // A bounded loop keeps one shift-click from monopolizing the server.
        for (int attempt = 0; attempt < (all ? 64 : 1); attempt++) {
            CraftingMatch match;
            if (!Crafting_Find(w->recipes[a->slot], inputs, e->columns, e->rows, &match) ||
                !ServerWorld_IsBlockDefined(match.output.itemId)) break;
            if (crafted && match.recipe != recipe) break;
            recipe = match.recipe;
            if (all) {
                if (!Inventory_Add(&next, match.output.itemId, match.output.count)) break;
            } else {
                if ((next.cursor.count && next.cursor.itemId != match.output.itemId) ||
                    next.cursor.count + match.output.count > Item_GetMaxStack(match.output.itemId)) break;
                next.cursor.itemId = match.output.itemId;
                next.cursor.count += match.output.count;
            }
            for (int i = 0; i < e->columns * e->rows; i++) {
                inputs[i].count -= match.consume[i];
                if (!inputs[i].count) inputs[i].itemId = 0;
            }
            crafted++;
        }
        if (!crafted || !Write(p, w, slots)) return;
        next.cursorOrigin = INVENTORY_NO_SLOT;
        p->inventory = next;
        memcpy(w->view.slots, slots, w->view.slotCount * sizeof(ItemStack));
        RefreshPreviews(w);
        return;
    }
    if (a->y < 0 || a->y >= w->view.bindingCount) return;
    if (a->slot >= w->view.bindingSlots[a->y]) return;
    Inventory next = p->inventory;
    ItemStack *source = a->y ? &slots[InventoryView_Offset(&w->view, a->y) + a->slot] : &next.slots[a->slot];
    if (a->type == INVENTORY_VIEW_SHIFT) {
        if (next.cursor.count) return;
        if (a->y) Inventory_Transfer(source, next.slots, INVENTORY_SLOT_COUNT);
        else for (int i = 1; i < w->view.bindingCount && source->count; i++)
            Inventory_Transfer(source, slots + InventoryView_Offset(&w->view, i), w->view.bindingSlots[i]);
    } else if (a->type == INVENTORY_VIEW_LEFT || a->type == INVENTORY_VIEW_RIGHT) {
        Inventory_ClickStack(source, &next.cursor, a->type == INVENTORY_VIEW_RIGHT);
    } else return;
    next.cursorOrigin = INVENTORY_NO_SLOT;
    // Commit metadata first. Failed allocation/encoding leaves both owners intact.
    if (!Write(p, w, slots)) return;
    p->inventory = next;
    memcpy(w->view.slots, slots, w->view.slotCount * sizeof(ItemStack));
    RefreshPreviews(w);
}

void InventoryWindow_Update(void) {
    if (!serverWorld.players) return;
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *p = serverWorld.players[i];
        if (!p || p->disconnected || !p->inventoryWindow.view.session) continue;
        InventoryWindow *w = &p->inventoryWindow;
        ItemStack slots[INVENTORY_VIEW_SLOTS];
        if (!Reach(p, w) || !Read(p, w, slots)) { Revoke(p); continue; }
        bool changed = !SameSlots(slots, w->view.slots, w->view.slotCount);
        memcpy(w->view.slots, slots, sizeof(ItemStack) * w->view.slotCount);
        if (RefreshPreviews(w)) changed = true;
        if (changed) {
            p->inventoryRevision++;
            ServerInventory_Send(p);
        }
    }
}
void InventoryWindow_Invalidate(Vector3 position) {
    if (!serverWorld.players) return;
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *p = serverWorld.players[i];
        if (p && p->inventoryWindow.block && p->inventoryWindow.view.session && SamePosition(p->inventoryWindow.position, position)) Revoke(p);
    }
}
void InventoryWindow_UnloadChunk(Vector3 position) {
    if (!serverWorld.players) return;
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *p = serverWorld.players[i];
        if (!p || !p->inventoryWindow.block || !p->inventoryWindow.view.session) continue;
        Vector3 block = p->inventoryWindow.position;
        Vector3 chunk = {floorf(block.x / 16), floorf(block.y / 16), floorf(block.z / 16)};
        if (SamePosition(chunk, position)) Revoke(p);
    }
}
