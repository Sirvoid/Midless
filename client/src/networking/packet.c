#include "version.h"
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
#include <limits.h>
#include "raylib.h"
#include "packet.h"
#include "networkhandler.h"
#include "world.h"
#include "chat.h"
#include "particle.h"
#include "block.h"
#include "entitymodel.h"
#include "textureprotocol.h"
#include "rotation.h"
#include "inventoryprotocol.h"
#include "itemdefinition.h"
#include "../textures.h"
#include "../items.h"
#include "../inventoryclient.h"
#include "../digging.h"
#include "../gui/hudbars.h"
#include "../gui/formattedtext.h"


unsigned char *packetData;
int packetDataLength;
int Packet_Lengths[256] = {
    IDENTIFICATION_PACKET_SIZE, // 0
    PLAYER_POSITION_PACKET_SIZE, // 1
    MESSAGE_PACKET_SIZE, // 2
    DRAW_DISTANCE_PACKET_SIZE, // 3
    PLAYER_CLICK_PACKET_SIZE, // 4
    TEXTURE_ACK_SIZE, // 5
    INVENTORY_ACTION_PACKET_SIZE, // 6
};
int pingCalculationPreviousTime = 0;

int Packet_GetLength(unsigned char opcode) {
    return Packet_Lengths[opcode];
}

