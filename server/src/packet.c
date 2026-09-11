#include "blockstates.h"
#include "version.h"
#include "playerimpulse.h"
/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "raylib.h"
#include "packet.h"
#include "networkhandler.h"
#include "world/world.h"
#include "world/chunk/chunk.h"
#include "entity.h"
#include "entitytexture.h"
#include "logger.h"
#include "luabindings.h"
#include "rotation.h"
#include "world/textures.h"
#include "serverinventory.h"
#include "inventoryprotocol.h"
#include "items.h"
#include "hudbars.h"
#include "textcolors.h"


unsigned char *serverPacketData;
Player *serverPacketPlayer;
int serverPacketLastDynamicLength;
int serverPacketDataLength;

int serverPacketLengths[256] = {
    MAP_INIT_PACKET_SIZE, // 0
    PACKET_VARIABLE_SIZE, // 1
    SET_BLOCK_PACKET_SIZE, // 2
    SPAWN_ENTITY_PACKET_SIZE, // 3
    TELEPORT_ENTITY_PACKET_SIZE, // 4
    MESSAGE_PACKET_SIZE, // 5
    DESPAWN_ENTITY_PACKET_SIZE, // 6
    UNLOAD_CHUNK_PACKET_SIZE, // 7
    PACKET_VARIABLE_SIZE, // 8
    WORLD_TIME_PACKET_SIZE, // 9
    MESSAGE_CONTINUATION_PACKET_SIZE, // 10
    ENTITY_ANIMATION_PACKET_SIZE, // 11
    DEFINE_BLOCK_PACKET_SIZE, // 12
    REMOVE_BLOCK_DEFINITION_PACKET_SIZE, // 13
    PACKET_VARIABLE_SIZE, // 14
    REMOVE_ENTITY_MODEL_PACKET_SIZE, // 15
    SET_ENTITY_MODEL_PACKET_SIZE, // 16
    TEXTURE_BEGIN_SIZE, // 17
    TEXTURE_DATA_SIZE, // 18
    TERRAIN_TEXTURE_PACKET_SIZE, // 19
    HELD_BLOCK_PACKET_SIZE, // 20
    INVENTORY_STATE_PACKET_SIZE, // 21
    DROPPED_ITEM_PACKET_SIZE, // 22
    PACKET_VARIABLE_SIZE, // 23
    ITEM_DEFINITION_PACKET_SIZE, // 24
    DIG_PROGRESS_PACKET_SIZE, // 25
    BREAKING_TEXTURE_PACKET_SIZE, // 26
    HUD_BAR_DEFINE_SIZE, // 27
    HUD_BAR_STATE_SIZE, // 28
    HUD_BAR_REMOVE_SIZE, // 29
    TEXT_COLOR_PACKET_SIZE, // 30
    NAMETAG_PACKET_SIZE, // 31
    PLAYER_IMPULSE_PACKET_SIZE, // 32
    SET_ENTITY_TEXTURE_PACKET_SIZE, // 33
    PACKET_VARIABLE_SIZE, // 34
    RESET_CHUNKS_PACKET_SIZE, // 35
};

int ServerPacket_GetLength(unsigned char opcode) {
    return serverPacketLengths[opcode];
}

//Packet Readers

int serverPacketReaderIndex = 1;

const unsigned char *ServerPacket_ReadBytes(int size) {
    if (size < 0 || serverPacketReaderIndex > serverPacketDataLength - size) return NULL;
    const unsigned char *bytes = serverPacketData + serverPacketReaderIndex;
    serverPacketReaderIndex += size;
    return bytes;
}

uint32_t ServerPacket_ReadUInt(void) {
    const unsigned char *bytes = ServerPacket_ReadBytes(4);
    if (!bytes) return 0;
    return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 | (uint32_t)bytes[2] << 8 | bytes[3];
}

unsigned char ServerPacket_ReadByte(void) {
    if (serverPacketReaderIndex >= serverPacketDataLength) return 0;
    return serverPacketData[serverPacketReaderIndex++];
}

short ServerPacket_ReadShort(void) {
    if (serverPacketReaderIndex > serverPacketDataLength - 2) return 0;
    short value = (short)(serverPacketData[serverPacketReaderIndex] << 8 | serverPacketData[serverPacketReaderIndex + 1]);
    serverPacketReaderIndex += 2;
    return value;
}

