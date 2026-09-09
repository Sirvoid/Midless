#include "version.h"
#include "minilua.h"
#include "../items.h"
#include "../world/worldgen.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

extern lua_State *L;
#define FIELD_TYPE "midless.worldgen.field"

static void CheckDefinitionsMutable(lua_State *luaState) {
    if (worldgen.frozen)
        luaL_error(luaState, "worldgen definitions are locked; register them during mod loading");
}

static const char *CheckName(lua_State *luaState, int index) {
    size_t length;
    const char *name = luaL_checklstring(luaState, index, &length);
    if (!length || length >= WG_NAME || memchr(name, 0, length))
        luaL_error(luaState, "name must be 1..64 bytes without NULs");
    return name;
}

static double ReadNumber(lua_State *luaState, int tableIndex, const char *key, double fallback,
                         double min, double max) {
    lua_getfield(luaState, tableIndex, key);
    double value = lua_isnil(luaState, -1) ? fallback : luaL_checknumber(luaState, -1);
    lua_pop(luaState, 1);
    if (!isfinite(value) || value < min || value > max)
        luaL_error(luaState, "%s must be in [%f, %f]", key, min, max);
    return value;
}

static int ReadInteger(lua_State *luaState, int tableIndex, const char *key, int fallback, int min,
                       int max) {
    if (max == 255) {
        lua_getfield(luaState, tableIndex, key);
        if (lua_type(luaState, -1) == LUA_TSTRING) {
            int id = ServerItems_Id(luaState, -1, true, false); lua_pop(luaState, 1);
            if (id < min || id > max) return luaL_error(luaState, "block ID out of range");
            return id;
        }
        lua_pop(luaState, 1);
    }
    double value = ReadNumber(luaState, tableIndex, key, fallback, min, max);
    if (value != floor(value))
        luaL_error(luaState, "%s must be an integer", key);
    return (int)value;
}

static bool ReadBoolean(lua_State *luaState, int tableIndex, const char *key, bool fallback) {
    lua_getfield(luaState, tableIndex, key);
    bool value = fallback;
    if (!lua_isnil(luaState, -1)) {
        luaL_checktype(luaState, -1, LUA_TBOOLEAN);
        value = lua_toboolean(luaState, -1);
    }
    lua_pop(luaState, 1);
    return value;
}

static int ReadChoice(lua_State *luaState, int tableIndex, const char *key,
                      const char *const *choices, int fallback) {
    lua_getfield(luaState, tableIndex, key);
    int result = fallback;
    if (!lua_isnil(luaState, -1)) {
        const char *choice = luaL_checkstring(luaState, -1);
        result = -1;
        for (int i = 0; choices[i]; i++)
            if (!strcmp(choice, choices[i]))
                result = i;
        if (result < 0)
            luaL_error(luaState, "unknown %s: %s", key, choice);
    }
    lua_pop(luaState, 1);
    return result;
}

static int PushFieldNode(lua_State *luaState, WGField node) {
    CheckDefinitionsMutable(luaState);
    int index = -1;
    for (int i = 0; i < worldgen.fieldCount; i++) {
        WGField *existingField = &worldgen.fields[i];
        if (existingField->op != node.op || existingField->firstInput != node.firstInput ||
            existingField->secondInput != node.secondInput ||
            existingField->thirdInput != node.thirdInput || existingField->value != node.value ||
            existingField->seedOffset != node.seedOffset ||
            existingField->seedScale != node.seedScale ||
            existingField->saltScale != node.saltScale)
            continue;
        if ((existingField->op == WG_NOISE2 || existingField->op == WG_NOISE3) &&
            (existingField->noise.noise_type != node.noise.noise_type ||
             existingField->noise.fractal_type != node.noise.fractal_type ||
             existingField->noise.frequency != node.noise.frequency ||
             existingField->noise.octaves != node.noise.octaves ||
             existingField->noise.lacunarity != node.noise.lacunarity ||
             existingField->noise.gain != node.noise.gain))
            continue;
        index = i;
        break;
    }
    if (index < 0) {
        if (worldgen.fieldCount == WG_MAX_FIELDS)
            return luaL_error(luaState, "worldgen field limit (512) reached");
        index = worldgen.fieldCount++;
        worldgen.fields[index] = node;
    }
    int *handle = lua_newuserdata(luaState, sizeof(int));
    *handle = index;
    luaL_setmetatable(luaState, FIELD_TYPE);
    return 1;
}

