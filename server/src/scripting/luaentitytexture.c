#include "luaentitytexture.h"
#include "../entitytexture.h"
#include "../world/textures.h"
#include <string.h>

int LuaEntityTexture_Set(lua_State *state, Entity *entity) {
    const char *name = "";
    if (!lua_isnoneornil(state, 2)) {
        size_t length;
        name = luaL_checklstring(state, 2, &length);
        if (!length || length > 64 || memchr(name, 0, length) || ServerTextures_Find(name) < 0)
            return luaL_error(state, "texture must be a registered name or nil");
    }
    ServerEntityTexture_Set(entity, name);
    return 0;
}

int LuaEntityTexture_Get(lua_State *state, Entity *entity) {
    if (entity->texture[0])
        lua_pushstring(state, entity->texture);
    else
        lua_pushnil(state);
    return 1;
}