unsigned short ServerPacket_ReadUShort(void) {
    if (serverPacketReaderIndex > serverPacketDataLength - 2) return 0;
    unsigned short value = (unsigned short)(serverPacketData[serverPacketReaderIndex] << 8 | serverPacketData[serverPacketReaderIndex + 1]);
    serverPacketReaderIndex += 2;
    return value;
}

int ServerPacket_ReadInt(void) { return (int32_t)ServerPacket_ReadUInt(); }

char* ServerPacket_ReadString(void) {
    char *string = MemAlloc(PACKET_STRING_SIZE + 1);
    if (string == NULL) return NULL;
    if (serverPacketReaderIndex > serverPacketDataLength - PACKET_STRING_SIZE) {
        string[0] = 0;
        return string;
    }
    
    for (int i = 0; i < PACKET_STRING_SIZE; i++) {
        string[i] = serverPacketData[serverPacketReaderIndex++];
    }
    
    string[PACKET_STRING_SIZE] = 0;
    
    return string;
}

unsigned char* ServerPacket_ReadArray(int size) {
    if (size < 0 || serverPacketReaderIndex > serverPacketDataLength - size) return NULL;
    unsigned char *arr = MemAlloc(size);
    if (arr == NULL) return NULL;
    memcpy(arr, &serverPacketData[serverPacketReaderIndex], size);
    serverPacketReaderIndex += size;
    return arr;
}

//Packet Writers

int serverPacketWriterIndex = 0;

void ServerPacket_WriteByte(unsigned char* packet, unsigned char value) {
    packet[serverPacketWriterIndex++] = value;
}

void ServerPacket_WriteShort(unsigned char* packet, short value) {
    packet[serverPacketWriterIndex++] = (char)(value >> 8);
	packet[serverPacketWriterIndex++] = (char)(value);
}

void ServerPacket_WriteUShort(unsigned char* packet, unsigned short value) {
    packet[serverPacketWriterIndex++] = (char)(value >> 8);
	packet[serverPacketWriterIndex++] = (char)(value);
}

void ServerPacket_WriteInt(unsigned char* packet, int value) {
    packet[serverPacketWriterIndex++] = (char)(value >> 24);
	packet[serverPacketWriterIndex++] = (char)(value >> 16);
    packet[serverPacketWriterIndex++] = (char)(value >> 8);
    packet[serverPacketWriterIndex++] = (char)(value);
}

void ServerPacket_WriteString(unsigned char *packet, const char *string) {
    int length = TextLength(string);
    for (int i = 0; i < PACKET_STRING_SIZE; i++) {
        if (i < length) {
            packet[serverPacketWriterIndex++] = string[i];
        } else {
            packet[serverPacketWriterIndex++] = 0;
        }  
    }
}

void ServerPacket_WriteArray(unsigned char* packet, unsigned char* array, int size) {
    for (int i = 0; i < size; i++) {
        packet[serverPacketWriterIndex++] = array[i];
    }
}

/* Packets Received */

void ServerPacket_HandleIdentification(void) {
    if(serverPacketPlayer->name != NULL) return;
    int protocolVersion = ServerPacket_ReadUShort();
    if (protocolVersion != GAME_PROTOCOL_VERSION) {
        ServerLogger_Log("Rejected incompatible protocol version.\n");
        ServerNetwork_Send(serverPacketPlayer, ServerPacket_CreateMessage("Incompatible protocol; update your client."));
        return;
    }
    serverPacketPlayer->name = ServerPacket_ReadString();
    if (!ServerItems_Ready() || !ServerInventory_Load(serverPacketPlayer)) {
        ServerNetwork_Send(serverPacketPlayer, ServerPacket_CreateMessage("Cannot load inventory, or that name is already connected."));
        MemFree(serverPacketPlayer->name);
        serverPacketPlayer->name = NULL;
        return;
    }
    ServerLogger_Log(TextFormat("%s connected. Protocol version: %i\n", serverPacketPlayer->name, protocolVersion));
    ServerNetwork_Send(serverPacketPlayer, ServerPacket_CreateMapInit());
    ServerItems_Send(serverPacketPlayer);
    ServerTextures_SendTerrain(serverPacketPlayer);
    ServerTextures_SendBreaking(serverPacketPlayer);
    ServerHudBars_Send(serverPacketPlayer);
    ServerTextColors_Send(serverPacketPlayer);
    ServerWorld_SendEntityModels(serverPacketPlayer);
    ServerWorld_AddPlayer(serverPacketPlayer);
    if (serverPacketPlayer->entityId < 0) {
        ServerNetwork_Send(serverPacketPlayer, ServerPacket_CreateMessage("World is full: no player or entity slots available."));
        MemFree(serverPacketPlayer->name);
        serverPacketPlayer->name = NULL;
        return;
    }
    ServerWorld_SendBlockDefinitions(serverPacketPlayer);
    ServerInventory_UpdateHeldBlock(serverPacketPlayer);
    ServerInventory_Send(serverPacketPlayer);
    ServerNetwork_Send(serverPacketPlayer, ServerPacket_CreateWorldTime(serverWorld.time));
    if (serverWorld.players[serverPacketPlayer->id] == serverPacketPlayer)
        LuaBindings_InvokePlayerJoin(serverPacketPlayer->id);
}



