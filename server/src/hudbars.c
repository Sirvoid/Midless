/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <string.h>
#include "hudbars.h"
#include "networkhandler.h"
#include "world/world.h"
#include "world/textures.h"

static HudBarDefinition bars[HUD_BAR_LIMIT];
static char names[HUD_BAR_LIMIT][65];

int ServerHudBars_Find(const char *name) {
    if (!name)
        return -1;
    for (int id = 0; id < HUD_BAR_LIMIT; id++)
        if (bars[id].defined && !strcmp(names[id], name))
            return id;
    return -1;
}

static void SendState(Player *player, int id) {
    unsigned char *packet = MemAlloc(HUD_BAR_STATE_SIZE);
    if (!packet)
        return;
    HudBar_WriteState(packet, id, player->hudBars[id]);
    ServerNetwork_Send(player, packet);
}

static void SendDefinition(Player *player, int id) {
    unsigned char *packet = MemAlloc(HUD_BAR_DEFINE_SIZE);
    if (!packet)
        return;
    HudBar_WriteDefinition(packet, id, &bars[id]);
    ServerNetwork_Send(player, packet);
    SendState(player, id);
}

void ServerHudBars_Send(Player *player) {
    for (int id = 0; id < HUD_BAR_LIMIT; id++)
        if (bars[id].defined)
            SendDefinition(player, id);
}

bool ServerHudBars_UsesTexture(int texture) {
    for (int id = 0; id < HUD_BAR_LIMIT; id++)
        if (bars[id].defined && bars[id].texture == texture)
            return true;
    return false;
}

void ServerHudBars_Reset(void) {
    memset(bars, 0, sizeof(bars));
    memset(names, 0, sizeof(names));
}

const char *ServerHudBars_Define(const char *name, const HudBarDefinition *definition) {
    if (!name || !name[0] || strlen(name) > 64)
        return "invalid HUD bar name";
    if (!definition || !definition->max || !definition->icons ||
        definition->icons > HUD_BAR_MAX_ICONS)
        return "invalid HUD bar definition";
    if (!ServerTextures_HudSize(definition->texture))
        return "HUD bar texture must be a defined 9x9 PNG";
    int id = ServerHudBars_Find(name);
    if (id < 0) {
        for (int i = 0; i < HUD_BAR_LIMIT; i++) {
            if (!bars[i].defined) {
                id = i;
                break;
            }
        }
    }
    if (id < 0)
        return "HUD bar registry is full";
    bars[id] = *definition;
    bars[id].defined = true;
    strcpy(names[id], name);
    if (!serverWorld.players)
        return NULL;
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *player = serverWorld.players[i];
        if (!player || player->disconnected)
            continue;
        if (player->hudBars[id].value > bars[id].max)
            player->hudBars[id].value = bars[id].max;
        SendDefinition(player, id);
    }
    return NULL;
}

bool ServerHudBars_Set(Player *player, int id, HudBarState state) {
    if (!player || id < 0 || id >= HUD_BAR_LIMIT || !bars[id].defined)
        return false;
    if (state.value > bars[id].max)
        state.value = bars[id].max;
    if (state.value == player->hudBars[id].value && state.visible == player->hudBars[id].visible)
        return true;
    player->hudBars[id] = state;
    SendState(player, id);
    return true;
}

void ServerHudBars_Remove(const char *name) {
    int id = ServerHudBars_Find(name);
    if (id < 0)
        return;
    bars[id] = (HudBarDefinition){0};
    names[id][0] = 0;
    if (!serverWorld.players)
        return;
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *player = serverWorld.players[i];
        if (!player)
            continue;
        player->hudBars[id] = (HudBarState){0};
        if (player->disconnected)
            continue;
        unsigned char *packet = MemAlloc(HUD_BAR_REMOVE_SIZE);
        if (!packet)
            continue;
        packet[0] = PACKET_REMOVE_HUD_BAR;
        packet[1] = id;
        ServerNetwork_Send(player, packet);
    }
}