static int CheckFieldIndex(lua_State *luaState, int arg) {
    if (lua_type(luaState, arg) == LUA_TNUMBER) {
        float value = luaL_checknumber(luaState, arg);
        if (!isfinite(value) || fabsf(value) > 1000000)
            luaL_error(luaState, "field constant must be finite and within +/-1000000");
        PushFieldNode(luaState, (WGField){.op = WG_CONSTANT,
                                          .firstInput = -1,
                                          .secondInput = -1,
                                          .value = value,
                                          .isColumnConstant = true});
        int index = *(int *)lua_touserdata(luaState, -1);
        lua_pop(luaState, 1);
        return index;
    }
    int index = *(int *)luaL_checkudata(luaState, arg, FIELD_TYPE);
    if (index < 0 || index >= worldgen.fieldCount)
        luaL_error(luaState, "invalid field handle");
    return index;
}

static int ReadField(lua_State *luaState, int tableIndex, const char *key, int fallback,
                     bool column) {
    lua_getfield(luaState, tableIndex, key);
    int index =
        lua_isnil(luaState, -1) ? fallback : CheckFieldIndex(luaState, lua_gettop(luaState));
    lua_pop(luaState, 1);
    if (column && index >= 0 && !worldgen.fields[index].isColumnConstant)
        luaL_error(luaState, "%s requires a 2D field", key);
    return index;
}

static int CreateBinaryField(lua_State *luaState, WGOp op) {
    int a = CheckFieldIndex(luaState, 1), b = CheckFieldIndex(luaState, 2);
    return PushFieldNode(luaState,
                         (WGField){.op = op,
                                   .firstInput = a,
                                   .secondInput = b,
                                   .isColumnConstant = worldgen.fields[a].isColumnConstant &&
                                                       worldgen.fields[b].isColumnConstant});
}

static int Add(lua_State *luaState) {
    return CreateBinaryField(luaState, WG_ADD);
}

static int Sub(lua_State *luaState) {
    return CreateBinaryField(luaState, WG_SUB);
}

static int Mul(lua_State *luaState) {
    return CreateBinaryField(luaState, WG_MUL);
}

static int Div(lua_State *luaState) {
    return CreateBinaryField(luaState, WG_DIV);
}

static int Min(lua_State *luaState) {
    return CreateBinaryField(luaState, WG_MIN);
}

static int Max(lua_State *luaState) {
    return CreateBinaryField(luaState, WG_MAX);
}

static int Lt(lua_State *luaState) {
    return CreateBinaryField(luaState, WG_LT);
}

static int Eq(lua_State *luaState) {
    return CreateBinaryField(luaState, WG_EQ);
}

static int Mod(lua_State *luaState) {
    return CreateBinaryField(luaState, WG_MOD);
}

static int CreateUnaryField(lua_State *luaState, WGOp op) {
    int a = CheckFieldIndex(luaState, 1);
    return PushFieldNode(luaState,
                         (WGField){.op = op,
                                   .firstInput = a,
                                   .secondInput = -1,
                                   .thirdInput = -1,
                                   .isColumnConstant = worldgen.fields[a].isColumnConstant});
}

static int Floor(lua_State *luaState) {
    return CreateUnaryField(luaState, WG_FLOOR);
}

static int Ceil(lua_State *luaState) {
    return CreateUnaryField(luaState, WG_CEIL);
}

static int Trunc(lua_State *luaState) {
    return CreateUnaryField(luaState, WG_TRUNC);
}

static int Sin(lua_State *luaState) {
    return CreateUnaryField(luaState, WG_SIN);
}

static int Cos(lua_State *luaState) {
    return CreateUnaryField(luaState, WG_COS);
}

