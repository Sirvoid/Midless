/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "version.h"
#include "worldgen.h"
#include "worldgenerator.h"
#include "world.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

WGConfig worldgen;

uint32_t Worldgen_Hash(const char *name) {
    uint32_t h = 2166136261u;
    while (*name)
        h = (h ^ (unsigned char)*name++) * 16777619u;
    return h;
}

static uint32_t MixSeed(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    return x ^ (x >> 16);
}

static uint32_t RandomAtPosition(int x, int y, int z, uint32_t salt) {
    return MixSeed((uint32_t)worldgen.seed ^ MixSeed((uint32_t)x) ^
                   MixSeed((uint32_t)y + 0x9e3779b9u) ^ MixSeed((uint32_t)z + 0x85ebca6bu) ^ salt);
}

static float RandomToUnitInterval(uint32_t n) {
    return (n >> 8) * (1.0f / 16777216.0f);
}

static int FloorDivide(int a, int b) {
    int q = a / b;
    return q - (a % b < 0);
}

void Worldgen_Reset(int seed) {
    Worldgen_ClearFeatures();
    Worldgen_ClearSkyCache();
    memset(&worldgen, 0, sizeof(worldgen));
    worldgen.bounded = true;
    worldgen.material = worldgen.skyField = worldgen.ceiling = -1;
    worldgen.seed = seed;
    worldgen.seaLevel = 48;
    worldgen.minY = -128;
    worldgen.maxY = 256;
    worldgen.density = worldgen.caves = worldgen.temperature = worldgen.moisture = -1;
    strcpy(worldgen.id, "builtin:flat");
    worldgen.version = WORLDGEN_DEFAULT_VERSION;
    worldgen.biomeCount = 1;
    worldgen.biomes[0] = (WGBiome){.name = "builtin:plains",
                                   .spread = 1,
                                   .height = 64,
                                   .heightField = -1,
                                   .top = 3,
                                   .filler = 2,
                                   .stone = 1,
                                   .underwater = 6,
                                   .depth = 3};
}

typedef struct TerrainColumn {
    WGEval eval;
    float height;
    int biome;
} TerrainColumn;
static void InitializeTerrainColumn(TerrainColumn *column, int x, int z) {
    column->biome = 0;
    Worldgen_EvalInit(&column->eval, (Vector3){x, 0, z}, (Vector3){x, 0, z});
    float temperature =
        worldgen.temperature < 0 ? 0 : Worldgen_Eval(&column->eval, worldgen.temperature);
    float moisture = worldgen.moisture < 0 ? 0 : Worldgen_Eval(&column->eval, worldgen.moisture);
    double totalWeight = 0, strongestWeight = -1, blendedHeight = 0;
    for (int i = 0; i < worldgen.biomeCount; i++) {
        const WGBiome *biome = &worldgen.biomes[i];
        double temperatureDistance = (double)temperature - biome->temperature,
               moistureDistance = (double)moisture - biome->moisture;
        double weight = 1 / (0.0001 + (temperatureDistance * temperatureDistance +
                                       moistureDistance * moistureDistance) /
                                          (biome->spread * biome->spread));
        double height =
            biome->height +
            (double)biome->variation *
                (biome->heightField < 0 ? 0 : Worldgen_Eval(&column->eval, biome->heightField));
        blendedHeight += height * weight;
        totalWeight += weight;
        if (weight > strongestWeight) {
            strongestWeight = weight;
            column->biome = i;
        }
    }
    column->height = (float)fmax(-4097, fmin(4097, blendedHeight / totalWeight));
}

static bool IsTerrainSolid(TerrainColumn *column, int x, int y, int z) {
    if (worldgen.bounded && (y < worldgen.minY || y > worldgen.maxY))
        return false;
    if (worldgen.density < 0 && worldgen.caves < 0)
        return y <= floorf(column->height);
    Worldgen_EvalY(&column->eval, y);
    bool solid = worldgen.density < 0 ? y <= floorf(column->height)
                                      : Worldgen_Eval(&column->eval, worldgen.density) > 0;
    return solid && (worldgen.caves < 0 || Worldgen_Eval(&column->eval, worldgen.caves) <= 0);
}

