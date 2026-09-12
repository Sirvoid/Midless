#ifndef MIDLESS_WORLDGEN_H
#define MIDLESS_WORLDGEN_H

#include <stdint.h>
#include "FastNoiseLite.h"
#include "chunk/chunk.h"

#define WG_MAX_FIELDS 512
#define WG_MAX_RULES 32
#define WG_MAX_FEATURES 16
#define WG_MAX_COMMANDS 32
#define WG_MAX_BIOMES 32
#define WG_MAX_ORES 64
#define WG_MAX_STRUCTURES 64
#define WG_MAX_BLOCKS 4096
#define WG_NAME 65

typedef enum WGCommandType { WG_COMMAND_STROKE = 0, WG_COMMAND_SPHERE = 1 } WGCommandType;

typedef enum WGOreDistribution {
    WG_ORE_CLUSTERS = 0,
    WG_ORE_VEINS = 1,
    WG_ORE_LAYERS = 2,
    WG_ORE_NOISE = 3
} WGOreDistribution;

typedef enum WGOp {
    WG_CONSTANT,
    WG_X,
    WG_Y,
    WG_Z,
    WG_ADD,
    WG_SUB,
    WG_MUL,
    WG_DIV,
    WG_MIN,
    WG_MAX,
    WG_ABS,
    WG_NOISE2,
    WG_NOISE3,
    WG_LT,
    WG_EQ,
    WG_SELECT,
    WG_FLOOR,
    WG_CEIL,
    WG_TRUNC,
    WG_MOD,
    WG_SIN,
    WG_COS,
    WG_RANDOM,
    WG_LOCAL_INDEX,
    WG_ORIGIN_X,
    WG_ORIGIN_Y,
    WG_ORIGIN_Z,
    WG_STEP,
    WG_STEPS
} WGOp;
// A graph node. Input indices refer to previously registered nodes; -1 means absent.
typedef struct WGField {
    WGOp op;
    int firstInput;
    int secondInput;
    int thirdInput;
    float value;
    fnl_state noise;
    int seedOffset;
    int seedScale;
    int saltScale;
    bool isColumnConstant;
} WGField;
typedef struct WGBiome {
    char name[WG_NAME];
    float temperature;
    float moisture;
    float spread;
    float height;
    float variation;
    int heightField;
    int top;
    int filler;
    int stone;
    int underwater;
    int depth;
} WGBiome;
typedef struct WGOre {
    char name[WG_NAME];
    uint32_t salt;
    int block;
    int minY;
    int maxY;
    int size;
    int spacing;
    int biome;
    int field;
    WGOreDistribution distribution;
    float chance;
    float threshold;
    bool replaces[256];
} WGOre;
typedef struct WGBlock {
    int x, y, z, id;
} WGBlock;
typedef struct WGStructure {
    char name[WG_NAME];
    uint32_t salt;
    int spacing;
    int biome;
    int minY;
    int maxY;
    int maxSlope;
    int foundation;
    int foundationDepth;
    float chance;
    bool rotate;
    bool airOnly;
    int count;
    int radius;
    int minDY;
    int maxDY;
    WGBlock blocks[WG_MAX_BLOCKS];
} WGStructure;
typedef struct WGRule {
    int match;
    int when;
    int block;
    int dy;
    bool descending;
} WGRule;
// Geometric parameters are field indices, evaluated at the command cursor.
typedef struct WGCommand {
    WGCommandType op;
    int from;
    int to;
    int when;
    int block;
    int steps;
    int dx;
    int dy;
    int dz;
    int radius;
    int bounds;
} WGCommand;
typedef struct WGFeature {
    char name[WG_NAME];
    int when;
    int paddingMin[3];
    int paddingMax[3];
    int count;
    WGCommand commands[WG_MAX_COMMANDS];
} WGFeature;
// Per-column or per-command cache; never shared between generation threads.
typedef struct WGEval {
    Vector3 position;
    Vector3 origin;
    float step;
    float steps;
    int evaluationVersion;
    int cachedVersions[WG_MAX_FIELDS];
    float values[WG_MAX_FIELDS];
} WGEval;
// Built during Lua mod loading, then frozen before chunk generation begins.
typedef struct WGConfig {
    int seed;
    bool frozen;
    bool bounded;
    int material;
    int skyField;
    int ceiling;
    int seaLevel;
    int minY;
    int maxY;
    int density;
    int caves;
    int temperature;
    int moisture;
    char id[WG_NAME];
    int version;
    bool opaque[256];
    int fieldCount;
    int biomeCount;
    int oreCount;
    int structureCount;
    WGField fields[WG_MAX_FIELDS];
    WGBiome biomes[WG_MAX_BIOMES];
    WGOre ores[WG_MAX_ORES];
    WGStructure structures[WG_MAX_STRUCTURES];
    int ruleCount;
    int featureCount;
    WGRule rules[WG_MAX_RULES];
    WGFeature features[WG_MAX_FEATURES];
} WGConfig;

extern WGConfig worldgen;
void Worldgen_Reset(int seed);
bool Worldgen_Freeze(void);
void Worldgen_Generate(Chunk *chunk);
void Worldgen_SkyMask(Chunk *chunk);
uint32_t Worldgen_Hash(const char *name);
float Worldgen_Field(int field, float x, float y, float z);
void Worldgen_EvalInit(WGEval *e, Vector3 position, Vector3 origin);
void Worldgen_EvalY(WGEval *e, float y);
float Worldgen_Eval(WGEval *e, int field);
void Worldgen_Features(Chunk *chunk);
void Worldgen_ClearFeatures(void);
void Worldgen_ClearSkyCache(void);
bool Worldgen_CachedSkyMask(Chunk *chunk);

#endif
