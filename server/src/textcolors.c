/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "textcolors.h"
#include "textcolor.h"
#include "packet.h"
#include "networkhandler.h"
#include "world/world.h"
#include <string.h>
#include <math.h>

static Color palette[256];

static unsigned char *CreateColorPacket(unsigned char code) {
    unsigned char *packet = MemAlloc(TEXT_COLOR_PACKET_SIZE);
    Color c = palette[code];
    packet[0] = PACKET_TEXT_COLOR;
    packet[1] = c.r;
    packet[2] = c.g;
    packet[3] = c.b;
    packet[4] = c.a;
    packet[5] = code;
    return packet;
}

void ServerTextColors_Send(Player *player) {
    for (int i = 0; i < 256; i++)
        if (palette[i].a)
            ServerNetwork_Send(player, CreateColorPacket(i));
}

void ServerTextColors_Reset(void) {
    memset(palette, 0, sizeof(palette));
}

unsigned char *ServerNametag_CreatePacket(const Entity *entity) {
    unsigned char *packet = MemAlloc(NAMETAG_PACKET_SIZE);
    packet[0] = PACKET_NAMETAG;
    packet[1] = entity->id >> 8;
    packet[2] = entity->id;
    memcpy(packet + 3, entity->nametag.text, NAMETAG_TEXT_SIZE);
    int i = 3 + NAMETAG_TEXT_SIZE;
    Color c = entity->nametag.color;
    packet[i++] = c.r;
    packet[i++] = c.g;
    packet[i++] = c.b;
    packet[i++] = c.a;
    packet[i++] = entity->nametag.visible;
    int offset = (int)(entity->nametag.offset * 64);
    unsigned int bits = (unsigned int)offset;
    for (int j = 0; j < 4; j++)
        packet[i++] = bits >> (24 - j * 8);
    return packet;
}

void ServerTextColors_Define(unsigned char code, Color color) {
    if (!TextColor_ValidCode(code))
        return;
    palette[code] = color;
    ServerWorld_Broadcast(CreateColorPacket(code));
}

void ServerTextColors_Remove(unsigned char code) {
    ServerTextColors_Define(code, (Color){0});
}

void ServerNametag_Set(Entity *entity, Nametag tag) {
    if (strcmp(tag.text, entity->nametag.text) || tag.visible != entity->nametag.visible ||
        tag.offset != entity->nametag.offset ||
        memcmp(&tag.color, &entity->nametag.color, sizeof(Color))) {
        entity->nametag = tag;
        entity->nametagDirty = true;
    }
}