static int FindSurfaceHeight(int x, int z, TerrainColumn *column) {
    InitializeTerrainColumn(column, x, z);
    if (worldgen.density < 0 && worldgen.caves < 0)
        return (int)fmaxf(worldgen.minY - 1, fminf(worldgen.maxY, floorf(column->height)));
    for (int y = worldgen.maxY; y >= worldgen.minY; y--)
        if (IsTerrainSolid(column, x, y, z))
            return y;
    return worldgen.minY - 1;
}

static bool IsInsideChunk(Chunk *chunk, int x, int y, int z) {
    return x >= chunk->blockPosition.x && x < chunk->blockPosition.x + CHUNK_SIZE_X &&
           y >= chunk->blockPosition.y && y < chunk->blockPosition.y + CHUNK_SIZE_Y &&
           z >= chunk->blockPosition.z && z < chunk->blockPosition.z + CHUNK_SIZE_Z;
}

static int WorldPositionToIndex(Chunk *chunk, int x, int y, int z) {
    return ((y - (int)chunk->blockPosition.y) * CHUNK_SIZE_Z + z - (int)chunk->blockPosition.z) *
               CHUNK_SIZE_X +
           x - (int)chunk->blockPosition.x;
}

static void PlaceOreSphere(Chunk *chunk, const WGOre *ore, int x, int y, int z, int radius) {
    for (int dz = -radius; dz <= radius; dz++)
        for (int dx = -radius; dx <= radius; dx++) {
            TerrainColumn column;
            if (ore->biome >= 0) {
                InitializeTerrainColumn(&column, x + dx, z + dz);
                if (column.biome != ore->biome)
                    continue;
            }
            for (int dy = -radius; dy <= radius; dy++) {
                int wx = x + dx, wy = y + dy, wz = z + dz;
                if (dx * dx + dy * dy + dz * dz > radius * radius || wy < ore->minY ||
                    wy > ore->maxY || !IsInsideChunk(chunk, wx, wy, wz))
                    continue;
                int i = WorldPositionToIndex(chunk, wx, wy, wz);
                if (chunk->data[i] < 256 && ore->replaces[chunk->data[i]])
                    chunk->data[i] = ore->block;
            }
        }
}