void ServerPacket_HandlePlayerPosition(void) {
    Vector3 position = (Vector3) { ServerPacket_ReadInt() / 64.0f, ServerPacket_ReadInt() / 64.0f, ServerPacket_ReadInt() / 64.0f };
    Vector3 rotation = {0};
    rotation.x = Rotation_Decode(ServerPacket_ReadByte());
    rotation.y = Rotation_Decode(ServerPacket_ReadByte());
    rotation.z = Rotation_Decode(ServerPacket_ReadByte());
    ServerPlayer_UpdatePositionRotation(serverPacketPlayer, position, rotation);
}

void ServerPacket_HandleMessage(void) {
    char *message = ServerPacket_ReadString();
    if (LuaBindings_InvokeChatMessage(serverPacketPlayer->id, message)) {
        MemFree(message);
        return;
    }
    
    char name[PACKET_STRING_SIZE * 2 + 1];
    int nameLen = TextColor_Escape(name, serverPacketPlayer->name);
    char* sentMessage = MemAlloc(nameLen + 3 + 64 + 1);
    
    //username
    sentMessage[0] = '<';
    memcpy(&sentMessage[1], name, nameLen);
    sentMessage[nameLen + 1] = '>';
    sentMessage[nameLen + 2] = ' ';
    
    //message
    memcpy(&sentMessage[nameLen + 3], message, TextLength(message));
    
    //end string
    sentMessage[nameLen + 3 + TextLength(message)] = 0;
    
    ServerWorld_SendMessage(sentMessage);
    MemFree(sentMessage);
    MemFree(message);
}

void ServerPacket_HandleSetDrawDistance(void) {
    unsigned char distance = ServerPacket_ReadByte();
    if (distance > serverWorld.maxDrawDistance) distance = serverWorld.maxDrawDistance;
    if (distance < 2) distance = 2;
    serverPacketPlayer->drawDistance = distance;
}

void ServerPacket_HandlePlayerClick(void) {
    unsigned char button = ServerPacket_ReadByte();
    if (button > 1) return;
    EntityAnimationType animation = ENTITY_ANIMATION_SWING_RIGHT_ARM;
    ServerWorld_BroadcastExcluding(
        ServerPacket_CreateEntityAnimation(serverPacketPlayer->entityId, animation),
        serverPacketPlayer->id
    );
    LuaBindings_InvokePlayerClick(serverPacketPlayer->id, button);
}

void ServerPacket_HandleTextureAck(void) {
    if (serverPacketDataLength != TEXTURE_ACK_SIZE) return;
    int id = ServerPacket_ReadUShort();
    uint32_t revision = ServerPacket_ReadUInt();
    uint32_t offset = ServerPacket_ReadUInt();
    ServerTextures_Acknowledge(serverPacketPlayer, id, revision, offset);
}

void ServerPacket_HandleInventoryAction(void) {
    if (serverPacketDataLength != INVENTORY_ACTION_PACKET_SIZE) return;
    InventoryAction action = {0};
    action.sequence = ServerPacket_ReadUInt();
    action.type = ServerPacket_ReadByte();
    action.slot = ServerPacket_ReadByte();
    action.x = ServerPacket_ReadInt();
    action.y = ServerPacket_ReadInt();
    action.z = ServerPacket_ReadInt();
    action.face = ServerPacket_ReadByte();
    action.targetBlock = ServerPacket_ReadUShort();
    for (int i = 0; i < 3; i++) action.hit[i] = ServerPacket_ReadByte();
    ServerInventory_ApplyAction(serverPacketPlayer, action);
}



/* Packets sent */

