#include <string.h>
#include "entitytexture.h"
#include "world/world.h"
#include "world/textures.h"
#include "packetsizes.h"

int ServerEntityTexture_Set(lua_State *state, Entity *entity) {
    const char *name = "";
    if (!lua_isnoneornil(state,2)) {
        size_t length;
        name = luaL_checklstring(state,2,&length);
        if (!length || length>64 || memchr(name,0,length) || ServerTextures_Find(name)<0)
            return luaL_error(state,"texture must be a registered name or nil");
    }
    strcpy(entity->texture,name); entity->textureDirty = true;
    if (entity->ownerPlayerId>=0 && serverWorld.players[entity->ownerPlayerId])
        strcpy(serverWorld.players[entity->ownerPlayerId]->texture,name);
    return 0;
}
int ServerEntityTexture_Get(lua_State *state, Entity *entity) {
    if (entity->texture[0]) lua_pushstring(state,entity->texture); else lua_pushnil(state);
    return 1;
}
int ServerEntityTexture_Id(const Entity *entity) {
    return entity->texture[0] ? ServerTextures_Find(entity->texture)+1 : 0;
}
unsigned char *ServerEntityTexture_Packet(const Entity *entity, int recipientEntityId) {
    unsigned char *packet = MemAlloc(SET_ENTITY_TEXTURE_PACKET_SIZE);
    int id = entity->id == recipientEntityId ? 65535 : entity->id;
    int texture = ServerEntityTexture_Id(entity);
    packet[0] = PACKET_SET_ENTITY_TEXTURE; packet[1] = id>>8; packet[2] = id;
    packet[3] = texture>>8; packet[4] = texture;
    return packet;
}
