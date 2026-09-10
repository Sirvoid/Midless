#include <string.h>
#include "minilua.h"
#include "hudbars.h"
#include "networkhandler.h"
#include "world/world.h"
#include "world/textures.h"

extern lua_State *L;
static HudBarDefinition bars[HUD_BAR_LIMIT];
static char names[HUD_BAR_LIMIT][65];

static const char *ReadName(int index) {
    size_t length;
    const char *name = luaL_checklstring(L, index, &length);
    const char *separator = strchr(name, ':');
    if (!length || length > 64 || memchr(name, 0, length) ||
        !separator || separator == name || !separator[1]) luaL_error(L, "expected a namespaced HUD bar name (mod:name)");
    return name;
}

static int Find(const char *name) {
    for (int id = 0; id < HUD_BAR_LIMIT; id++)
        if (bars[id].defined && !strcmp(names[id], name)) return id;
    return -1;
}

static int ReadNumber(int table, const char *field, int fallback, int min, int max) {
    lua_getfield(L, table, field);
    lua_Integer value = lua_isnil(L, -1) ? fallback : luaL_checkinteger(L, -1);
    if (value < min || value > max) luaL_error(L, "%s must be between %d and %d", field, min, max);
    lua_pop(L, 1);
    return (int)value;
}

static void SendState(Player *player, int id) {
    unsigned char *packet = MemAlloc(HUD_BAR_STATE_SIZE);
    if (!packet) return;
    HudBar_WriteState(packet, id, player->hudBars[id]);
    ServerNetwork_Send(player, packet);
}

static void SendDefinition(Player *player, int id) {
    unsigned char *packet = MemAlloc(HUD_BAR_DEFINE_SIZE);
    if (!packet) return;
    HudBar_WriteDefinition(packet, id, &bars[id]);
    ServerNetwork_Send(player, packet);
    SendState(player, id);
}

int ServerHudBars_Define(void) {
    const char *name = ReadName(1);
    luaL_checktype(L, 2, LUA_TTABLE);
    HudBarDefinition bar = {.defined = true};
    lua_getfield(L, 2, "texture");
    int texture = ServerTextures_Find(luaL_checkstring(L, -1));
    if (!ServerTextures_HudSize(texture)) return luaL_error(L, "HUD bar texture must be a defined 9x9 PNG");
    lua_pop(L, 1);
    bar.texture = texture;
    bar.icons = ReadNumber(2, "icons", 10, 1, HUD_BAR_MAX_ICONS);
    bar.priority = ReadNumber(2, "priority", 0, 0, 65535);
    bar.max = ReadNumber(2, "max", 0, 1, 65535);
    int id = Find(name);
    if (id < 0) for (int i = 0; i < HUD_BAR_LIMIT; i++) if (!bars[i].defined) { id = i; break; }
    if (id < 0) return luaL_error(L, "HUD bar registry is full");
    bars[id] = bar;
    strcpy(names[id], name);
    if (serverWorld.players) for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *player = serverWorld.players[i];
        if (!player || player->disconnected) continue;
        if (player->hudBars[id].value > bar.max) player->hudBars[id].value = bar.max;
        SendDefinition(player, id);
    }
    return 0;
}

int ServerHudBars_Set(Player *player) {
    int id = Find(ReadName(2));
    if (id < 0) return luaL_error(L, "HUD bar is not defined");
    luaL_checktype(L, 3, LUA_TTABLE);
    HudBarState state = player->hudBars[id];
    state.value = ReadNumber(3, "value", state.value, 0, 65535);
    if (state.value > bars[id].max) state.value = bars[id].max;
    lua_getfield(L, 3, "visible");
    if (!lua_isnil(L, -1)) {
        luaL_checktype(L, -1, LUA_TBOOLEAN);
        state.visible = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);
    if (state.value == player->hudBars[id].value && state.visible == player->hudBars[id].visible) return 0;
    player->hudBars[id] = state;
    SendState(player, id);
    return 0;
}

int ServerHudBars_Remove(void) {
    int id = Find(ReadName(1));
    if (id < 0) return 0;
    bars[id] = (HudBarDefinition){0};
    names[id][0] = 0;
    if (serverWorld.players) for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *player = serverWorld.players[i];
        if (!player) continue;
        player->hudBars[id] = (HudBarState){0};
        if (player->disconnected) continue;
        unsigned char *packet = MemAlloc(HUD_BAR_REMOVE_SIZE);
        if (!packet) continue;
        packet[0] = PACKET_REMOVE_HUD_BAR;
        packet[1] = id;
        ServerNetwork_Send(player, packet);
    }
    return 0;
}

void ServerHudBars_Send(Player *player) {
    for (int id = 0; id < HUD_BAR_LIMIT; id++) if (bars[id].defined) SendDefinition(player, id);
}

bool ServerHudBars_UsesTexture(int texture) {
    for (int id = 0; id < HUD_BAR_LIMIT; id++) if (bars[id].defined && bars[id].texture == texture) return true;
    return false;
}

void ServerHudBars_Reset(void) {
    memset(bars, 0, sizeof(bars));
    memset(names, 0, sizeof(names));
}