static void GenerateOres(Chunk *chunk) {
    int x0 = (int)chunk->blockPosition.x, y0 = (int)chunk->blockPosition.y,
        z0 = (int)chunk->blockPosition.z;
    for (int oreIndex = 0; oreIndex < worldgen.oreCount; oreIndex++) {
        const WGOre *ore = &worldgen.ores[oreIndex];
        if (y0 > ore->maxY || y0 + CHUNK_SIZE_Y - 1 < ore->minY)
            continue;
        if ((ore->distribution == WG_ORE_LAYERS || ore->distribution == WG_ORE_NOISE)) {
            for (int z = z0; z < z0 + CHUNK_SIZE_Z; z++)
                for (int x = x0; x < x0 + CHUNK_SIZE_X; x++) {
                    TerrainColumn column;
                    InitializeTerrainColumn(&column, x, z);
                    if (ore->biome >= 0 && column.biome != ore->biome)
                        continue;
                    for (int y = y0; y < y0 + CHUNK_SIZE_Y; y++) {
                        int i = WorldPositionToIndex(chunk, x, y, z);
                        if (y < ore->minY || y > ore->maxY || chunk->data[i] >= 256 ||
                            !ore->replaces[chunk->data[i]])
                            continue;
                        float noiseValue = ore->field < 0 ? 0 : Worldgen_Field(ore->field, x, y, z);
                        bool place =
                            ore->distribution == WG_ORE_LAYERS
                                ? fabs((double)y - floor((ore->minY + ore->maxY) * 0.5 +
                                                         (double)noiseValue * ore->size)) <=
                                      ore->size / 2
                                : noiseValue > ore->threshold;
                        if (place && RandomToUnitInterval(RandomAtPosition(x, y, z, ore->salt)) <
                                         ore->chance)
                            chunk->data[i] = ore->block;
                    }
                }
            continue;
        }
        /* Enumerate every origin cell whose bounded deposit can intersect us. */
        int reach = ore->distribution == WG_ORE_CLUSTERS ? ore->size : ore->size + 2;
        for (int cz = FloorDivide(z0 - reach, ore->spacing);
             cz <= FloorDivide(z0 + CHUNK_SIZE_Z - 1 + reach, ore->spacing); cz++)
            for (int cx = FloorDivide(x0 - reach, ore->spacing);
                 cx <= FloorDivide(x0 + CHUNK_SIZE_X - 1 + reach, ore->spacing); cx++)
                for (int cy = FloorDivide(y0 - reach, ore->spacing);
                     cy <= FloorDivide(y0 + CHUNK_SIZE_Y - 1 + reach, ore->spacing); cy++) {
                    uint32_t randomValue = RandomAtPosition(cx, cy, cz, ore->salt);
                    if (RandomToUnitInterval(randomValue) >= ore->chance)
                        continue;
                    int x = cx * ore->spacing + (int)(MixSeed(randomValue + 1) % ore->spacing);
                    int y = cy * ore->spacing + (int)(MixSeed(randomValue + 2) % ore->spacing);
                    int z = cz * ore->spacing + (int)(MixSeed(randomValue + 3) % ore->spacing);
                    if (ore->distribution == WG_ORE_CLUSTERS)
                        PlaceOreSphere(chunk, ore, x, y, z, ore->size);
                    else
                        for (int step = 0; step < ore->size; step++) {
                            PlaceOreSphere(chunk, ore, x, y, z, 1);
                            uint32_t d = MixSeed(randomValue + 4 + step);
                            switch (d % 6) {
                                case 0:
                                    x++;
                                    break;
                                case 1:
                                    x--;
                                    break;
                                case 2:
                                    y++;
                                    break;
                                case 3:
                                    y--;
                                    break;
                                case 4:
                                    z++;
                                    break;
                                default:
                                    z--;
                            }
                        }
                }
    }
}

static void RotateStructureOffset(int *x, int *z, int rotation) {
    int a = *x, b = *z;
    if (rotation == 1) {
        *x = -b;
        *z = a;
    }
    if (rotation == 2) {
        *x = -a;
        *z = -b;
    }
    if (rotation == 3) {
        *x = b;
        *z = -a;
    }
}

static void PlaceStructureBlock(Chunk *chunk, const WGStructure *structure, int x, int y, int z,
                                int id) {
    if (!IsInsideChunk(chunk, x, y, z))
        return;
    int i = WorldPositionToIndex(chunk, x, y, z);
    if (!structure->airOnly || chunk->data[i] == 0)
        chunk->data[i] = id;
}

