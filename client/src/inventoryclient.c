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
static bool digging, digApproved;
static Vector3 digPosition;
static ItemStack digStack;
static int digBlock, digSlot;
static uint32_t digSequence;
static double digStart, digDuration;


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
    digging=digApproved=false;
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
        if (!displayedView.session && !pendingActions[i].x)
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
        .slot = slot, .y = binding});
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

void ClientInventory_Interact(bool place, Vector3 hit, Vector3 normal, int targetBlock) {
    if (displayedInventory.open) return;
    if (targetBlock<=0) {
        if (place) QueueAction((InventoryAction){.type=INVENTORY_USE});
        return;
    }
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
    if (QueueAction(action) && !place) digSequence=nextSequence-1;
}

bool ClientInventory_CloseBlocked(void) {
    return closeBlocked;
}

void ClientInventory_Dig(bool held, Vector3 hit, Vector3 normal, int block) {
    Vector3 position={floorf(hit.x),floorf(hit.y),floorf(hit.z)};
    ItemStack stack=*Inventory_GetSelected(&displayedInventory);
    bool changed=position.x!=digPosition.x || position.y!=digPosition.y || position.z!=digPosition.z ||
        block!=digBlock || digSlot!=displayedInventory.selectedHotbar ||
        stack.count!=digStack.count || !ItemStack_Matches(stack,digStack);
    if (digging && (!held || changed || displayedInventory.open)) {
        if (!QueueAction((InventoryAction){.type=INVENTORY_CANCEL_DIG})) return;
        digging=digApproved=false;
    }
    if (!digging && held && block>0 && !displayedInventory.open && ready && pendingCount<MAX_PENDING_ACTIONS) {
        digPosition=position; digBlock=block; digSlot=displayedInventory.selectedHotbar; digStack=stack;
        uint32_t before=nextSequence;
        ClientInventory_Interact(false,hit,normal,block);
        digging=nextSequence!=before; digApproved=false;
    }
}
void ClientInventory_SetDigProgress(uint32_t sequence, int milliseconds) {
    if (!digging || sequence != digSequence) return;
    digApproved=milliseconds>=0;
    digStart=GetTime(); digDuration=milliseconds/1000.0;
}
float ClientInventory_DigProgress(Vector3 *position) {
    if (!digging || !digApproved) return -1;
    *position=digPosition;
    return digDuration>0 ? fminf(1,(GetTime()-digStart)/digDuration) : 1;
}