unsigned char* ServerPacket_CreateMapInit(void) {
    serverPacketWriterIndex = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(serverPacketLengths[0]);
    ServerPacket_WriteByte(packet, 0);
    ServerPacket_WriteUShort(packet, GAME_PROTOCOL_VERSION);
    
    return packet;
}

unsigned char* ServerPacket_CreateLoadChunk(unsigned short* chunkArray, unsigned short length,
                                            Vector3 chunkPosition, const unsigned char *skyMask) {
    serverPacketWriterIndex = 0;
    serverPacketLastDynamicLength = (length * 2) + LOAD_CHUNK_HEADER_SIZE + CHUNK_SKY_MASK_SIZE;
    unsigned char* packet = (unsigned char*)MemAlloc(serverPacketLastDynamicLength);
    ServerPacket_WriteByte(packet, 1);
    ServerPacket_WriteInt(packet, (int)chunkPosition.x);
    ServerPacket_WriteInt(packet, (int)chunkPosition.y);
    ServerPacket_WriteInt(packet, (int)chunkPosition.z);
    ServerPacket_WriteUShort(packet, length);
    ServerPacket_WriteArray(packet, (unsigned char*)chunkArray, length * 2);
    ServerPacket_WriteArray(packet, (unsigned char*)skyMask, CHUNK_SKY_MASK_SIZE);
    return packet;
}

unsigned char* ServerPacket_CreateUnloadChunk(Vector3 chunkPosition) {
    serverPacketWriterIndex = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(serverPacketLengths[7]);
    ServerPacket_WriteByte(packet, 7);
    ServerPacket_WriteInt(packet, (int)chunkPosition.x);
    ServerPacket_WriteInt(packet, (int)chunkPosition.y);
    ServerPacket_WriteInt(packet, (int)chunkPosition.z);
    return packet;
}

unsigned char* ServerPacket_CreateSetBlock(unsigned char blockId, Vector3 position, bool byPlayer) {
    serverPacketWriterIndex = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(serverPacketLengths[2]);
    ServerPacket_WriteByte(packet, 2);
    ServerPacket_WriteUShort(packet, ServerBlockStates_WireId(blockId, position));
    ServerPacket_WriteInt(packet, (int)position.x);
    ServerPacket_WriteInt(packet, (int)position.y);
    ServerPacket_WriteInt(packet, (int)position.z);
    ServerPacket_WriteByte(packet, byPlayer);
    return packet;
}

unsigned char* ServerPacket_CreateBlockBatch(const ServerBlockUpdate *updates, unsigned short count) {
    serverPacketWriterIndex = 0;
    serverPacketLastDynamicLength = BLOCK_BATCH_HEADER_SIZE + count * BLOCK_BATCH_UPDATE_SIZE;
    unsigned char *packet = MemAlloc(serverPacketLastDynamicLength);
    ServerPacket_WriteByte(packet, 8);
    ServerPacket_WriteUShort(packet, count);
    for (int i = 0; i < count; i++) {
        ServerPacket_WriteUShort(packet, ServerBlockStates_WireId(updates[i].blockId, updates[i].position));
        ServerPacket_WriteInt(packet, (int)updates[i].position.x);
        ServerPacket_WriteInt(packet, (int)updates[i].position.y);
        ServerPacket_WriteInt(packet, (int)updates[i].position.z);
    }
    return packet;
}

unsigned char* ServerPacket_CreateSpawnEntity(Entity *entity) {
    serverPacketWriterIndex = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(serverPacketLengths[3]);
    ServerPacket_WriteByte(packet, 3);
    ServerPacket_WriteUShort(packet, entity->id);
    ServerPacket_WriteByte(packet, entity->type);
    ServerPacket_WriteByte(packet, entity->model);
    ServerPacket_WriteInt(packet, (int)(entity->position.x * 64));
    ServerPacket_WriteInt(packet, (int)(entity->position.y * 64));
    ServerPacket_WriteInt(packet, (int)(entity->position.z * 64));
    ServerPacket_WriteUShort(packet, entity->heldBlock);
    ServerPacket_WriteUShort(packet, ServerEntityTexture_Id(entity));
    return packet;
}

unsigned char* ServerPacket_CreateDespawnEntity(Entity *entity) {
    serverPacketWriterIndex = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(serverPacketLengths[6]);
    ServerPacket_WriteByte(packet, 6);
    ServerPacket_WriteUShort(packet, entity->id);
    return packet;
}

