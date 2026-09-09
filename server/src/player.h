/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SERVER_PLAYER_H
#define MIDLESS_SERVER_PLAYER_H

#include "raylib.h"
#include "blockdefinition.h"
#include "textureprotocol.h"
#include "inventory.h"
#include "inventorywindow.h"
#include "playerinventories.h"

typedef struct Player {
    Inventory inventory;
    NamedInventory namedInventories[PLAYER_INVENTORIES];
    uint8_t namedInventoryCount;
    uint32_t inventoryRevision, inventorySequence;
    InventoryWindow inventoryWindow;
    uint32_t nextInventorySession;
    bool inventoryLoaded, leaveInvoked;
    unsigned char id;
    int entityId;
    uint32_t textureSent[TEXTURE_LIMIT], textureRevision, textureOffset;
    int textureId;
    bool textureWaiting;
    double textureLastSend;
    uint64_t connectionId;
    void *peer;
    char *name;
    int drawDistance;
    bool isWeb;
    bool disconnected;
    int pendingPackets;
    bool chunkRequestPending;
    Vector3 pendingChunkPosition;
} Player;

Player *ServerPlayer_Create(void *peer, bool isWeb);
void ServerPlayer_Destroy(Player *player);
void ServerPlayer_UpdatePositionRotation(Player* player, Vector3 position, Vector3 rotation);
void ServerPlayer_LoadChunks(Player* player);
void ServerPlayer_Teleport(Player *player, Vector3 position);
void ServerPlayer_SendMessage(Player *player, const char *message);

// Send block definitions to this player.
void ServerPlayer_DefineBlock(Player *player, int id, const BlockDefinition *definition);
void ServerPlayer_RemoveBlockDefinition(Player *player, int id);

#endif