static int Select(lua_State *luaState) {
    int a = CheckFieldIndex(luaState, 1), b = CheckFieldIndex(luaState, 2),
        c = CheckFieldIndex(luaState, 3);
    return PushFieldNode(luaState,
                         (WGField){.op = WG_SELECT,
                                   .firstInput = a,
                                   .secondInput = b,
                                   .thirdInput = c,
                                   .isColumnConstant = worldgen.fields[a].isColumnConstant &&
                                                       worldgen.fields[b].isColumnConstant &&
                                                       worldgen.fields[c].isColumnConstant});
}

static int Random(lua_State *luaState) {
    luaL_checktype(luaState, 1, LUA_TTABLE);
    WGField field = {.op = WG_RANDOM,
                     .firstInput = ReadField(luaState, 1, "seed", -1, false),
                     .secondInput = ReadField(luaState, 1, "salt", -1, false),
                     .thirdInput = -1};
    field.seedScale = ReadInteger(luaState, 1, "seed_scale", 1, -1000000, 1000000);
    field.saltScale = ReadInteger(luaState, 1, "salt_scale", 1, -1000000, 1000000);
    field.isColumnConstant =
        (field.firstInput < 0 || worldgen.fields[field.firstInput].isColumnConstant) &&
        (field.secondInput < 0 || worldgen.fields[field.secondInput].isColumnConstant);
    return PushFieldNode(luaState, field);
}

static int LocalIndex(lua_State *luaState) {
    return PushFieldNode(
        luaState,
        (WGField){.op = WG_LOCAL_INDEX, .firstInput = -1, .secondInput = -1, .thirdInput = -1});
}

static int OriginX(lua_State *luaState) {
    return PushFieldNode(
        luaState,
        (WGField){.op = WG_ORIGIN_X, .firstInput = -1, .secondInput = -1, .thirdInput = -1});
}

static int OriginY(lua_State *luaState) {
    return PushFieldNode(
        luaState,
        (WGField){.op = WG_ORIGIN_Y, .firstInput = -1, .secondInput = -1, .thirdInput = -1});
}

static int OriginZ(lua_State *luaState) {
    return PushFieldNode(
        luaState,
        (WGField){.op = WG_ORIGIN_Z, .firstInput = -1, .secondInput = -1, .thirdInput = -1});
}

static int Step(lua_State *luaState) {
    return PushFieldNode(
        luaState, (WGField){.op = WG_STEP, .firstInput = -1, .secondInput = -1, .thirdInput = -1});
}

static int Steps(lua_State *luaState) {
    return PushFieldNode(
        luaState, (WGField){.op = WG_STEPS, .firstInput = -1, .secondInput = -1, .thirdInput = -1});
}

static int Abs(lua_State *luaState) {
    int a = CheckFieldIndex(luaState, 1);
    return PushFieldNode(luaState,
                         (WGField){.op = WG_ABS,
                                   .firstInput = a,
                                   .secondInput = -1,
                                   .isColumnConstant = worldgen.fields[a].isColumnConstant});
}

static int Neg(lua_State *luaState) {
    lua_settop(luaState, 1);
    lua_pushnumber(luaState, -1);
    return Mul(luaState);
}

static int Constant(lua_State *luaState) {
    int i = CheckFieldIndex(luaState, 1);
    int *h = lua_newuserdata(luaState, sizeof(int));
    *h = i;
    luaL_setmetatable(luaState, FIELD_TYPE);
    return 1;
}

static int X(lua_State *luaState) {
    return PushFieldNode(
        luaState,
        (WGField){.op = WG_X, .firstInput = -1, .secondInput = -1, .isColumnConstant = true});
}

static int Y(lua_State *luaState) {
    return PushFieldNode(luaState, (WGField){.op = WG_Y, .firstInput = -1, .secondInput = -1});
}

static int Z(lua_State *luaState) {
    return PushFieldNode(
        luaState,
        (WGField){.op = WG_Z, .firstInput = -1, .secondInput = -1, .isColumnConstant = true});
}