unsigned char* ServerPacket_CreateTeleportEntity(Entity *entity, Vector3 position, Vector3 rotation) {
    serverPacketWriterIndex = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(serverPacketLengths[4]);
    ServerPacket_WriteByte(packet, 4);
    ServerPacket_WriteUShort(packet, entity->id);
    ServerPacket_WriteInt(packet, (int)(position.x * 64));
    ServerPacket_WriteInt(packet, (int)(position.y * 64));
    ServerPacket_WriteInt(packet, (int)(position.z * 64));
    ServerPacket_WriteByte(packet, Rotation_Encode(rotation.x));
    ServerPacket_WriteByte(packet, Rotation_Encode(rotation.y));
    ServerPacket_WriteByte(packet, Rotation_Encode(rotation.z));
    return packet;
}

unsigned char* ServerPacket_CreateMessage(const char* message) {
    serverPacketWriterIndex = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(serverPacketLengths[5]);
    ServerPacket_WriteByte(packet, 5);
    ServerPacket_WriteString(packet, message);
    return packet;
}

unsigned char* ServerPacket_CreateMessageContinuation(const char* message) {
    serverPacketWriterIndex = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(serverPacketLengths[10]);
    ServerPacket_WriteByte(packet, 10);
    ServerPacket_WriteString(packet, message);
    return packet;
}

unsigned char* ServerPacket_CreateWorldTime(float timeSeconds) {
    serverPacketWriterIndex = 0;
    unsigned char *packet = MemAlloc(serverPacketLengths[9]);
    ServerPacket_WriteByte(packet, 9);
    ServerPacket_WriteInt(packet, (int)(timeSeconds * 1000.0f));
    return packet;
}

unsigned char *ServerPacket_CreatePlayerImpulse(Vector3 impulse) {
    if (!PlayerImpulse_Valid(impulse)) return NULL;
    unsigned char *packet = MemAlloc(PLAYER_IMPULSE_PACKET_SIZE);
    if (!packet) return NULL;
    serverPacketWriterIndex = 0;
    ServerPacket_WriteByte(packet, PACKET_PLAYER_IMPULSE);
    ServerPacket_WriteInt(packet, (int)roundf(impulse.x * PLAYER_IMPULSE_SCALE));
    ServerPacket_WriteInt(packet, (int)roundf(impulse.y * PLAYER_IMPULSE_SCALE));
    ServerPacket_WriteInt(packet, (int)roundf(impulse.z * PLAYER_IMPULSE_SCALE));
    return packet;
}

unsigned char* ServerPacket_CreateEntityAnimation(unsigned short entityId, EntityAnimationType animation) {
    serverPacketWriterIndex = 0;
    unsigned char *packet = MemAlloc(serverPacketLengths[11]);
    ServerPacket_WriteByte(packet, 11);
    ServerPacket_WriteUShort(packet, entityId);
    ServerPacket_WriteByte(packet, (unsigned char)animation);
    return packet;
}

unsigned char *ServerPacket_CreateDefineBlock(int id, const BlockDefinition *definition) {
    if (!BlockDefinition_Validate(id, definition)) return NULL;
    serverPacketWriterIndex = 0;
    unsigned char *packet = MemAlloc(DEFINE_BLOCK_PACKET_SIZE);
    if (!packet) return NULL;
    ServerPacket_WriteByte(packet, PACKET_DEFINE_BLOCK);
    ServerPacket_WriteUShort(packet, id);
    ServerPacket_WriteString(packet, definition->name);
    for (int i = 0; i < 6; i++) ServerPacket_WriteByte(packet, definition->textures[i]);
    ServerPacket_WriteByte(packet, definition->modelType);
    ServerPacket_WriteByte(packet, definition->renderType);
    ServerPacket_WriteByte(packet, definition->colliderType);
    ServerPacket_WriteByte(packet, definition->lightType);
    for (int i = 0; i < 3; i++) ServerPacket_WriteByte(packet, definition->min[i]);
    for (int i = 0; i < 3; i++) ServerPacket_WriteByte(packet, definition->max[i]);
    uint8_t geometry[BLOCK_GEOMETRY_BYTES];
    BlockGeometry_Encode(geometry, &definition->geometry);
    ServerPacket_WriteArray(packet, geometry, sizeof(geometry));
    return packet;
}

unsigned char *ServerPacket_CreateRemoveBlockDefinition(int id) {
    if (id < 1 || id > 255) return NULL;
    serverPacketWriterIndex = 0;
    unsigned char *packet = MemAlloc(REMOVE_BLOCK_DEFINITION_PACKET_SIZE);
    if (!packet) return NULL;
    ServerPacket_WriteByte(packet, PACKET_REMOVE_BLOCK_DEFINITION);
    ServerPacket_WriteByte(packet, (unsigned char)id);
    return packet;
}