static void GenerateStructures(Chunk *chunk) {
    int x0 = chunk->blockPosition.x, z0 = chunk->blockPosition.z;
    for (int structureIndex = 0; structureIndex < worldgen.structureCount; structureIndex++) {
        const WGStructure *structure = &worldgen.structures[structureIndex];
        for (int cz = FloorDivide(z0 - structure->radius, structure->spacing);
             cz <= FloorDivide(z0 + CHUNK_SIZE_Z - 1 + structure->radius, structure->spacing); cz++)
            for (int cx = FloorDivide(x0 - structure->radius, structure->spacing);
                 cx <= FloorDivide(x0 + CHUNK_SIZE_X - 1 + structure->radius, structure->spacing);
                 cx++) {
                uint32_t randomValue = RandomAtPosition(cx, 0, cz, structure->salt);
                if (RandomToUnitInterval(randomValue) >= structure->chance)
                    continue;
                int x =
                    cx * structure->spacing + (int)(MixSeed(randomValue + 1) % structure->spacing);
                int z =
                    cz * structure->spacing + (int)(MixSeed(randomValue + 2) % structure->spacing);
                TerrainColumn column;
                int y = FindSurfaceHeight(x, z, &column) + 1;
                if (y <= worldgen.minY || y < structure->minY || y > structure->maxY ||
                    (structure->biome >= 0 && column.biome != structure->biome))
                    continue;
                if (y + structure->maxDY < chunk->blockPosition.y ||
                    y + structure->minDY - structure->foundationDepth >=
                        chunk->blockPosition.y + CHUNK_SIZE_Y)
                    continue;
                bool valid = true;
                for (int dz = -1; dz <= 1 && valid; dz++)
                    for (int dx = -1; dx <= 1; dx++) {
                        TerrainColumn corner;
                        if (abs(FindSurfaceHeight(x + dx * structure->radius,
                                                  z + dz * structure->radius, &corner) +
                                1 - y) > structure->maxSlope) {
                            valid = false;
                            break;
                        }
                    }
                if (!valid)
                    continue;
                int rotation = structure->rotate ? MixSeed(randomValue + 3) % 4 : 0;
                for (int i = 0; i < structure->count; i++) {
                    WGBlock block = structure->blocks[i];
                    RotateStructureOffset(&block.x, &block.z, rotation);
                    PlaceStructureBlock(chunk, structure, x + block.x, y + block.y, z + block.z,
                                        block.id);
                    if (structure->foundation && block.id && block.y == structure->minDY) {
                        TerrainColumn support;
                        InitializeTerrainColumn(&support, x + block.x, z + block.z);
                        for (int d = 1; d <= structure->foundationDepth; d++) {
                            int fy = y + block.y - d;
                            if (IsTerrainSolid(&support, x + block.x, fy, z + block.z))
                                break;
                            PlaceStructureBlock(chunk, structure, x + block.x, fy, z + block.z,
                                                structure->foundation);
                        }
                    }
                }
            }
    }
}

static void ApplyMaterialRules(Chunk *chunk) {
    for (int ruleIndex = 0; ruleIndex < worldgen.ruleCount; ruleIndex++) {
        WGRule *rule = &worldgen.rules[ruleIndex];
        for (int step = 0; step < CHUNK_SIZE; step++) {
            int i = rule->descending ? CHUNK_SIZE - 1 - step : step;
            if (rule->match >= 0 && chunk->data[i] != rule->match)
                continue;
            int target = i + rule->dy * CHUNK_SIZE_XZ;
            if (target < 0 || target >= CHUNK_SIZE)
                continue;
            Vector3 position = ServerChunk_IndexToPos(i);
            position.x += chunk->blockPosition.x;
            position.y += chunk->blockPosition.y;
            position.z += chunk->blockPosition.z;
            if (rule->when < 0 ||
                Worldgen_Field(rule->when, position.x, position.y, position.z) != 0)
                chunk->data[target] = rule->block;
        }
    }
}