static int Noise(lua_State *luaState, bool isThreeDimensional) {
    CheckDefinitionsMutable(luaState);
    luaL_checktype(luaState, 1, LUA_TTABLE);
    const char *const types[] = {"opensimplex2", "opensimplex2s", "cellular", "perlin",
                                 "value_cubic",  "value",         NULL};
    const char *const fractals[] = {"none", "fbm", "ridged", "pingpong", NULL};
    WGField field = {.op = isThreeDimensional ? WG_NOISE3 : WG_NOISE2,
                     .firstInput = -1,
                     .secondInput = -1,
                     .thirdInput = -1,
                     .isColumnConstant = !isThreeDimensional};
    field.noise = fnlCreateState();
    field.noise.noise_type = ReadChoice(luaState, 1, "type", types, 1);
    field.noise.fractal_type = ReadChoice(luaState, 1, "fractal", fractals, 1);
    field.noise.frequency = ReadNumber(luaState, 1, "frequency", 0.01, 0.000001, 10);
    field.noise.octaves = ReadInteger(luaState, 1, "octaves", 3, 1, 12);
    field.noise.lacunarity = ReadNumber(luaState, 1, "lacunarity", 2, 0.01, 8);
    field.noise.gain = ReadNumber(luaState, 1, "gain", 0.5, 0, 1);
    field.seedOffset = ReadInteger(luaState, 1, "seed_offset", 0, -1000000, 1000000);
    field.firstInput = ReadField(luaState, 1, "x", -1, false);
    field.secondInput = ReadField(luaState, 1, isThreeDimensional ? "y" : "z", -1, false);
    if (isThreeDimensional)
        field.thirdInput = ReadField(luaState, 1, "z", -1, false);
    field.isColumnConstant =
        (field.firstInput < 0 || worldgen.fields[field.firstInput].isColumnConstant) &&
        (field.secondInput < 0 ? !isThreeDimensional
                               : worldgen.fields[field.secondInput].isColumnConstant) &&
        (field.thirdInput < 0 || worldgen.fields[field.thirdInput].isColumnConstant);
    return PushFieldNode(luaState, field);
}

static int Noise2(lua_State *luaState) {
    return Noise(luaState, false);
}

static int Noise3(lua_State *luaState) {
    return Noise(luaState, true);
}

static int ConfigureWorldgen(lua_State *luaState) {
    CheckDefinitionsMutable(luaState);
    luaL_checktype(luaState, 1, LUA_TTABLE);
    const char *const presets[] = {"flat", "native", NULL};
    int preset = ReadChoice(luaState, 1, "preset", presets, 1);
    int minY = ReadInteger(luaState, 1, "min_y", -128, -4096, 4096);
    int maxY = ReadInteger(luaState, 1, "max_y", 256, -4096, 4096);
    if (maxY < minY || maxY - minY > 1024)
        return luaL_error(luaState, "vertical generation span must be 0..1024 blocks");
    int sea = ReadInteger(luaState, 1, "sea_level", 48, -4096, 4096);
    int density = ReadField(luaState, 1, "density", -1, false);
    int caves = ReadField(luaState, 1, "caves", -1, false);
    int temp = ReadField(luaState, 1, "temperature", -1, true);
    int moisture = ReadField(luaState, 1, "moisture", -1, true);
    int material = ReadField(luaState, 1, "material", -1, false);
    int sky = ReadField(luaState, 1, "skylight", -1, false);
    int ceiling = ReadField(luaState, 1, "ceiling", -1, true);
    bool bounded = ReadBoolean(luaState, 1, "bounded", true);
    int version = ReadInteger(luaState, 1, "version", WORLDGEN_DEFAULT_VERSION, 1, 1000000);
    lua_getfield(luaState, 1, "id");
    const char *id = lua_isnil(luaState, -1) ? "custom:world" : CheckName(luaState, -1);
    strcpy(worldgen.id, id);
    lua_pop(luaState, 1);
    worldgen.version = version;
    worldgen.material = material;
    worldgen.skyField = sky;
    worldgen.ceiling = ceiling;
    worldgen.bounded = bounded;
    worldgen.minY = minY;
    worldgen.maxY = maxY;
    worldgen.seaLevel = sea;
    worldgen.density = preset == 0 ? -1 : density;
    worldgen.caves = preset == 0 ? -1 : caves;
    worldgen.temperature = temp;
    worldgen.moisture = moisture;
    return 0;
}

