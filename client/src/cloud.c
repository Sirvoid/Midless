#include <math.h>
#include <string.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "FastNoiseLite.h"
#include "cloud.h"
#include "player.h"

#define CLOUD_PATTERN_SIZE 64
#define CLOUD_RADIUS 24
#define CLOUD_CELL_SIZE 12.0f
#define CLOUD_HEIGHT 136.0f
#define CLOUD_THICKNESS 4.0f
#define CLOUD_SPEED 1.0f
#define CLOUD_MAX_VERTICES ((CLOUD_RADIUS * 2 + 1) * (CLOUD_RADIUS * 2 + 1) * 36)

static unsigned char pattern[CLOUD_PATTERN_SIZE * CLOUD_PATTERN_SIZE];
static Mesh mesh;
static Material material;
static Shader shader;
static float windOffset;
static int meshBaseX;
static int meshBaseZ;
static bool meshBuilt;

static int Cloud_Wrap(int value) {
    value %= CLOUD_PATTERN_SIZE;
    return value < 0 ? value + CLOUD_PATTERN_SIZE : value;
}

static bool HasCell(int x, int z) {
    return pattern[Cloud_Wrap(z) * CLOUD_PATTERN_SIZE + Cloud_Wrap(x)] != 0;
}

static void AddVertex(float *vertices, float *normals, unsigned char *colors, int *count,
                      Vector3 position, Vector3 normal, unsigned char shade, unsigned char alpha) {
    int index = *count * 3;
    vertices[index] = position.x;
    vertices[index + 1] = position.y;
    vertices[index + 2] = position.z;
    normals[index] = normal.x;
    normals[index + 1] = normal.y;
    normals[index + 2] = normal.z;
    int colorIndex = *count * 4;
    colors[colorIndex] = shade;
    colors[colorIndex + 1] = shade;
    colors[colorIndex + 2] = shade;
    colors[colorIndex + 3] = alpha;
    (*count)++;
}

static void AddQuad(float *vertices, float *normals, unsigned char *colors, int *count,
                    Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector3 normal,
                    unsigned char shade, unsigned char alpha) {
    AddVertex(vertices, normals, colors, count, a, normal, shade, alpha);
    AddVertex(vertices, normals, colors, count, b, normal, shade, alpha);
    AddVertex(vertices, normals, colors, count, c, normal, shade, alpha);
    AddVertex(vertices, normals, colors, count, a, normal, shade, alpha);
    AddVertex(vertices, normals, colors, count, c, normal, shade, alpha);
    AddVertex(vertices, normals, colors, count, d, normal, shade, alpha);
}

static void AddCell(float *vertices, float *normals, unsigned char *colors, int *count,
                    int localX, int localZ, int patternX, int patternZ) {
    float x0 = localX * CLOUD_CELL_SIZE, x1 = x0 + CLOUD_CELL_SIZE;
    float z0 = localZ * CLOUD_CELL_SIZE, z1 = z0 + CLOUD_CELL_SIZE;
    float y0 = 0.0f, y1 = CLOUD_THICKNESS;
    unsigned char alpha = 255;

    AddQuad(vertices, normals, colors, count, (Vector3){x0,y1,z0}, (Vector3){x0,y1,z1},
        (Vector3){x1,y1,z1}, (Vector3){x1,y1,z0}, (Vector3){0,1,0}, 255, alpha);
    AddQuad(vertices, normals, colors, count, (Vector3){x0,y0,z0}, (Vector3){x1,y0,z0},
        (Vector3){x1,y0,z1}, (Vector3){x0,y0,z1}, (Vector3){0,-1,0}, 145, alpha);
    if (!HasCell(patternX - 1, patternZ)) AddQuad(vertices, normals, colors, count,
        (Vector3){x0,y0,z0}, (Vector3){x0,y0,z1}, (Vector3){x0,y1,z1}, (Vector3){x0,y1,z0}, (Vector3){-1,0,0}, 205, alpha);
    if (!HasCell(patternX + 1, patternZ)) AddQuad(vertices, normals, colors, count,
        (Vector3){x1,y0,z1}, (Vector3){x1,y0,z0}, (Vector3){x1,y1,z0}, (Vector3){x1,y1,z1}, (Vector3){1,0,0}, 205, alpha);
    if (!HasCell(patternX, patternZ - 1)) AddQuad(vertices, normals, colors, count,
        (Vector3){x1,y0,z0}, (Vector3){x0,y0,z0}, (Vector3){x0,y1,z0}, (Vector3){x1,y1,z0}, (Vector3){0,0,-1}, 180, alpha);
    if (!HasCell(patternX, patternZ + 1)) AddQuad(vertices, normals, colors, count,
        (Vector3){x0,y0,z1}, (Vector3){x1,y0,z1}, (Vector3){x1,y1,z1}, (Vector3){x0,y1,z1}, (Vector3){0,0,1}, 180, alpha);
}