void Worldgen_Generate(Chunk *chunk) {
    int x0 = chunk->blockPosition.x, y0 = chunk->blockPosition.y, z0 = chunk->blockPosition.z;
    if (worldgen.material >= 0) {
        for (int z = z0; z < z0 + CHUNK_SIZE_Z; z++)
            for (int x = x0; x < x0 + CHUNK_SIZE_X; x++) {
                WGEval context;
                Worldgen_EvalInit(&context, (Vector3){x, y0, z}, (Vector3){x, y0, z});
                for (int y = y0; y < y0 + CHUNK_SIZE_Y; y++) {
                    Worldgen_EvalY(&context, y);
                    float id = worldgen.bounded && (y < worldgen.minY || y > worldgen.maxY)
                                   ? 0
                                   : Worldgen_Eval(&context, worldgen.material);
                    chunk->data[WorldPositionToIndex(chunk, x, y, z)] =
                        id >= 0 && id <= 255 ? (int)id : 0;
                }
            }
    } else {
        for (int z = z0; z < z0 + CHUNK_SIZE_Z; z++)
            for (int x = x0; x < x0 + CHUNK_SIZE_X; x++) {
                TerrainColumn column;
                InitializeTerrainColumn(&column, x, z);
                const WGBiome *biome = &worldgen.biomes[column.biome];
                /* Halo above the chunk makes surface layers independent of load order. */
                int depth = 0;
                for (int y = y0 + CHUNK_SIZE_Y - 1 + biome->depth + 1; y >= y0; y--) {
                    bool solid = IsTerrainSolid(&column, x, y, z);
                    if (!solid)
                        depth = 0;
                    else
                        depth++;
                    if (y >= y0 + CHUNK_SIZE_Y)
                        continue;
                    int block = 0;
                    if (solid)
                        block = depth == 1
                                    ? (y <= worldgen.seaLevel ? biome->underwater : biome->top)
                                : depth <= biome->depth + 1 ? biome->filler
                                                            : biome->stone;
                    else if (y >= worldgen.minY && y <= worldgen.seaLevel)
                        block = 5;
                    chunk->data[WorldPositionToIndex(chunk, x, y, z)] = block;
                }
            }
    }
    ApplyMaterialRules(chunk);
    GenerateOres(chunk);
    GenerateStructures(chunk);
    Worldgen_Features(chunk);
}

void Worldgen_SkyMask(Chunk *chunk) {
    if (Worldgen_CachedSkyMask(chunk)) return;
    if (worldgen.skyField >= 0) {
        memset(chunk->skyMask, 255, sizeof(chunk->skyMask));
        for (int z = 0; z < CHUNK_SIZE_Z; z++)
            for (int x = 0; x < CHUNK_SIZE_X; x++) {
                int first = chunk->blockPosition.y + CHUNK_SIZE_Y;
                WGEval context;
                Vector3 position = {chunk->blockPosition.x + x, first, chunk->blockPosition.z + z};
                Worldgen_EvalInit(&context, position, position);
                int ceiling =
                    worldgen.ceiling < 0
                        ? worldgen.maxY
                        : (int)fmaxf(-4096, fminf(4096, Worldgen_Eval(&context, worldgen.ceiling)));
                for (int y = first; y <= ceiling; y++) {
                    Worldgen_EvalY(&context, y);
                    if (Worldgen_Eval(&context, worldgen.skyField) > 0) {
                        int column = z * CHUNK_SIZE_X + x;
                        chunk->skyMask[column >> 3] &= ~(1u << (column & 7));
                        break;
                    }
                }
            }
        return;
    }
    memset(chunk->skyMask, 0, sizeof(chunk->skyMask));
    /* Evaluate actual generated chunks above us, including structures and ores.
     * Only columns still blocked are retained; no world loads or recursive jobs. */
    memset(chunk->skyMask, 255, sizeof(chunk->skyMask));
    int ceiling = worldgen.maxY;
    for (int i = 0; i < worldgen.structureCount; i++)
        if (worldgen.maxY + 1 + worldgen.structures[i].maxDY > ceiling)
            ceiling = worldgen.maxY + 1 + worldgen.structures[i].maxDY;
    int first = chunk->blockPosition.y + CHUNK_SIZE_Y;
    bool opaqueTerrain = true;
    for (int i = 0; i < worldgen.biomeCount; i++) {
        WGBiome *biome = &worldgen.biomes[i];
        if (!worldgen.opaque[biome->top] || !worldgen.opaque[biome->filler] ||
            !worldgen.opaque[biome->stone] || !worldgen.opaque[biome->underwater])
            opaqueTerrain = false;
    }
    if (worldgen.structureCount == 0 && worldgen.oreCount == 0 && worldgen.featureCount == 0 &&
        worldgen.material < 0 && opaqueTerrain) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++)
            for (int x = 0; x < CHUNK_SIZE_X; x++) {
                TerrainColumn column;
                InitializeTerrainColumn(&column, chunk->blockPosition.x + x,
                                        chunk->blockPosition.z + z);
                bool blocked = false;
                if (worldgen.density < 0 && worldgen.caves < 0)
                    blocked = first <= fminf(worldgen.maxY, floorf(column.height));
                else
                    for (int y = first; y <= ceiling; y++)
                        if (IsTerrainSolid(&column, chunk->blockPosition.x + x, y,
                                           chunk->blockPosition.z + z)) {
                            blocked = true;
                            break;
                        }
                if (blocked) {
                    int i = z * CHUNK_SIZE_X + x;
                    chunk->skyMask[i >> 3] &= ~(1u << (i & 7));
                }
            }
        return;
    }
    Chunk above = {0};
    for (int y = first; y <= ceiling; y += CHUNK_SIZE_Y) {
        above.blockPosition = (Vector3){chunk->blockPosition.x, y, chunk->blockPosition.z};
        memset(above.data, 0, sizeof(above.data));
        above.position = (Vector3){above.blockPosition.x / CHUNK_SIZE_X, y / CHUNK_SIZE_Y,
                                   above.blockPosition.z / CHUNK_SIZE_Z};
        ServerWorldGenerator_Generate(&above);
        ServerWorldGenerator_GenerateStructures(&above);
        for (int z = 0; z < CHUNK_SIZE_Z; z++)
            for (int x = 0; x < CHUNK_SIZE_X; x++) {
                int column = z * CHUNK_SIZE_X + x;
                if (!(chunk->skyMask[column >> 3] & (1u << (column & 7))))
                    continue;
                for (int dy = 0; dy < CHUNK_SIZE_Y; dy++) {
                    int id = above.data[(dy * CHUNK_SIZE_Z + z) * CHUNK_SIZE_X + x];
                    if (id < 256 && worldgen.opaque[id]) {
                        chunk->skyMask[column >> 3] &= ~(1u << (column & 7));
                        break;
                    }
                }
            }
        bool any = false;
        for (int i = 0; i < CHUNK_SKY_MASK_SIZE; i++)
            if (chunk->skyMask[i])
                any = true;
        if (!any)
            break;
    }
}