static int DefineMaterialRule(lua_State *luaState) {
    CheckDefinitionsMutable(luaState);
    luaL_checktype(luaState, 1, LUA_TTABLE);
    if (worldgen.ruleCount == WG_MAX_RULES)
        return luaL_error(luaState, "material rule limit (32) reached");
    WGRule rule = {.match = ReadInteger(luaState, 1, "match", -1, -1, 255),
                   .block = ReadInteger(luaState, 1, "block", 0, 0, 255),
                   .dy = ReadInteger(luaState, 1, "offset_y", 0, -32, 32),
                   .when = ReadField(luaState, 1, "when", -1, false),
                   .descending = ReadBoolean(luaState, 1, "descending", false)};
    worldgen.rules[worldgen.ruleCount++] = rule;
    return 0;
}

static void ReadOriginPadding(lua_State *luaState, int table, const char *key, int out[3],
                              int fallback) {
    lua_getfield(luaState, table, key);
    if (lua_isnil(luaState, -1)) {
        for (int i = 0; i < 3; i++)
            out[i] = fallback;
    } else {
        luaL_checktype(luaState, -1, LUA_TTABLE);
        for (int i = 0; i < 3; i++) {
            lua_rawgeti(luaState, -1, i + 1);
            lua_Integer v = luaL_checkinteger(luaState, -1);
            if (v < -4 || v > 4)
                luaL_error(luaState, "origin padding must be -4..4 chunks");
            out[i] = (int)v;
            lua_pop(luaState, 1);
        }
    }
    lua_pop(luaState, 1);
}

static int DefineFeature(lua_State *luaState) {
    CheckDefinitionsMutable(luaState);
    const char *name = CheckName(luaState, 1);
    luaL_checktype(luaState, 2, LUA_TTABLE);
    if (worldgen.featureCount == WG_MAX_FEATURES)
        return luaL_error(luaState, "feature limit (16) reached");
    for (int i = 0; i < worldgen.featureCount; i++)
        if (!strcmp(name, worldgen.features[i].name))
            return luaL_error(luaState, "duplicate feature: %s", name);
    WGFeature feature = {0};
    strcpy(feature.name, name);
    feature.when = ReadField(luaState, 2, "when", -1, false);
    if (feature.when < 0)
        return luaL_error(luaState, "feature requires a candidate predicate");
    ReadOriginPadding(luaState, 2, "origin_min", feature.paddingMin, -1);
    ReadOriginPadding(luaState, 2, "origin_max", feature.paddingMax, 1);
    for (int i = 0; i < 3; i++)
        if (feature.paddingMin[i] > feature.paddingMax[i])
            return luaL_error(luaState, "invalid origin padding range");
    lua_getfield(luaState, 2, "commands");
    luaL_checktype(luaState, -1, LUA_TTABLE);
    feature.count = lua_rawlen(luaState, -1);
    if (feature.count < 1 || feature.count > WG_MAX_COMMANDS)
        return luaL_error(luaState, "feature needs 1..32 commands");
    bool initialized[8] = {true};
    for (int i = 0; i < feature.count; i++) {
        lua_rawgeti(luaState, -1, i + 1);
        luaL_checktype(luaState, -1, LUA_TTABLE);
        int tableIndex = lua_gettop(luaState);
        WGCommand *command = &feature.commands[i];
        const char *const ops[] = {"stroke", "sphere", NULL};
        command->op = ReadChoice(luaState, tableIndex, "op", ops, 0);
        command->from = ReadInteger(luaState, tableIndex, "from", 0, 0, 7);
        command->to = ReadInteger(luaState, tableIndex, "to", 0, 0, 7);
        if (!initialized[command->from])
            return luaL_error(luaState, "command reads an unwritten position slot");
        command->when = ReadField(luaState, tableIndex, "when", -1, false);
        command->block = ReadInteger(luaState, tableIndex, "block", 1, 0, 255);
        command->steps = ReadField(luaState, tableIndex, "steps", -1, false);
        command->dx = ReadField(luaState, tableIndex, "dx", -1, false);
        command->dy = ReadField(luaState, tableIndex, "dy", -1, false);
        command->dz = ReadField(luaState, tableIndex, "dz", -1, false);
        command->radius = ReadField(luaState, tableIndex, "radius", -1, false);
        command->bounds = ReadField(luaState, tableIndex, "bounds", -1, false);
        if (command->radius < 0 || command->bounds < 0 ||
            (command->op == WG_COMMAND_STROKE && command->steps < 0))
            return luaL_error(luaState, "command needs radius, bounds, and stroke steps");
        initialized[command->to] = true;
        lua_pop(luaState, 1);
    }
    lua_pop(luaState, 1);
    worldgen.features[worldgen.featureCount++] = feature;
    return 0;
}

