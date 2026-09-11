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
#include "hudbarprotocol.h"
#include "world/chunk/chunkmetadata.h"
#include "inventorywindow.h"
#include "playerinventories.h"

#define PLAYER_METADATA_GROUPS 16
typedef struct PlayerMetadata { char name[65]; Metadata value; } PlayerMetadata;

typedef struct ChunkRequest { Vector3 position; float distanceSquared; } ChunkRequest;

#define PLAYER_CHUNK_REQUESTS 16
#define PLAYER_LIGHT_REQUESTS 32

typedef struct Player {
    Vector3 spawnPoint;
    Vector3 savedPosition;
    bool hasSavedPosition;
    HudBarState hudBars[HUD_BAR_LIMIT];
    Inventory inventory;
    PlayerMetadata metadata[PLAYER_METADATA_GROUPS];
    uint8_t metadataCount;
    bool digging;
    InventoryAction digAction;
    ItemStack digStack;
    int digSlot;
    double digEnd;
    NamedInventory namedInventories[PLAYER_INVENTORIES];
    uint8_t namedInventoryCount;
    uint32_t inventoryRevision, inventorySequence;
    InventoryWindow inventoryWindow;
    uint32_t nextInventorySession;
    bool inventoryLoaded, leaveInvoked;
    bool movementReady, falling;
    Vector3 impulseAllowance; // Extra horizontal/up/down distance, consumed by accepted moves.
    double impulseExpires;
    double movementTime;
    double movementReceivedTime;
    double movementLogTime;
    double nextMeleeAttack;
    float horizontalAllowance, upAllowance, downAllowance, fallPeak;
    unsigned char id;
    int entityId;
    char texture[65];
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
    ChunkRequest *chunkRequests;
    int chunkRequestCursor, chunkRequestDistance;
    Vector3 chunkRequestCenter;
    Vector3 pendingChunks[PLAYER_CHUNK_REQUESTS];
    int pendingChunkCount;
    Vector3 lightingChunks[PLAYER_LIGHT_REQUESTS];
    int lightingChunkCount;
    double chunkRetryTime;
} Player;

Player *ServerPlayer_Create(void *peer, bool isWeb);
void ServerPlayer_Destroy(Player *player);
void ServerPlayer_UpdatePositionRotation(Player* player, Vector3 position, Vector3 rotation);
void ServerPlayer_ResetMovement(Player *player);
bool ServerPlayer_ApplyImpulse(Player *player, Vector3 impulse);
bool ServerPlayer_FindSpawnPoint(Vector3 *position);
void ServerPlayer_LoadChunks(Player* player);
void ServerPlayer_Teleport(Player *player, Vector3 position);
void ServerPlayer_SendMessage(Player *player, const char *message);

// Send block definitions to this player.
void ServerPlayer_DefineBlock(Player *player, int id, const BlockDefinition *definition);
void ServerPlayer_RemoveBlockDefinition(Player *player, int id);

#endif
