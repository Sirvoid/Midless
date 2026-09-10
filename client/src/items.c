#include "items.h"
#include "gui/itemspritemesh.h"
#include "textures.h"
#include "block/block.h"
#include "raymath.h"
#include "rlgl.h"
#include <string.h>

static ItemDefinition definitions[ITEM_LIMIT];
static struct { Mesh mesh; unsigned texture; } models[ITEM_LIMIT];
static Material material;

static const Vector3 spriteRotationDegrees = {-25.0f, 270.0f, 0.0f};
static const Vector3 thirdPersonSpriteRotationDegrees = {180.0f, 90.0f, 0.0f};
static const float spriteScale = 2.0f;

// Transparent sprite pixels must not hide geometry drawn behind them.
static const char *spriteFragmentShader =
#if defined(PLATFORM_WEB)
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 fragTexCoord;\n"
    "varying vec4 fragColor;\n"
    "#define SAMPLE texture2D\n"
    "#define OUTPUT gl_FragColor\n"
#else
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "out vec4 finalColor;\n"
    "#define SAMPLE texture\n"
    "#define OUTPUT finalColor\n"
#endif
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "void main() {\n"
    "    vec4 pixel = SAMPLE(texture0, fragTexCoord);\n"
    "    if (pixel.a < 0.5) discard;\n"
    "    OUTPUT = vec4(pixel.rgb * colDiffuse.rgb * fragColor.rgb, 1.0);\n"
    "}\n";

void ClientItems_Define(int id, const ItemDefinition *definition) {
    definitions[id] = *definition;
    const ItemDefinition *item = definition;
    Item_SetMaxStack(id,item->maxStack);
    if (models[id].mesh.vertexCount) UnloadMesh(models[id].mesh);
    memset(&models[id],0,sizeof(models[id]));
}
void ClientItems_Reset(void) {
    for (int i=0;i<ITEM_LIMIT;i++) if (models[i].mesh.vertexCount) UnloadMesh(models[i].mesh);
    memset(models,0,sizeof(models)); memset(definitions,0,sizeof(definitions));
    if (material.maps) { material.maps[MATERIAL_MAP_DIFFUSE].texture.id=rlGetTextureIdDefault(); UnloadMaterial(material); material=(Material){0}; }
}
const char *ClientItems_Name(int id) {
    if (id<0 || id>=ITEM_LIMIT) return "Unknown item";
    if (definitions[id].name[0]) return definitions[id].name;
    if (definitions[id].identifier[0]) return definitions[id].identifier;
    return id<256 ? Block_GetDefinition(id)->name : "Unknown item";
}
void ClientItems_Draw(int id, Rectangle bounds) {
    if (id<256 || id>=ITEM_LIMIT) return;
    Texture2D texture=definitions[id].texture ? ClientTextures_Get(definitions[id].texture) : (Texture2D){0};
    if (!texture.id) { DrawRectangleRec(bounds,MAGENTA); return; }
    float scale=fminf(bounds.width/texture.width,bounds.height/texture.height);
    Rectangle target={bounds.x+(bounds.width-texture.width*scale)/2,bounds.y+(bounds.height-texture.height*scale)/2,
        texture.width*scale,texture.height*scale};
    DrawTexturePro(texture,(Rectangle){0,0,texture.width,texture.height},target,(Vector2){0},0,WHITE);
}
bool ClientItems_Draw3D(int id, Matrix transform, float brightness, bool thirdPerson) {
    if (id<256 || id>=ITEM_LIMIT) return false;
    Texture2D texture=definitions[id].texture ? ClientTextures_Get(definitions[id].texture) : (Texture2D){0};
    if (!texture.id) return false;
    if (models[id].texture!=texture.id) {
        if (models[id].mesh.vertexCount) UnloadMesh(models[id].mesh);
        Image image=LoadImageFromTexture(texture);
        models[id].mesh=image.data?ItemSprite_CreateMesh(image):(Mesh){0}; models[id].texture=texture.id;
        if (models[id].mesh.vertexCount) UploadMesh(&models[id].mesh,false);
        if (image.data) UnloadImage(image);
    }
    if (!models[id].mesh.vertexCount) return false;
    if (!material.maps) {
        material=LoadMaterialDefault();
        material.shader=LoadShaderFromMemory(NULL,spriteFragmentShader);
    }
    unsigned char light=Clamp(brightness,0,1)*255;
    material.maps[MATERIAL_MAP_DIFFUSE].texture=texture;
    material.maps[MATERIAL_MAP_DIFFUSE].color=(Color){light,light,light,255};
    Vector3 rotation = thirdPerson ? thirdPersonSpriteRotationDegrees : spriteRotationDegrees;
    Matrix orientation = MatrixRotateXYZ(Vector3Scale(rotation, DEG2RAD));
    Matrix localTransform = MatrixMultiply(MatrixScale(spriteScale, spriteScale, spriteScale), orientation);
    transform = MatrixMultiply(localTransform, transform);
    rlDisableBackfaceCulling(); DrawMesh(models[id].mesh,material,transform); rlEnableBackfaceCulling();
    return true;
}