static int ReadBiomeIndex(lua_State *luaState, int table) {
    lua_getfield(luaState, table, "biome");
    int result = -1;
    if (!lua_isnil(luaState, -1)) {
        const char *name = CheckName(luaState, -1);
        for (int i = 0; i < worldgen.biomeCount; i++)
            if (!strcmp(name, worldgen.biomes[i].name))
                result = i;
        if (result < 0)
            luaL_error(luaState, "biome must be registered before this definition: %s", name);
    }
    lua_pop(luaState, 1);
    return result;
}

static int DefineBiome(lua_State *luaState) {
    CheckDefinitionsMutable(luaState);
    const char *name = CheckName(luaState, 1);
    luaL_checktype(luaState, 2, LUA_TTABLE);
    bool first = worldgen.biomeCount == 1 && !strcmp(worldgen.biomes[0].name, "builtin:plains");
    if (!first && worldgen.biomeCount == WG_MAX_BIOMES)
        return luaL_error(luaState, "biome limit (32) reached");
    for (int i = 0; i < worldgen.biomeCount; i++)
        if (!strcmp(name, worldgen.biomes[i].name))
            return luaL_error(luaState, "duplicate biome: %s", name);
    WGBiome biome = {0};
    strcpy(biome.name, name);
    biome.temperature = ReadNumber(luaState, 2, "temperature", 0, -10, 10);
    biome.moisture = ReadNumber(luaState, 2, "moisture", 0, -10, 10);
    biome.spread = ReadNumber(luaState, 2, "spread", 1, 0.01, 10);
    biome.height = ReadNumber(luaState, 2, "height", 64, -4096, 4096);
    biome.variation = ReadNumber(luaState, 2, "height_variation", 0, 0, 4096);
    biome.heightField = ReadField(luaState, 2, "height_noise", -1, true);
    biome.top = ReadInteger(luaState, 2, "top", 3, 1, 255);
    biome.filler = ReadInteger(luaState, 2, "filler", 2, 1, 255);
    biome.stone = ReadInteger(luaState, 2, "stone", 1, 1, 255);
    biome.underwater = ReadInteger(luaState, 2, "underwater", 6, 1, 255);
    biome.depth = ReadInteger(luaState, 2, "filler_depth", 3, 0, 32);
    worldgen.biomes[first ? 0 : worldgen.biomeCount++] = biome;
    return 0;
}

