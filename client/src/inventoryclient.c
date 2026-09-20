/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <math.h>
#include "inventoryclient.h"
#include "inventory.h"
#include "player.h"
#include "packet.h"
#include "networkhandler.h"
#include "screens.h"

#define MAX_PENDING_ACTIONS 64

static Inventory displayedInventory;
static InventoryView displayedView;
static InventoryAction pendingActions[MAX_PENDING_ACTIONS];
static int pendingCount;
static uint32_t nextSequence, latestRevision;
static bool ready;
static bool closeBlocked;
static double lastRetryTime;
static void ApplyScreenState(void) {
    player.blockSelected = Inventory_GetSelected(&displayedInventory)->itemId;
    if (displayedInventory.open && currentScreen == SCREEN_GAME) {
        Screen_Switch(SCREEN_INVENTORY);
        screenCursorEnabled = true;
        EnableCursor();
    } else if (!displayedInventory.open && currentScreen == SCREEN_INVENTORY) {
        Screen_Switch(SCREEN_GAME);
        screenCursorEnabled = false;
        DisableCursor();
    }
}

void ClientInventory_Reset(void) {
    displayedView = (InventoryView){0};
    Inventory_Init(&displayedInventory);
    pendingCount = 0;
    nextSequence = 1;
    latestRevision = 0;
    ready = false;
    closeBlocked = false;
    lastRetryTime = 0;
    player.blockSelected = 0;
}

static void SendAction(const InventoryAction *action) {
    unsigned char *packet = Packet_CreateInventoryAction(action);
    if (packet) Network_Send(packet);
}

static bool QueueAction(InventoryAction action) {
    if (!ready || !networkConnectedToServer || pendingCount == MAX_PENDING_ACTIONS || nextSequence == 0) return false;
    action.sequence = nextSequence++;
    action.session = displayedView.session;
    if (pendingCount == 0) lastRetryTime = GetTime();
    pendingActions[pendingCount++] = action;
    if (!displayedView.session) Inventory_ApplyAction(&displayedInventory, &action);
    SendAction(&action);
    ApplyScreenState();
    return true;
}

void ClientInventory_SetState(const Inventory *authoritative, uint32_t revision, uint32_t acknowledged, const InventoryView *view) {
    if (ready && revision < latestRevision) return;
    latestRevision = revision;
    ready = true;
    int remaining = 0;
    bool acknowledgedClose = false;
    for (int i = 0; i < pendingCount; i++) {
        if (pendingActions[i].sequence > acknowledged) pendingActions[remaining++] = pendingActions[i];
        else if (pendingActions[i].type == INVENTORY_CLOSE) acknowledgedClose = true;
    }
    pendingCount = remaining;
    displayedInventory = *authoritative;
    displayedView = view ? *view : (InventoryView){0};
    for (int i = 0; i < pendingCount; i++) {
        if (!displayedView.session && !pendingActions[i].session)
            Inventory_ApplyAction(&displayedInventory, &pendingActions[i]);
    }
    if (acknowledgedClose && displayedInventory.open) closeBlocked = true;
    if (!displayedInventory.cursor.count) closeBlocked = false;
    ApplyScreenState();
}


const InventoryView *ClientInventory_GetView(void) { return displayedView.session ? &displayedView : NULL; }
void ClientInventory_ClickView(int binding, int slot, bool right, bool shift) {
    if (!displayedView.session || binding < 0 || binding >= displayedView.bindingCount ||
        slot < 0 || slot >= displayedView.bindingSlots[binding]) return;
    QueueAction((InventoryAction){.type = shift ? INVENTORY_VIEW_SHIFT : right ? INVENTORY_VIEW_RIGHT : INVENTORY_VIEW_LEFT,
        .slot = slot, .binding = binding});
}
void ClientInventory_Craft(int element, bool all) {
    if (!displayedView.session || element < 0 || element >= displayedView.count ||
        !displayedView.elements[element].crafting) return;
    QueueAction((InventoryAction){.type = all ? INVENTORY_CRAFT_ALL : INVENTORY_CRAFT, .slot = element});
}

void ClientInventory_Update(void) {
    if (!ready || !networkConnectedToServer) return;
    ApplyScreenState();
    if (pendingCount && GetTime() - lastRetryTime > 0.5) {
        // Recover even if the server's bounded incoming queue discarded a request.
        for (int i = 0; i < pendingCount; i++) SendAction(&pendingActions[i]);
        lastRetryTime = GetTime();
    }
}

bool ClientInventory_IsOpen(void) {
    return displayedInventory.open;
}

const Inventory *ClientInventory_Get(void) {
    return &displayedInventory;
}

bool ClientInventory_Toggle(void) {
    bool wasOpen = displayedInventory.open;
    bool queued = QueueAction((InventoryAction){.type = wasOpen ? INVENTORY_CLOSE : INVENTORY_OPEN});
    closeBlocked = false;
    return queued;
}

void ClientInventory_Click(int slot, bool rightClick) {
    if (slot < 0 || slot >= INVENTORY_SLOT_COUNT) return;
    closeBlocked = false;
    QueueAction((InventoryAction){.type = rightClick ? INVENTORY_RIGHT_CLICK : INVENTORY_LEFT_CLICK, .slot = slot});
}

void ClientInventory_Throw(bool oneItem) {
    if (!displayedInventory.open || !displayedInventory.cursor.count) return;
    QueueAction((InventoryAction){.type = oneItem ? INVENTORY_THROW_ONE : INVENTORY_THROW_STACK});
}

void ClientInventory_Select(int hotbarSlot) {
    if (hotbarSlot < 0 || hotbarSlot >= INVENTORY_HOTBAR_SLOTS || hotbarSlot == displayedInventory.selectedHotbar) return;
    QueueAction((InventoryAction){.type = INVENTORY_SELECT, .slot = hotbarSlot});
}

void ClientInventory_Scroll(int direction) {
    ClientInventory_Select((displayedInventory.selectedHotbar + direction + INVENTORY_HOTBAR_SLOTS) % INVENTORY_HOTBAR_SLOTS);
}

bool ClientInventory_CloseBlocked(void) { return closeBlocked; }
bool ClientInventory_Ready(void) { return ready && networkConnectedToServer; }
uint32_t ClientInventory_Sequence(void) { return nextSequence - 1; }
