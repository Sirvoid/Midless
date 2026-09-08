#include <math.h>
#include "inventoryclient.h"
#include "inventoryprotocol.h"
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
    unsigned char *packet = MemAlloc(INVENTORY_ACTION_PACKET_SIZE);
    if (!packet) return;
    InventoryProtocol_WriteAction(packet, action);
    Network_Send(packet);
}

static bool QueueAction(InventoryAction action) {
    if (!ready || !networkConnectedToServer || pendingCount == MAX_PENDING_ACTIONS || nextSequence == 0) return false;
    action.sequence = nextSequence++;
    if (action.type != INVENTORY_PLACE && action.type != INVENTORY_BREAK)
        action.x = (int32_t)displayedView.session;
    if (pendingCount == 0) lastRetryTime = GetTime();
    pendingActions[pendingCount++] = action;
    if (!displayedView.session) Inventory_ApplyAction(&displayedInventory, &action);
    SendAction(&action);
    ApplyScreenState();
    return true;
}

static void HandleState(const uint8_t *data, int length, const InventoryView *view) {
    Inventory authoritative;
    uint32_t revision, acknowledged;
    if (!InventoryProtocol_ReadState(data, length, &authoritative, &revision, &acknowledged)) return;
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
    displayedInventory = authoritative;
    displayedView = view ? *view : (InventoryView){0};
    for (int i = 0; i < pendingCount; i++) {
        if (!displayedView.session && !pendingActions[i].x)
            Inventory_ApplyAction(&displayedInventory, &pendingActions[i]);
    }
    if (acknowledgedClose && displayedInventory.open) closeBlocked = true;
    if (!displayedInventory.cursor.count) closeBlocked = false;
    ApplyScreenState();
}
void ClientInventory_HandleState(void) {
    HandleState(packetData, packetDataLength, NULL);
}
void ClientInventory_HandleView(void) {
    if (packetDataLength <= 1 + INVENTORY_STATE_PACKET_SIZE || packetData[0] != PACKET_INVENTORY_VIEW) return;
    BinaryReader in = {packetData + 1 + INVENTORY_STATE_PACKET_SIZE, packetDataLength - 1 - INVENTORY_STATE_PACKET_SIZE};
    InventoryView view;
    if (!InventoryView_Read(&in, &view) || !Binary_End(&in) || packetData[11] != 1) return;
    HandleState(packetData + 1, INVENTORY_STATE_PACKET_SIZE, &view);
}
const InventoryView *ClientInventory_GetView(void) { return displayedView.session ? &displayedView : NULL; }
void ClientInventory_ClickView(bool container, int slot, bool right, bool shift) {
    if (!displayedView.session || slot < 0 || slot >= (container ? displayedView.slotCount : INVENTORY_SLOT_COUNT)) return;
    QueueAction((InventoryAction){.type = shift ? INVENTORY_VIEW_SHIFT : right ? INVENTORY_VIEW_RIGHT : INVENTORY_VIEW_LEFT,
        .slot = slot, .y = container});
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

void ClientInventory_Interact(bool place, Vector3 hit, Vector3 normal, int targetBlock) {
    if (targetBlock <= 0 || displayedInventory.open) return;
    InventoryAction action = {
        .type = place ? INVENTORY_PLACE : INVENTORY_BREAK,
        .x = (int)floorf(hit.x), .y = (int)floorf(hit.y), .z = (int)floorf(hit.z),
        .targetBlock = targetBlock
    };
    if (normal.x) action.face = normal.x > 0 ? 1 : 0;
    else if (normal.y) action.face = normal.y > 0 ? 3 : 2;
    else if (normal.z) action.face = normal.z > 0 ? 5 : 4;
    else return;
    action.hit[0] = (uint8_t)((hit.x - floorf(hit.x)) * 255);
    action.hit[1] = (uint8_t)((hit.y - floorf(hit.y)) * 255);
    action.hit[2] = (uint8_t)((hit.z - floorf(hit.z)) * 255);
    QueueAction(action);
}

bool ClientInventory_CloseBlocked(void) {
    return closeBlocked;
}