static int DefineOre(lua_State *luaState) {
    CheckDefinitionsMutable(luaState);
    const char *name = CheckName(luaState, 1);
    luaL_checktype(luaState, 2, LUA_TTABLE);
    if (worldgen.oreCount == WG_MAX_ORES)
        return luaL_error(luaState, "ore limit (64) reached");
    for (int i = 0; i < worldgen.oreCount; i++)
        if (!strcmp(name, worldgen.ores[i].name))
            return luaL_error(luaState, "duplicate ore: %s", name);
    WGOre ore = {0};
    strcpy(ore.name, name);
    ore.salt = Worldgen_Hash(name);
    ore.block = ReadInteger(luaState, 2, "block", 19, 1, 255);
    ore.minY = ReadInteger(luaState, 2, "min_y", -64, -4096, 4096);
    ore.maxY = ReadInteger(luaState, 2, "max_y", 48, ore.minY, 4096);
    ore.size = ReadInteger(luaState, 2, "size", 3, 1, 16);
    ore.spacing = ReadInteger(luaState, 2, "spacing", 16, 8, 256);
    ore.chance = ReadNumber(luaState, 2, "chance", 0.5, 0, 1);
    ore.threshold = ReadNumber(luaState, 2, "threshold", 0.5, -1000000, 1000000);
    ore.biome = ReadBiomeIndex(luaState, 2);
    ore.field = ReadField(luaState, 2, "noise", -1, false);
    const char *const distributions[] = {"clusters", "veins", "layers", "noise", NULL};
    ore.distribution = ReadChoice(luaState, 2, "distribution", distributions, 0);
    if (ore.distribution == WG_ORE_NOISE && ore.field < 0)
        return luaL_error(luaState, "noise distribution requires a noise field");
    lua_getfield(luaState, 2, "replaces");
    if (lua_isnil(luaState, -1))
        ore.replaces[1] = true;
    else {
        luaL_checktype(luaState, -1, LUA_TTABLE);
        int count = lua_rawlen(luaState, -1);
        if (!count || count > 255)
            return luaL_error(luaState, "replaces needs 1..255 block IDs");
        for (int i = 1; i <= count; i++) {
            lua_rawgeti(luaState, -1, i);
            lua_Integer id = luaL_checkinteger(luaState, -1);
            if (id < 1 || id > 255)
                return luaL_error(luaState, "ore host block must be 1..255");
            ore.replaces[id] = true;
            lua_pop(luaState, 1);
        }
    }
    lua_pop(luaState, 1);
    worldgen.ores[worldgen.oreCount++] = ore;
    return 0;
}

static void AppendStructureBlock(lua_State *luaState, WGStructure *structure, int x, int y, int z,
                                 int id) {
    if (structure->count == WG_MAX_BLOCKS)
        luaL_error(luaState, "structure exceeds 4096 blocks");
    structure->blocks[structure->count++] = (WGBlock){x, y, z, id};
    if (abs(x) > structure->radius)
        structure->radius = abs(x);
    if (abs(z) > structure->radius)
        structure->radius = abs(z);
    if (y < structure->minDY)
        structure->minDY = y;
    if (y > structure->maxDY)
        structure->maxDY = y;
}