unsigned char *ServerPacket_CreateDefineEntityModel(int id, const ModelDefinition *d) {
    if (!ModelDefinition_Validate(id, d)) return NULL;
    serverPacketWriterIndex = 0;
    serverPacketLastDynamicLength = ENTITY_MODEL_HEADER_SIZE + d->partCount * ENTITY_MODEL_PART_SIZE;
    unsigned char *packet = MemAlloc(serverPacketLastDynamicLength);
    if (!packet) return NULL;
    ServerPacket_WriteByte(packet, PACKET_DEFINE_ENTITY_MODEL);
    ServerPacket_WriteByte(packet, id);
    ServerPacket_WriteString(packet, d->name);
    ServerPacket_WriteUShort(packet, d->texture);
    ServerPacket_WriteByte(packet, d->partCount);
    for (int i = 0; i < d->partCount; i++) {
        const ModelPartDefinition *p = &d->parts[i];
        ServerPacket_WriteByte(packet, p->role);
        ServerPacket_WriteByte(packet, p->firstPersonVisible);
        ServerPacket_WriteByte(packet, p->hasGrip);
        for (int a = 0; a < 3; a++) ServerPacket_WriteShort(packet, p->grip[a]);
        for (int a = 0; a < 3; a++) ServerPacket_WriteShort(packet, p->position[a]);
        for (int a = 0; a < 3; a++) ServerPacket_WriteShort(packet, p->min[a]);
        for (int a = 0; a < 3; a++) ServerPacket_WriteShort(packet, p->max[a]);
        for (int f = 0; f < 6; f++) for (int a = 0; a < 4; a++) ServerPacket_WriteShort(packet, p->uv[f][a]);
    }
    return packet;
}
unsigned char *ServerPacket_CreateRemoveEntityModel(int id) {
    if (id < 1 || id > 255) return NULL;
    serverPacketWriterIndex = 0;
    unsigned char *packet = MemAlloc(REMOVE_ENTITY_MODEL_PACKET_SIZE);
    if (!packet) return NULL;
    ServerPacket_WriteByte(packet, PACKET_REMOVE_ENTITY_MODEL);
    ServerPacket_WriteByte(packet, id);
    return packet;
}
unsigned char *ServerPacket_CreateSetEntityModel(unsigned short entityId, unsigned char modelId) {
    serverPacketWriterIndex = 0;
    unsigned char *packet = MemAlloc(SET_ENTITY_MODEL_PACKET_SIZE);
    if (!packet) return NULL;
    ServerPacket_WriteByte(packet, PACKET_SET_ENTITY_MODEL);
    ServerPacket_WriteUShort(packet, entityId);
    ServerPacket_WriteByte(packet, modelId);
    return packet;
}

unsigned char *ServerPacket_CreateHeldBlock(Entity *entity) {
    serverPacketWriterIndex = 0;
    unsigned char *packet = MemAlloc(HELD_BLOCK_PACKET_SIZE);
    ServerPacket_WriteByte(packet, 20);
    ServerPacket_WriteUShort(packet, entity->id);
    ServerPacket_WriteUShort(packet, entity->heldBlock);
    return packet;
}

unsigned char *ServerPacket_CreateDroppedItem(Entity *entity) {
    serverPacketWriterIndex = 0;
    unsigned char *packet = MemAlloc(DROPPED_ITEM_PACKET_SIZE);
    if (!packet) return NULL;
    ServerPacket_WriteByte(packet, PACKET_DROPPED_ITEM);
    ServerPacket_WriteUShort(packet, entity->id);
    ServerPacket_WriteUShort(packet, entity->drop.stack.itemId);
    ServerPacket_WriteByte(packet, entity->drop.stack.count);
    ServerPacket_WriteInt(packet, (int)(entity->position.x * 64));
    ServerPacket_WriteInt(packet, (int)(entity->position.y * 64));
    ServerPacket_WriteInt(packet, (int)(entity->position.z * 64));
    packet[18] = entity->drop.stack.metadataSize;
    packet[19] = entity->drop.stack.metadataVersion >> 8; packet[20] = entity->drop.stack.metadataVersion;
    memcpy(packet + 21, entity->drop.stack.metadata, ITEM_METADATA_BYTES);
    return packet;
}

