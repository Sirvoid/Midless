#include "textures.h"
#include "textureprotocol.h"
#include "networking/packet.h"
#include "networking/networkhandler.h"
#include "world.h"
#include "entity/entitymodel.h"
#include "gui/blockitemrenderer.h"
#include "block/block.h"
#include "resource.h"
#include <string.h>
#include <stdint.h>

typedef struct ClientTexture { Texture2D gpu; uint32_t revision; } ClientTexture;
static ClientTexture textures[TEXTURE_LIMIT];
static Texture2D builtinTerrain;
static int desiredTerrain=1, currentTerrain=1, totalPixels;
static Color terrainSamples[256];
static struct { int id, size, received, width, height; uint32_t revision; unsigned char *data; } transfer;

Texture2D ClientTextures_Get(int id) {
    if(id==0) return entityModels[0].defaultTexture;
    if(id==1) return currentTerrain==1 ? builtinTerrain : textures[currentTerrain].gpu;
    if(id<2 || id>=TEXTURE_LIMIT) return (Texture2D){0};
    return textures[id].gpu;
}
void ClientTextures_UpdateLiquidTints(void) {
    for(int i=0;i<256;i++) if(blockDefinitions[i].colliderType==BLOCK_COLLIDER_LIQUID) {
        blockDefinitions[i].liquidTint=terrainSamples[blockDefinitions[i].textures[BLOCK_FACE_TOP]];
        blockDefinitions[i].liquidTint.a=105;
    }
}
static void Samples(Image image) {
    for(int i=0;i<256;i++) terrainSamples[i]=GetImageColor(image,(i%16)*16,(i/16)*16);
    ClientTextures_UpdateLiquidTints();
}
static void ApplyTerrain(void) {
    int selected=desiredTerrain;
    Texture2D t=selected==1 ? builtinTerrain : textures[selected].gpu;
    if(!t.id || t.width!=builtinTerrain.width || t.height!=builtinTerrain.height) {
        selected=currentTerrain;
        t=selected==1 ? builtinTerrain : textures[selected].gpu;
    }
    if(!t.id) return;
    currentTerrain=selected;
    World_ApplyTexture(t);
    BlockItemRenderer_SetTexture(t);
    Image image=LoadImageFromTexture(t);
    if(image.data) { Samples(image); UnloadImage(image); }
    EntityModel_RefreshTextures(1);
}
void ClientTextures_Init(Texture2D terrain) {
    builtinTerrain=terrain;
    Image image=Resource_LoadImage("terrain.png");
    if(image.data) { Samples(image); UnloadImage(image); }
}
static void Ack(int id,uint32_t revision,uint32_t offset) {
    unsigned char *packet=MemAlloc(TEXTURE_ACK_SIZE);
    if(!packet) return;
    packet[0]=PACKET_TEXTURE_ACK; Texture_Write16(packet+1,id); Texture_Write32(packet+3,revision); Texture_Write32(packet+7,offset);
    Network_Send(packet);
}
static void ClearTransfer(void) { MemFree(transfer.data); memset(&transfer,0,sizeof(transfer)); }
void ClientTextures_HandleBegin(void) {
    if(packetDataLength!=TEXTURE_BEGIN_SIZE) return;
    int id=Texture_Read16(packetData+1);
    uint32_t revision=Texture_Read32(packetData+3), size=Texture_Read32(packetData+7);
    int width=Texture_Read16(packetData+11),height=Texture_Read16(packetData+13);
    if(id<2 || id>=TEXTURE_LIMIT) return;
    ClearTransfer();
    if(!revision || revision<=textures[id].revision || size<33 || size>TEXTURE_MAX_BYTES ||
        !width || !height || width>TEXTURE_MAX_DIMENSION || height>TEXTURE_MAX_DIMENSION ||
        totalPixels-textures[id].gpu.width*textures[id].gpu.height+width*height>TEXTURE_TOTAL_PIXELS ||
        (currentTerrain==id && (width!=256 || height!=256))) { Ack(id,revision,UINT32_MAX); return; }
    transfer.data=MemAlloc(size);
    if(!transfer.data) { Ack(id,revision,UINT32_MAX); return; }
    transfer.id=id; transfer.revision=revision; transfer.size=size; transfer.width=width; transfer.height=height;
    Ack(id,revision,0);
}
void ClientTextures_HandleData(void) {
    if(packetDataLength!=TEXTURE_DATA_SIZE) return;
    int id=Texture_Read16(packetData+1);
    uint32_t revision=Texture_Read32(packetData+3), offset=Texture_Read32(packetData+7);
    unsigned count=Texture_Read16(packetData+11);
    if(!transfer.data || id!=transfer.id || revision!=transfer.revision) return;
    if(offset!=(unsigned)transfer.received || !count || count>TEXTURE_CHUNK_BYTES || count>(unsigned)(transfer.size-transfer.received)) {
        Ack(id,revision,UINT32_MAX); ClearTransfer(); return;
    }
    memcpy(transfer.data+offset,packetData+13,count); transfer.received+=count;
    if(transfer.received<transfer.size) { Ack(id,revision,transfer.received); return; }
    int width=0,height=0;
    bool valid=Texture_ValidatePNG(transfer.data,transfer.size,&width,&height) && width==transfer.width && height==transfer.height;
    Image image={0};
    if(valid) image=LoadImageFromMemory(".png",transfer.data,transfer.size);
    valid=valid && image.data && image.width==width && image.height==height && EntityModel_TextureFits(id,width,height);
    Texture2D replacement={0};
    if(valid) replacement=LoadTextureFromImage(image);
    if(replacement.id) {
        SetTextureFilter(replacement,TEXTURE_FILTER_POINT);
        Texture2D old=textures[id].gpu;
        textures[id].gpu=replacement; textures[id].revision=revision;
        totalPixels+=width*height-old.width*old.height;
        EntityModel_RefreshTextures(id);
        if(desiredTerrain==id || currentTerrain==id) ApplyTerrain();
        if(old.id) UnloadTexture(old);
        Ack(id,revision,transfer.size);
    } else {
        TraceLog(LOG_WARNING,"Rejected server texture %i revision %u",id,revision);
        Ack(id,revision,UINT32_MAX);
    }
    if(image.data) UnloadImage(image);
    ClearTransfer();
}
void ClientTextures_HandleTerrain(void) {
    if(packetDataLength!=3) return;
    int id=Texture_Read16(packetData+1);
    if(id<1 || id>=TEXTURE_LIMIT) return;
    desiredTerrain=id; ApplyTerrain();
}
void ClientTextures_Reset(void) {
    ClearTransfer();
    desiredTerrain=1; ApplyTerrain();
    for(int id=2;id<TEXTURE_LIMIT;id++) {
        if(textures[id].gpu.id) UnloadTexture(textures[id].gpu);
        textures[id]=(ClientTexture){0};
    }
    totalPixels=0;
}
