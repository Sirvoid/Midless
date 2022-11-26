#include "raylib.h"
#include "resource.h"

Image Resource_LoadImage(const char* fileName) {
    #if defined(PLATFORM_WEB)
        Image image = LoadImage(TextFormat("client/bin/textures/%s", fileName)); 
    #else
        Image image = LoadImage(TextFormat("textures/%s", fileName)); 
    #endif

    return image;
}

Texture2D Resource_LoadTexture(const char* fileName) {
    Image image = Resource_LoadImage(fileName);
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);

    return texture;
}