static void HashInt(uint32_t *h, uint32_t value) {
    for (int i = 0; i < 4; i++) {
        *h = (*h ^ (value & 255)) * 16777619u;
        value >>= 8;
    }
}

static void HashFloat(uint32_t *h, float value) {
    uint32_t bits;
    memcpy(&bits, &value, 4);
    HashInt(h, bits);
}

static uint32_t CalculateDefinitionFingerprint(void) {
    uint32_t h = Worldgen_Hash(worldgen.id);
    HashInt(&h, worldgen.version);
    HashInt(&h, worldgen.seed);
    HashInt(&h, worldgen.bounded);
    HashInt(&h, worldgen.material);
    HashInt(&h, worldgen.skyField);
    HashInt(&h, worldgen.ceiling);
    HashInt(&h, worldgen.minY);
    HashInt(&h, worldgen.maxY);
    HashInt(&h, worldgen.seaLevel);
    HashInt(&h, worldgen.density);
    HashInt(&h, worldgen.caves);
    HashInt(&h, worldgen.temperature);
    HashInt(&h, worldgen.moisture);
    HashInt(&h, worldgen.fieldCount);
    HashInt(&h, worldgen.biomeCount);
    HashInt(&h, worldgen.oreCount);
    HashInt(&h, worldgen.structureCount);
    HashInt(&h, worldgen.ruleCount);
    HashInt(&h, worldgen.featureCount);
    for (int i = 0; i < worldgen.ruleCount; i++) {
        WGRule *r = &worldgen.rules[i];
        HashInt(&h, r->match);
        HashInt(&h, r->when);
        HashInt(&h, r->block);
        HashInt(&h, r->dy);
        HashInt(&h, r->descending);
    }
    for (int i = 0; i < worldgen.featureCount; i++) {
        WGFeature *f = &worldgen.features[i];
        HashInt(&h, Worldgen_Hash(f->name));
        HashInt(&h, f->when);
        HashInt(&h, f->count);
        for (int j = 0; j < 3; j++) {
            HashInt(&h, f->paddingMin[j]);
            HashInt(&h, f->paddingMax[j]);
        }
        for (int j = 0; j < f->count; j++) {
            WGCommand *c = &f->commands[j];
            HashInt(&h, c->op);
            HashInt(&h, c->from);
            HashInt(&h, c->to);
            HashInt(&h, c->when);
            HashInt(&h, c->block);
            HashInt(&h, c->steps);
            HashInt(&h, c->dx);
            HashInt(&h, c->dy);
            HashInt(&h, c->dz);
            HashInt(&h, c->radius);
            HashInt(&h, c->bounds);
        }
    }
    for (int i = 0; i < worldgen.fieldCount; i++) {
        WGField *f = &worldgen.fields[i];
        HashInt(&h, f->op);
        HashInt(&h, f->firstInput);
        HashInt(&h, f->secondInput);
        HashInt(&h, f->thirdInput);
        HashFloat(&h, f->value);
        HashInt(&h, f->seedOffset);
        HashInt(&h, f->seedScale);
        HashInt(&h, f->saltScale);
        HashInt(&h, f->noise.noise_type);
        HashInt(&h, f->noise.fractal_type);
        HashFloat(&h, f->noise.frequency);
        HashInt(&h, f->noise.octaves);
        HashFloat(&h, f->noise.lacunarity);
        HashFloat(&h, f->noise.gain);
    }
    for (int i = 0; i < worldgen.biomeCount; i++) {
        WGBiome *b = &worldgen.biomes[i];
        HashInt(&h, Worldgen_Hash(b->name));
        HashFloat(&h, b->temperature);
        HashFloat(&h, b->moisture);
        HashFloat(&h, b->spread);
        HashFloat(&h, b->height);
        HashFloat(&h, b->variation);
        HashInt(&h, b->heightField);
        HashInt(&h, b->top);
        HashInt(&h, b->filler);
        HashInt(&h, b->stone);
        HashInt(&h, b->underwater);
        HashInt(&h, b->depth);
    }
    for (int i = 0; i < worldgen.oreCount; i++) {
        WGOre *o = &worldgen.ores[i];
        HashInt(&h, o->salt);
        HashInt(&h, o->block);
        HashInt(&h, o->minY);
        HashInt(&h, o->maxY);
        HashInt(&h, o->size);
        HashInt(&h, o->spacing);
        HashInt(&h, o->biome);
        HashInt(&h, o->field);
        HashInt(&h, o->distribution);
        HashFloat(&h, o->chance);
        HashFloat(&h, o->threshold);
        for (int j = 0; j < 256; j++)
            HashInt(&h, o->replaces[j]);
    }
    for (int i = 0; i < worldgen.structureCount; i++) {
        WGStructure *s = &worldgen.structures[i];
        HashInt(&h, s->salt);
        HashInt(&h, s->spacing);
        HashInt(&h, s->biome);
        HashInt(&h, s->minY);
        HashInt(&h, s->maxY);
        HashInt(&h, s->maxSlope);
        HashInt(&h, s->foundation);
        HashInt(&h, s->foundationDepth);
        HashFloat(&h, s->chance);
        HashInt(&h, s->rotate);
        HashInt(&h, s->airOnly);
        HashInt(&h, s->count);
        for (int j = 0; j < s->count; j++) {
            WGBlock *b = &s->blocks[j];
            HashInt(&h, b->x);
            HashInt(&h, b->y);
            HashInt(&h, b->z);
            HashInt(&h, b->id);
        }
    }
    return h;
}

