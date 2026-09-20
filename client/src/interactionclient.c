/**
 * Copyright (c) 2026 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */
#include <math.h>
#include "interactionclient.h"
#include "inventoryclient.h"
#include "worldaction.h"
#include "networkhandler.h"
#include "packet.h"
#define MAX_PENDING_WORLD_ACTIONS 64
static WorldAction pending[MAX_PENDING_WORLD_ACTIONS];
static int pendingCount;
static uint32_t nextSequence = 1, digSequence;
static double retryTime, digStart, digDuration;
static bool digging, approved;
static Vector3 digPosition;
static ItemStack digStack;
static int digBlock, digSlot;

void ClientInteraction_Reset(void) {
    pendingCount = 0;
    nextSequence = 1;
    digging = false;
    approved = false;
    retryTime = 0;
}

static void Send(const WorldAction *action) {
    unsigned char *packet = Packet_CreateWorldAction(action);
    if (packet) Network_Send(packet);
}

static uint32_t Queue(WorldAction action) {
    if (!ClientInventory_Ready() || pendingCount == MAX_PENDING_WORLD_ACTIONS || nextSequence == 0) return 0;
    action.sequence = nextSequence++;
    action.inventorySequence = ClientInventory_Sequence();
    if (!pendingCount) retryTime = GetTime();
    pending[pendingCount++] = action;
    Send(&action);
    return action.sequence;
}

void ClientInteraction_Update(void) {
    if (!networkConnectedToServer) return;
    if (pendingCount && GetTime() - retryTime > 0.5) {
        for (int i = 0; i < pendingCount; i++) Send(&pending[i]);
        retryTime = GetTime();
    }
}

void ClientInteraction_Acknowledge(uint32_t sequence) {
    int remaining = 0;
    for (int i = 0; i < pendingCount; i++) {
        if (pending[i].sequence > sequence) {
            pending[remaining++] = pending[i];
        }
    }
    pendingCount = remaining;
}

static WorldAction Target(int type, bool clicked, Vector3 hit, Vector3 normal, int block) {
    WorldAction action = {.type = type, .clicked = clicked};
    if (block <= 0) return action;

    action.x = floorf(hit.x);
    action.y = floorf(hit.y);
    action.z = floorf(hit.z);
    action.targetBlock = block;
    if (normal.x) action.face = normal.x > 0 ? 1 : 0;
    else if (normal.y) action.face = normal.y > 0 ? 3 : 2;
    else if (normal.z) action.face = normal.z > 0 ? 5 : 4;
    action.hit[0] = (hit.x - floorf(hit.x)) * 255;
    action.hit[1] = (hit.y - floorf(hit.y)) * 255;
    action.hit[2] = (hit.z - floorf(hit.z)) * 255;
    return action;
}

void ClientInteraction_Use(Vector3 hit, Vector3 normal, int block) {
    if (!ClientInventory_IsOpen()) Queue(Target(WORLD_USE, true, hit, normal, block));
}

void ClientInteraction_Dig(bool held, bool clicked, Vector3 hit, Vector3 normal, int block) {
    const Inventory *inventory = ClientInventory_Get();
    ItemStack stack = inventory->slots[INVENTORY_STORAGE_SLOTS + inventory->selectedHotbar];
    Vector3 position = {floorf(hit.x), floorf(hit.y), floorf(hit.z)};
    bool changed = position.x != digPosition.x || position.y != digPosition.y || position.z != digPosition.z ||
        block != digBlock || inventory->selectedHotbar != digSlot ||
        stack.count != digStack.count || !ItemStack_Matches(stack, digStack);
    if (digging && (!held || changed || inventory->open)) {
        if (!Queue((WorldAction){.type = WORLD_CANCEL_DIG})) return;
        digging = false;
        approved = false;
    }
    if (!digging && !inventory->open && ((held && block > 0) || clicked)) {
        uint32_t sequence = Queue(Target(WORLD_START_DIG, clicked, hit, normal, block));
        if (sequence && block > 0) {
            digSequence = sequence;
            digging = true;
            approved = false;
            digPosition = position;
            digBlock = block;
            digSlot = inventory->selectedHotbar;
            digStack = stack;
        }
    }
}

void ClientInteraction_SetDigProgress(uint32_t sequence, int milliseconds) {
    if (!digging || sequence != digSequence) return;
    approved = milliseconds >= 0;
    digStart = GetTime();
    digDuration = milliseconds / 1000.0;
}

float ClientInteraction_DigProgress(Vector3 *position) {
    if (!digging || !approved) return -1;
    *position = digPosition;
    return digDuration > 0 ? fminf(1, (GetTime() - digStart) / digDuration) : 1;
}