static int DefineStructure(lua_State *luaState) {
    CheckDefinitionsMutable(luaState);
    const char *name = CheckName(luaState, 1);
    luaL_checktype(luaState, 2, LUA_TTABLE);
    if (worldgen.structureCount == WG_MAX_STRUCTURES)
        return luaL_error(luaState, "structure limit (64) reached");
    for (int i = 0; i < worldgen.structureCount; i++)
        if (!strcmp(name, worldgen.structures[i].name))
            return luaL_error(luaState, "duplicate structure: %s", name);
    WGStructure structure = {0};
    strcpy(structure.name, name);
    structure.salt = Worldgen_Hash(name);
    structure.minDY = 64;
    structure.maxDY = -64;
    structure.spacing = ReadInteger(luaState, 2, "spacing", 64, 16, 4096);
    structure.chance = ReadNumber(luaState, 2, "chance", 0.25, 0, 1);
    structure.minY = ReadInteger(luaState, 2, "min_y", 49, -4096, 4096);
    structure.maxY = ReadInteger(luaState, 2, "max_y", 256, structure.minY, 4096);
    structure.maxSlope = ReadInteger(luaState, 2, "max_slope", 3, 0, 128);
    structure.biome = ReadBiomeIndex(luaState, 2);
    structure.rotate = ReadBoolean(luaState, 2, "rotate", true);
    structure.airOnly = ReadBoolean(luaState, 2, "air_only", false);
    structure.foundation = ReadInteger(luaState, 2, "foundation", 0, 0, 255);
    structure.foundationDepth = ReadInteger(luaState, 2, "foundation_depth", 8, 0, 32);
    lua_getfield(luaState, 2, "blocks");
    if (!lua_isnil(luaState, -1)) {
        luaL_checktype(luaState, -1, LUA_TTABLE);
        int blockCount = lua_rawlen(luaState, -1);
        if (blockCount > WG_MAX_BLOCKS)
            return luaL_error(luaState, "structure exceeds 4096 blocks");
        for (int i = 1; i <= blockCount; i++) {
            lua_rawgeti(luaState, -1, i);
            luaL_checktype(luaState, -1, LUA_TTABLE);
            int tableIndex = lua_gettop(luaState);
            int x = ReadInteger(luaState, tableIndex, "x", 0, -64, 64),
                y = ReadInteger(luaState, tableIndex, "y", 0, -64, 64),
                z = ReadInteger(luaState, tableIndex, "z", 0, -64, 64);
            AppendStructureBlock(luaState, &structure, x, y, z,
                                 ReadInteger(luaState, tableIndex, "block", 1, 0, 255));
            lua_pop(luaState, 1);
        }
    }
    lua_pop(luaState, 1);
    lua_getfield(luaState, 2, "tree");
    if (!lua_isnil(luaState, -1)) {
        luaL_checktype(luaState, -1, LUA_TTABLE);
        int tableIndex = lua_gettop(luaState);
        int height = ReadInteger(luaState, tableIndex, "height", 7, 2, 32),
            radius = ReadInteger(luaState, tableIndex, "radius", 3, 1, 7);
        int trunk = ReadInteger(luaState, tableIndex, "trunk", 10, 1, 255),
            leaves = ReadInteger(luaState, tableIndex, "leaves", 11, 1, 255);
        for (int z = -radius; z <= radius; z++)
            for (int x = -radius; x <= radius; x++)
                for (int y = -radius; y <= radius; y++)
                    if (x * x + y * y + z * z <= radius * radius)
                        AppendStructureBlock(luaState, &structure, x, height + y, z, leaves);
        for (int y = 0; y <= height; y++)
            AppendStructureBlock(luaState, &structure, 0, y, 0, trunk);
    }
    lua_pop(luaState, 1);
    if (!structure.count)
        return luaL_error(luaState, "structure requires blocks or a tree definition");
    worldgen.structures[worldgen.structureCount++] = structure;
    return 0;
}

void LuaWorldgen_Init(void) {
    static const luaL_Reg meta[] = {{"__add", Add}, {"__sub", Sub}, {"__mul", Mul}, {"__div", Div},
                                    {"__mod", Mod}, {"__unm", Neg}, {NULL, NULL}};
    static const luaL_Reg fields[] = {{"constant", Constant},
                                      {"x", X},
                                      {"y", Y},
                                      {"z", Z},
                                      {"min", Min},
                                      {"max", Max},
                                      {"abs", Abs},
                                      {"noise2d", Noise2},
                                      {"noise3d", Noise3},
                                      {"lt", Lt},
                                      {"eq", Eq},
                                      {"select", Select},
                                      {"floor", Floor},
                                      {"ceil", Ceil},
                                      {"trunc", Trunc},
                                      {"sin", Sin},
                                      {"cos", Cos},
                                      {"random", Random},
                                      {"local_index", LocalIndex},
                                      {"origin_x", OriginX},
                                      {"origin_y", OriginY},
                                      {"origin_z", OriginZ},
                                      {"step", Step},
                                      {"steps", Steps},
                                      {NULL, NULL}};
    static const luaL_Reg api[] = {{"configure", ConfigureWorldgen},
                                   {"define_biome", DefineBiome},
                                   {"define_ore", DefineOre},
                                   {"define_structure", DefineStructure},
                                   {"define_rule", DefineMaterialRule},
                                   {"define_feature", DefineFeature},
                                   {NULL, NULL}};
    luaL_newmetatable(L, FIELD_TYPE);
    luaL_setfuncs(L, meta, 0);
    lua_pushliteral(L, "worldgen field");
    lua_setfield(L, -2, "__metatable");
    lua_pop(L, 1);
    lua_getglobal(L, "midless");
    lua_newtable(L);
    luaL_setfuncs(L, api, 0);
    lua_newtable(L);
    luaL_setfuncs(L, fields, 0);
    lua_setfield(L, -2, "field");
    lua_setfield(L, -2, "worldgen");
    lua_pop(L, 1);
}