static bool ValidateBlockId(int id) {
    if (id >= 0 && id < 256 && (id <= BLOCK_DEFAULT_LAST_ID || serverWorld.hasBlockDefinition[id]))
        return true;
    TraceLog(LOG_ERROR, "Worldgen references undefined block %d", id);
    return false;
}

bool Worldgen_Freeze(void) {
    if (worldgen.frozen)
        return true;
    for (int i = 0; i < worldgen.ruleCount; i++)
        if (!ValidateBlockId(worldgen.rules[i].block))
            return false;
    for (int i = 0; i < worldgen.featureCount; i++)
        for (int j = 0; j < worldgen.features[i].count; j++)
            if (!ValidateBlockId(worldgen.features[i].commands[j].block))
                return false;
    for (int i = 0; i < worldgen.biomeCount; i++) {
        WGBiome *b = &worldgen.biomes[i];
        if (!ValidateBlockId(b->top) || !ValidateBlockId(b->filler) || !ValidateBlockId(b->stone) ||
            !ValidateBlockId(b->underwater))
            return false;
    }
    for (int i = 0; i < worldgen.oreCount; i++) {
        if (!ValidateBlockId(worldgen.ores[i].block))
            return false;
        for (int j = 1; j < 256; j++)
            if (worldgen.ores[i].replaces[j] && !ValidateBlockId(j))
                return false;
    }
    for (int i = 0; i < worldgen.structureCount; i++) {
        WGStructure *s = &worldgen.structures[i];
        if (!ValidateBlockId(s->foundation))
            return false;
        for (int j = 0; j < s->count; j++)
            if (!ValidateBlockId(s->blocks[j].id))
                return false;
    }
    for (int id = 0; id < 256; id++) {
        worldgen.opaque[id] = id > 0 && id != 5 && id != 11 && id != 12 && id != 13 && id != 14 &&
                              id != 15 && id != 17 && id != 18;
        if (serverWorld.hasBlockDefinition[id]) {
            BlockDefinition *b = &serverWorld.blockDefinitions[id];
            worldgen.opaque[id] = (!b->geometry.enabled || b->geometry.boxCount == 1) &&
                                  b->renderType == BLOCK_RENDER_OPAQUE && b->min[0] == 0 &&
                                  b->min[1] == 0 && b->min[2] == 0 && b->max[0] == 16 &&
                                  b->max[1] == 16 && b->max[2] == 16;
        }
    }
    /* Record the definition fingerprint with the seed. Refuse accidental seams
     * caused by enabling/disabling a worldgen mod on an established world. */
    uint32_t fingerprint = CalculateDefinitionFingerprint();
    char expected[160];
    snprintf(expected, sizeof(expected), "MIDLESS_WORLDGEN 1\n%s\n%d\n%08x\n", worldgen.id,
             worldgen.version, (unsigned)fingerprint);
    if (FileExists("world/worldgen.meta")) {
        char *saved = LoadFileText("world/worldgen.meta");
        bool match = saved && !strcmp(saved, expected);
        UnloadFileText(saved);
        if (!match) {
            TraceLog(LOG_ERROR, "Worldgen definitions differ from world/worldgen.meta. Restore the "
                                "world's mods or use a new world directory.");
            return false;
        }
    } else if (!SaveFileText("world/worldgen.meta", expected)) {
        TraceLog(LOG_ERROR, "Could not save world/worldgen.meta");
        return false;
    }
    for (int i = 0; i < worldgen.fieldCount; i++)
        worldgen.fields[i].noise.seed =
            (int)((uint32_t)worldgen.seed + (uint32_t)worldgen.fields[i].seedOffset);
    worldgen.frozen = true;
    TraceLog(LOG_INFO, "World generation: %s v%d (%d biomes, %d ores, %d structures)", worldgen.id,
             worldgen.version, worldgen.biomeCount, worldgen.oreCount, worldgen.structureCount);
    return true;
}
