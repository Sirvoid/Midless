#include "minilua.h"
#include "luamodels.h"
#include "../world/world.h"
#include <string.h>
extern lua_State *L;

static const char *CheckName(int argument) {
    size_t length;
    const char *name=luaL_checklstring(L,argument,&length);
    if(!length || length>64 || memchr(name,0,length))
        luaL_argerror(L,argument,"model name must contain 1 to 64 bytes without NULs");
    return name;
}

int LuaModels_Resolve(int argument, bool defining) {
    if(lua_type(L,argument)==LUA_TNUMBER) {
        lua_Integer id=luaL_checkinteger(L,argument);
        if(id<(defining?1:0) || id>255) luaL_argerror(L,argument,"model ID out of range");
        return (int)id;
    }
    const char *name=CheckName(argument);
    if(!strcmp(name,"humanoid")) {
        if(defining) luaL_argerror(L,argument,"humanoid is a reserved model name");
        return 0;
    }
    for(int id=1;id<256;id++) if(!strcmp(serverWorld.modelNames[id],name)) return id;
    if(defining) {
        for(int id=1;id<256;id++)
            if(!serverWorld.modelNames[id][0] && !serverWorld.modelDefinitions[id]) return id;
        return luaL_error(L,"entity model registry is full");
    }
    return luaL_error(L,"model name is not registered");
}

void LuaModels_BindName(int argument,int id) {
    if(lua_type(L,argument)==LUA_TSTRING)
        strcpy(serverWorld.modelNames[id],CheckName(argument));
}