/*-------------------------------------------------------------------------------------------------------*
*-------------------------------------------Packets Readers----------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/
int packetReaderIndex = 1;

const unsigned char *Packet_ReadBytes(int size) {
    if (size < 0 || packetReaderIndex > packetDataLength - size) return NULL;
    const unsigned char *bytes = packetData + packetReaderIndex;
    packetReaderIndex += size;
    return bytes;
}

uint32_t Packet_ReadUInt(void) {
    const unsigned char *bytes = Packet_ReadBytes(4);
    if (!bytes) return 0;
    return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 | (uint32_t)bytes[2] << 8 | bytes[3];
}

unsigned char Packet_ReadByte(void) {
    return packetData[packetReaderIndex++];
}

char Packet_ReadSByte(void) {
    return packetData[packetReaderIndex++];
}

short Packet_ReadShort(void) {
    short value = (short)(packetData[packetReaderIndex] << 8 | packetData[packetReaderIndex + 1]); 
    packetReaderIndex += 2;
    return value;
}

unsigned short Packet_ReadUShort(void) {
    unsigned short value = (unsigned short)(packetData[packetReaderIndex] << 8 | packetData[packetReaderIndex + 1]); 
    packetReaderIndex += 2;
    return value;
}

int Packet_ReadInt(void) { return (int32_t)Packet_ReadUInt(); }

char *Packet_ReadString(void) {
    char *string = MemAlloc(PACKET_STRING_SIZE + 1);
    if (!string) return NULL;
    
    for (int i = 0; i < PACKET_STRING_SIZE; i++) {
        string[i] = packetData[packetReaderIndex++];
    }
    
    string[PACKET_STRING_SIZE] = 0;

    return string;
}


unsigned char* Packet_ReadArray(int size) {
    unsigned char *arr = MemAlloc(size);
    memcpy(arr, &packetData[packetReaderIndex], size);
    packetReaderIndex += size;
    return arr;
}

/*-------------------------------------------------------------------------------------------------------*
*-------------------------------------------Packets Writer-----------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/
int packetWriterIndex = 0;

void Packet_WriteByte(unsigned char *packet, unsigned char value) {
    packet[packetWriterIndex++] = value;
}

void Packet_WriteSByte(unsigned char *packet, char value) {
    packet[packetWriterIndex++] = value;
}

void Packet_WriteShort(unsigned char *packet, short value) {
    packet[packetWriterIndex++] = (char)(value >> 8);
	packet[packetWriterIndex++] = (char)(value);
}

void Packet_WriteUShort(unsigned char *packet, unsigned short value) {
    packet[packetWriterIndex++] = (char)(value >> 8);
	packet[packetWriterIndex++] = (char)(value);
}

void Packet_WriteInt(unsigned char *packet, int value) {
    packet[packetWriterIndex++] = (char)(value >> 24);
	packet[packetWriterIndex++] = (char)(value >> 16);
    packet[packetWriterIndex++] = (char)(value >> 8);
    packet[packetWriterIndex++] = (char)(value);
}

void Packet_WriteString(unsigned char *packet, char *string) {
    int length = TextLength(string);
    for (int i = 0; i < PACKET_STRING_SIZE; i++) {
        if (i < length) {
            packet[packetWriterIndex++] = string[i];
        } else {
            packet[packetWriterIndex++] = 0;
        }  
    }
    
}

/*-------------------------------------------------------------------------------------------------------*
*------------------------------------------Packets Received----------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/

void Packet_HandleMapInit(void) {
    if (packetDataLength != MAP_INIT_PACKET_SIZE || Packet_ReadUShort() != GAME_PROTOCOL_VERSION) {
        TraceLog(LOG_WARNING, "Incompatible server protocol; expected version %d", GAME_PROTOCOL_VERSION);
        Network_Disconnect();
        return;
    }
    World_LoadMultiplayer();
}


void Packet_HandleLoadChunk(void) {
    // Opcode + three coordinates + compressed-array length.
    const int headerLength = LOAD_CHUNK_HEADER_SIZE;
    if (packetDataLength < headerLength + CHUNK_SKY_MASK_SIZE) return;
    int x = Packet_ReadInt();
    int y = Packet_ReadInt();
    int z = Packet_ReadInt();
    int length = Packet_ReadUShort();
    // Compressed data consists of (block ID, run length) ushort pairs.
    if (length == 0 || length > CHUNK_SIZE * 2 || length % 2 != 0) return;
    int expectedLength = headerLength + length * 2 + CHUNK_SKY_MASK_SIZE;
    if (packetDataLength != expectedLength) return;

    unsigned short* chunkData = (unsigned short*)Packet_ReadArray(length * 2);
    unsigned char* skyMask = Packet_ReadArray(CHUNK_SKY_MASK_SIZE);

    int blockCount = 0;
    for (int i = 0; i < length; i += 2) {
        if (chunkData[i + 1] == 0 ||
            blockCount + chunkData[i + 1] > CHUNK_SIZE) {
            MemFree(chunkData);
            MemFree(skyMask);
            return;
        }
        // Unknown block types must not prevent the rest of the chunk loading.
        if (!Block_IsDefined(chunkData[i])) chunkData[i] = 0;
        blockCount += chunkData[i + 1];
    }
    if (blockCount != CHUNK_SIZE) {
        MemFree(chunkData);
        MemFree(skyMask);
        return;
    }
    Vector3 position = (Vector3) {x,y,z};
    World_AddChunk(position);
    Chunk* chunk = World_GetChunkAt(position);
    if (!chunk) { MemFree(chunkData); MemFree(skyMask); return; }
    Chunk_Decompress(chunk, chunkData, length);
    memcpy(chunk->skyMask, skyMask, CHUNK_SKY_MASK_SIZE);
    MemFree(chunkData);
    MemFree(skyMask);
}

void Packet_HandleUnloadChunk(void) {
    int x = Packet_ReadInt();
    int y = Packet_ReadInt();
    int z = Packet_ReadInt();

    Vector3 position = (Vector3) {x,y,z};
    Chunk* chunk = World_GetChunkAt(position);
    if (chunk != NULL) {
        World_RemoveChunk(chunk);
    }
}

void Packet_HandleSetBlock(void) {
    int blockId = Packet_ReadUShort();
    Vector3 position = (Vector3) { Packet_ReadInt(), Packet_ReadInt(), Packet_ReadInt() };
    bool byPlayer = Packet_ReadByte();
    if (!Block_IsDefined(blockId)) return;
    int oldBlockId = World_GetBlock(position);
    if (byPlayer && blockId == 0 && oldBlockId != 0) Particle_SpawnBlockBreak(position, oldBlockId);
    World_SetBlock(position, blockId, false);
}

void Packet_HandleTextColor(void) {
    if (packetDataLength != TEXT_COLOR_PACKET_SIZE) return;
    Color color;
    color.r = Packet_ReadByte(); color.g = Packet_ReadByte();
    color.b = Packet_ReadByte(); color.a = Packet_ReadByte();
    unsigned char code = Packet_ReadByte();
    if (TextColor_ValidCode(code)) textColors[code] = color;
}

void Packet_HandleNametag(void) {
    if (packetDataLength != NAMETAG_PACKET_SIZE) return;
    int id = Packet_ReadUShort();
    const unsigned char *text = Packet_ReadBytes(NAMETAG_TEXT_SIZE);
    if (!text || !memchr(text, 0, NAMETAG_TEXT_SIZE)) return;
    Nametag tag = {0};
    memcpy(tag.text, text, NAMETAG_TEXT_SIZE);
    if (strchr(tag.text, '\n') || strchr(tag.text, '\r')) return;
    tag.color.r = Packet_ReadByte(); tag.color.g = Packet_ReadByte();
    tag.color.b = Packet_ReadByte(); tag.color.a = Packet_ReadByte();
    int visible = Packet_ReadByte();
    tag.visible = visible != 0;
    tag.offset = Packet_ReadInt() / 64.0f;
    if (visible > 1 || tag.offset < -16 || tag.offset > 16) return;
    if (world.entities && id < WORLD_MAX_ENTITIES && world.entities[id].type)
        world.entities[id].nametag = tag;
}

void Packet_HandleSpawnEntity(void) {
    int id = Packet_ReadUShort();
    int type = Packet_ReadByte();
    int modelId = Packet_ReadByte();
    int x = Packet_ReadInt();
    int y = Packet_ReadInt();
    int z = Packet_ReadInt();
    Vector3 position = (Vector3) { x / 64.0f, y / 64.0f, z / 64.0f };
    if (id == USHRT_MAX) {
        Player_SetEntityModel(type, modelId);
        Player_Teleport(position);
        return;
    }
    World_AddEntity(id, type, modelId, position, (Vector3) {0, 0, 0});
    if (world.entities && id < WORLD_MAX_ENTITIES) world.entities[id].heldBlock = Packet_ReadUShort();
}

void Packet_HandleDespawnEntity(void) {
    int id = Packet_ReadUShort();
    World_RemoveEntity(id);
}

void Packet_HandleTeleportEntity(void) {
    int id = Packet_ReadUShort();
    int x = Packet_ReadInt();
    int y = Packet_ReadInt();
    int z = Packet_ReadInt();
    Vector3 rotation = {0};
    rotation.x = Rotation_Decode(Packet_ReadByte());
    rotation.y = Rotation_Decode(Packet_ReadByte());
    rotation.z = Rotation_Decode(Packet_ReadByte());
    Vector3 position = (Vector3) { x / 64.0f, y / 64.0f, z / 64.0f };
    if (id == USHRT_MAX) {
        Player_Teleport(position);
        return;
    }
    World_TeleportEntity(id, position, rotation);
}

void Packet_HandleMessage(void) {
    char *message = Packet_ReadString();
    Chat_AddOwnedLine(message);
}

void Packet_HandleMessageContinuation(void) {
    char *message = Packet_ReadString();
    Chat_AppendOwnedLine(message);
}

void Packet_HandleBlockBatch(void) {
    const int headerLength = BLOCK_BATCH_HEADER_SIZE; // Opcode + update count.
    const int updateLength = BLOCK_BATCH_UPDATE_SIZE; // Block ID + three coordinates.
    if (packetDataLength < headerLength) return;
    int count = Packet_ReadUShort();
    if (packetDataLength != headerLength + count * updateLength) return;
    for (int i = 0; i < count; i++) {
        int blockId = Packet_ReadUShort();
        Vector3 position = {
            Packet_ReadInt(), Packet_ReadInt(), Packet_ReadInt()
        };
        if (!Block_IsDefined(blockId)) continue;
        World_SetBlock(position, blockId, false);
    }
}

void Packet_HandleWorldTime(void) {
    int timeMilliseconds = Packet_ReadInt();
    world.time = timeMilliseconds / 1000.0f;
}

void Packet_HandleEntityAnimation(void) {
    int id = Packet_ReadUShort();
    EntityAnimationType animation = (EntityAnimationType)Packet_ReadByte();
    World_PlayEntityAnimation(id, animation);
}

void Packet_HandleDefineBlock(void) {
    if (packetDataLength != DEFINE_BLOCK_PACKET_SIZE) return;
    BlockDefinition definition = {0};
    int id = Packet_ReadUShort();
    char *name = Packet_ReadString();
    if (!name) return;
    memcpy(definition.name, name, sizeof(definition.name));
    MemFree(name);
    for (int i = 0; i < 6; i++) definition.textures[i] = Packet_ReadByte();
    definition.modelType = Packet_ReadByte();
    definition.renderType = Packet_ReadByte();
    definition.colliderType = Packet_ReadByte();
    definition.lightType = Packet_ReadByte();
    for (int i = 0; i < 3; i++) definition.min[i] = Packet_ReadByte();
    for (int i = 0; i < 3; i++) definition.max[i] = Packet_ReadByte();
    uint8_t geometry[BLOCK_GEOMETRY_BYTES];
    for(int i=0;i<BLOCK_GEOMETRY_BYTES;i++) geometry[i]=Packet_ReadByte();
    if(!BlockGeometry_Decode(&definition.geometry,geometry)) return;
    if (!Block_ApplyDefinition(id, &definition)) {
        TraceLog(LOG_WARNING, "Rejected invalid block definition");
    }
}

void Packet_HandleRemoveBlockDefinition(void) {
    if (packetDataLength == REMOVE_BLOCK_DEFINITION_PACKET_SIZE) Block_RemoveDefinition(Packet_ReadByte());
}

void Packet_HandleDefineEntityModel(void) {
    if (packetDataLength < ENTITY_MODEL_HEADER_SIZE) return;
    ModelDefinition d = {0};
    int id = Packet_ReadByte();
    char *name = Packet_ReadString();
    if (!name) return;
    memcpy(d.name, name, sizeof(d.name)); MemFree(name);
    d.texture = Packet_ReadUShort();
    d.partCount = Packet_ReadByte();
    if (!d.partCount || d.partCount > ENTITY_MODEL_MAX_PARTS ||
        packetDataLength != ENTITY_MODEL_HEADER_SIZE + d.partCount * ENTITY_MODEL_PART_SIZE) return;
    for (int i = 0; i < d.partCount; i++) {
        ModelPartDefinition *p = &d.parts[i];
        p->role = Packet_ReadByte();
        p->firstPersonVisible = Packet_ReadByte();
        p->hasGrip = Packet_ReadByte();
        for (int a = 0; a < 3; a++) p->grip[a] = Packet_ReadShort();
        for (int a = 0; a < 3; a++) p->position[a] = Packet_ReadShort();
        for (int a = 0; a < 3; a++) p->min[a] = Packet_ReadShort();
        for (int a = 0; a < 3; a++) p->max[a] = Packet_ReadShort();
        for (int f = 0; f < 6; f++) for (int a = 0; a < 4; a++) p->uv[f][a] = Packet_ReadShort();
    }
    if (!EntityModel_ApplyDefinition(id, &d)) TraceLog(LOG_WARNING, "Rejected invalid entity model");
}
void Packet_HandleRemoveEntityModel(void) { EntityModel_RemoveDefinition(Packet_ReadByte()); }
void Packet_HandleSetEntityModel(void) {
    int entityId = Packet_ReadUShort();
    int modelId = Packet_ReadByte();
    EntityModel_SetEntityModel(entityId, modelId);
}

void Packet_HandleHeldBlock(void) {
    int id = Packet_ReadUShort();
    unsigned short blockId = Packet_ReadUShort();
    if (world.entities && id < WORLD_MAX_ENTITIES && world.entities[id].type)
        world.entities[id].heldBlock = blockId;
}

void Packet_HandleDroppedItem(void) {
    if (packetDataLength != DROPPED_ITEM_PACKET_SIZE) return;
    int id = Packet_ReadUShort();
    ItemStack stack = {Packet_ReadUShort(), Packet_ReadByte()};
    Vector3 position = {Packet_ReadInt() / 64.0f, Packet_ReadInt() / 64.0f, Packet_ReadInt() / 64.0f};
    stack.metadataSize = Packet_ReadByte();
    stack.metadataVersion = Packet_ReadUShort();
    if (stack.metadataSize > ITEM_METADATA_BYTES || (stack.metadataSize && !stack.metadataVersion)) return;
    memcpy(stack.metadata, Packet_ReadBytes(ITEM_METADATA_BYTES), ITEM_METADATA_BYTES);
    if (!world.entities || id >= WORLD_MAX_ENTITIES || !stack.itemId || stack.itemId >= ITEM_LIMIT ||
        !stack.count || stack.count > Item_GetMaxStack(stack.itemId) ||
        fabsf(position.x) > 1000000 || fabsf(position.y) > 1000000 || fabsf(position.z) > 1000000) return;
    Entity *entity = &world.entities[id];
    if (entity->type != ENTITY_TYPE_DROPPED_ITEM) {
        World_AddEntity(id, ENTITY_TYPE_DROPPED_ITEM, 0, position, (Vector3){0});
    } else {
        World_TeleportEntity(id, position, (Vector3){0});
    }
    entity->droppedStack = stack;
}

void Packet_HandleTextureBegin(void) {
    if (packetDataLength != TEXTURE_BEGIN_SIZE) return;
    int id = Packet_ReadUShort();
    uint32_t revision = Packet_ReadUInt();
    uint32_t size = Packet_ReadUInt();
    int width = Packet_ReadUShort();
    int height = Packet_ReadUShort();
    ClientTextures_Begin(id, revision, size, width, height);
}

void Packet_HandleTextureData(void) {
    if (packetDataLength != TEXTURE_DATA_SIZE) return;
    int id = Packet_ReadUShort();
    uint32_t revision = Packet_ReadUInt();
    uint32_t offset = Packet_ReadUInt();
    unsigned count = Packet_ReadUShort();
    const unsigned char *data = Packet_ReadBytes(TEXTURE_CHUNK_BYTES);
    ClientTextures_Data(id, revision, offset, count, data);
}

void Packet_HandleTerrainTexture(void) {
    if (packetDataLength != TERRAIN_TEXTURE_PACKET_SIZE) return;
    ClientTextures_SetTerrain(Packet_ReadUShort());
}

void Packet_HandleDefineItem(void) {
    if (packetDataLength != ITEM_DEFINITION_PACKET_SIZE) return;
    int id = Packet_ReadUShort();
    ItemDefinition item = {0};
    memcpy(item.identifier, Packet_ReadBytes(sizeof(item.identifier)), sizeof(item.identifier));
    memcpy(item.name, Packet_ReadBytes(sizeof(item.name)), sizeof(item.name));
    item.maxStack = Packet_ReadByte();
    item.texture = Packet_ReadByte();
    if (id < 1 || id >= ITEM_LIMIT || !memchr(item.identifier, 0, sizeof(item.identifier)) ||
        !memchr(item.name, 0, sizeof(item.name)) || !item.maxStack || item.maxStack > 64) return;
    item.defined = true;
    ClientItems_Define(id, &item);
}

static bool Packet_ReadInventory(Inventory *inventory, uint32_t *revision, uint32_t *acknowledged) {
    *inventory = (Inventory){0};
    *revision = Packet_ReadUInt();
    *acknowledged = Packet_ReadUInt();
    inventory->selectedHotbar = Packet_ReadByte();
    int open = Packet_ReadByte();
    inventory->open = open != 0;
    inventory->cursorOrigin = Packet_ReadByte();
    if (inventory->selectedHotbar >= INVENTORY_HOTBAR_SLOTS || open > 1 ||
        (inventory->cursorOrigin >= INVENTORY_SLOT_COUNT && inventory->cursorOrigin != INVENTORY_NO_SLOT)) return false;
    for (int i = 0; i <= INVENTORY_SLOT_COUNT; i++) {
        ItemStack *stack = i == INVENTORY_SLOT_COUNT ? &inventory->cursor : &inventory->slots[i];
        stack->itemId = Packet_ReadUShort();
        stack->count = Packet_ReadByte();
        stack->metadataSize = Packet_ReadByte();
        stack->metadataVersion = Packet_ReadUShort();
        memcpy(stack->metadata, Packet_ReadBytes(ITEM_METADATA_BYTES), ITEM_METADATA_BYTES);
        if (stack->metadataSize > ITEM_METADATA_BYTES ||
            (stack->metadataSize && (!stack->count || !stack->metadataVersion)) ||
            (stack->count == 0) != (stack->itemId == 0) || stack->count > Item_GetMaxStack(stack->itemId)) return false;
    }
    return inventory->open || !inventory->cursor.count;
}

void Packet_HandleInventoryState(void) {
    if (packetDataLength != INVENTORY_STATE_PACKET_SIZE) return;
    Inventory inventory;
    uint32_t revision, acknowledged;
    if (!Packet_ReadInventory(&inventory, &revision, &acknowledged)) return;
    ClientInventory_SetState(&inventory, revision, acknowledged, NULL);
}

void Packet_HandleInventoryView(void) {
    if (packetDataLength <= INVENTORY_VIEW_HEADER_SIZE || Packet_ReadByte() != PACKET_INVENTORY_STATE) return;
    Inventory inventory;
    uint32_t revision, acknowledged;
    if (!Packet_ReadInventory(&inventory, &revision, &acknowledged) || !inventory.open) return;
    // The view uses the shared variable-length UI format, also used by the server writer.
    int size = packetDataLength - packetReaderIndex;
    BinaryReader reader = {Packet_ReadBytes(size), size};
    InventoryView view;
    if (!InventoryView_Read(&reader, &view) || !Binary_End(&reader)) return;
    ClientInventory_SetState(&inventory, revision, acknowledged, &view);
}

void Packet_HandleDigProgress(void) {
    if (packetDataLength != DIG_PROGRESS_PACKET_SIZE) return;
    Packet_ReadInt(); // Target position, retained in the packet for compatibility.
    Packet_ReadInt();
    Packet_ReadInt();
    uint32_t sequence = Packet_ReadUInt();
    int milliseconds = Packet_ReadInt();
    ClientInventory_SetDigProgress(sequence, milliseconds);
}

void Packet_HandleBreakingTexture(void) {
    if (packetDataLength != BREAKING_TEXTURE_PACKET_SIZE) return;
    Digging_SetTexture(Packet_ReadByte());
}

void Packet_HandleDefineHudBar(void) {
    if (packetDataLength != HUD_BAR_DEFINE_SIZE) return;
    int id = Packet_ReadByte();
    HudBarDefinition bar = {.defined = true};
    bar.texture = Packet_ReadByte();
    bar.icons = Packet_ReadByte();
    bar.priority = Packet_ReadUShort();
    bar.max = Packet_ReadUShort();
    if (id >= HUD_BAR_LIMIT || bar.texture < 2 || bar.texture >= TEXTURE_LIMIT ||
        !bar.icons || bar.icons > HUD_BAR_MAX_ICONS || !bar.max) return;
    ClientHudBars_Define(id, bar);
}

void Packet_HandleSetHudBar(void) {
    if (packetDataLength != HUD_BAR_STATE_SIZE) return;
    int id = Packet_ReadByte();
    int value = Packet_ReadUShort();
    int visible = Packet_ReadByte();
    if (id >= HUD_BAR_LIMIT || visible > 1) return;
    ClientHudBars_Set(id, (HudBarState){value, visible != 0});
}

void Packet_HandleRemoveHudBar(void) {
    if (packetDataLength != HUD_BAR_REMOVE_SIZE) return;
    int id = Packet_ReadByte();
    if (id >= HUD_BAR_LIMIT) return;
    ClientHudBars_Remove(id);
}

/*-------------------------------------------------------------------------------------------------------*
*--------------------------------------------Packets Sent------------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/
unsigned char *Packet_CreateIdentification(unsigned short version, char *name) {
    packetWriterIndex = 0;
    unsigned char *packet = (unsigned char*)MemAlloc(Packet_Lengths[0]);
    Packet_WriteByte(packet, 0);
    Packet_WriteUShort(packet, version);
    Packet_WriteString(packet, name);
    return packet;
}

unsigned char *Packet_CreatePlayerPosition(Vector3 position, Vector3 rotation) {
    packetWriterIndex = 0;
    unsigned char *packet = (unsigned char*)MemAlloc(Packet_Lengths[1]);
    Packet_WriteByte(packet, 1);
    Packet_WriteInt(packet, (int)(position.x * 64));
    Packet_WriteInt(packet, (int)(position.y * 64));
    Packet_WriteInt(packet, (int)(position.z * 64));
    Packet_WriteByte(packet, Rotation_Encode(rotation.x));
    Packet_WriteByte(packet, Rotation_Encode(rotation.y));
    Packet_WriteByte(packet, Rotation_Encode(rotation.z));
    return packet;
}

unsigned char *Packet_CreateMessage(char *message) {
    packetWriterIndex = 0;
    unsigned char *packet = (unsigned char*)MemAlloc(Packet_Lengths[2]);
    Packet_WriteByte(packet, 2);
    Packet_WriteString(packet, message);
    return packet;
}

unsigned char *Packet_CreateSetDrawDistance(unsigned char distance) {
    packetWriterIndex = 0;
    unsigned char *packet = (unsigned char*)MemAlloc(Packet_Lengths[3]);
    Packet_WriteByte(packet, 3);
    Packet_WriteByte(packet, distance);
    return packet;
}

unsigned char *Packet_CreatePlayerClick(unsigned char button) {
    packetWriterIndex = 0;
    unsigned char *packet = (unsigned char*)MemAlloc(Packet_Lengths[4]);
    Packet_WriteByte(packet, 4);
    Packet_WriteByte(packet, button);
    return packet;
}