static void RebuildMesh(int baseX, int baseZ) {
    if (meshBuilt) UnloadMesh(mesh);
    mesh = (Mesh){0};
    float *vertices = MemAlloc(CLOUD_MAX_VERTICES * 3 * sizeof(float));
    float *normals = MemAlloc(CLOUD_MAX_VERTICES * 3 * sizeof(float));
    unsigned char *colors = MemAlloc(CLOUD_MAX_VERTICES * 4);
    int vertexCount = 0;
    for (int z = -CLOUD_RADIUS; z <= CLOUD_RADIUS; z++) {
        for (int x = -CLOUD_RADIUS; x <= CLOUD_RADIUS; x++) {
            if (HasCell(baseX + x, baseZ + z)) {
                AddCell(vertices, normals, colors, &vertexCount, x, z, baseX + x, baseZ + z);
            }
        }
    }
    mesh.vertexCount = vertexCount;
    mesh.triangleCount = vertexCount / 3;
    mesh.vertices = vertices;
    mesh.normals = normals;
    mesh.colors = colors;
    mesh.texcoords = MemAlloc(vertexCount * 2 * sizeof(float));
    memset(mesh.texcoords, 0, vertexCount * 2 * sizeof(float));
    UploadMesh(&mesh, false);
    meshBuilt = true;
    meshBaseX = baseX;
    meshBaseZ = baseZ;
}

void Cloud_Init(void) {
    fnl_state noise = fnlCreateState();
    noise.seed = 1337;
    noise.noise_type = FNL_NOISE_OPENSIMPLEX2S;
    noise.fractal_type = FNL_FRACTAL_FBM;
    noise.octaves = 3;
    noise.frequency = 0.075f;
    for (int z = 0; z < CLOUD_PATTERN_SIZE; z++) {
        for (int x = 0; x < CLOUD_PATTERN_SIZE; x++) {
            pattern[z * CLOUD_PATTERN_SIZE + x] =
                fnlGetNoise2D(&noise, (float)x, (float)z) > -0.06f;
        }
    }
    #if defined(PLATFORM_WEB)
        const char *vertexShader =
            #include "chunk/shaders/cloud_shader_gl100.vs"
        ;
        const char *fragmentShader =
            #include "chunk/shaders/cloud_shader_gl100.fs"
        ;
    #else
        const char *vertexShader =
            #include "chunk/shaders/cloud_shader.vs"
        ;
        const char *fragmentShader =
            #include "chunk/shaders/cloud_shader.fs"
        ;
    #endif
    shader = LoadShaderFromMemory(vertexShader, fragmentShader);
    material = LoadMaterialDefault();
    material.shader = shader;
    RebuildMesh(0, 0);
}

void Cloud_Shutdown(void) {
    if (meshBuilt) UnloadMesh(mesh);
    UnloadMaterial(material);
    UnloadShader(shader);
    meshBuilt = false;
}

void Cloud_Update(float deltaTime) {
    windOffset += CLOUD_SPEED * deltaTime;
}

void Cloud_Draw(Vector3 cameraPosition, float sunlightStrength) {
    int baseX = (int)floorf((cameraPosition.x - windOffset) / CLOUD_CELL_SIZE);
    int baseZ = (int)floorf(cameraPosition.z / CLOUD_CELL_SIZE);
    if (baseX != meshBaseX || baseZ != meshBaseZ) RebuildMesh(baseX, baseZ);

    unsigned char brightness = (unsigned char)(160.0f + 95.0f * sunlightStrength);
    material.maps[MATERIAL_MAP_DIFFUSE].color = (Color){brightness, brightness, brightness, 255};
    float fogEnd = CLOUD_RADIUS * CLOUD_CELL_SIZE;
    float fogStart = fogEnd * 0.7f;
    float fogColor[3] = {
        (140.0f / 255.0f) * sunlightStrength,
        (210.0f / 255.0f) * sunlightStrength,
        (240.0f / 255.0f) * sunlightStrength
    };
    Color liquidTint;
    if (Player_GetCameraLiquidTint(&liquidTint)) {
        fogStart = 10.0f;
        fogEnd = 32.0f;
        fogColor[0] = liquidTint.r / 255.0f;
        fogColor[1] = liquidTint.g / 255.0f;
        fogColor[2] = liquidTint.b / 255.0f;
    }
    SetShaderValue(shader, GetShaderLocation(shader, "cameraPosition"),
                   &cameraPosition, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, GetShaderLocation(shader, "fogColor"), fogColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, GetShaderLocation(shader, "fogStart"), &fogStart, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, GetShaderLocation(shader, "fogEnd"), &fogEnd, SHADER_UNIFORM_FLOAT);
    Matrix transform = MatrixTranslate(baseX * CLOUD_CELL_SIZE + windOffset,
                                       CLOUD_HEIGHT, baseZ * CLOUD_CELL_SIZE);
    rlDisableBackfaceCulling();
    DrawMesh(mesh, material, transform);
    rlEnableBackfaceCulling();
}
