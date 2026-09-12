/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "textures.h"
#include "world.h"
#include "../packet.h"
#include "../networkhandler.h"
#include "../items.h"
#include "../hudbars.h"
#include "textureprotocol.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

typedef struct ServerTexture {
    char name[65];
    unsigned char *data;
    int size, width, height;
    uint32_t revision;
} ServerTexture;
static ServerTexture textures[TEXTURE_LIMIT];
static int terrainId=1, totalBytes, totalPixels;
static double nextSend;
static int breakingId;

int ServerTextures_Find(const char *name) {
    if (!strcmp(name,"humanoid")) return 0;
    if (!strcmp(name,"terrain")) return 1;
    for (int id=2; id<TEXTURE_LIMIT; id++) if (textures[id].data && !strcmp(name,textures[id].name)) return id;
    return -1;
}

bool ServerTextures_ItemSize(int id) {
    return id >= 2 && id < TEXTURE_LIMIT && textures[id].data && textures[id].width <= 64 && textures[id].height <= 64;
}
bool ServerTextures_HudSize(int id) {
    return id >= 2 && id < TEXTURE_LIMIT && textures[id].data && textures[id].width == 9 && textures[id].height == 9;
}
bool ServerTextures_Define(const char *name, const char *path) {
    int id=ServerTextures_Find(name);
    if (id==0 || id==1 || !name[0] || strlen(name)>64) return false;
    if (id<0) for (int i=2;i<TEXTURE_LIMIT;i++) if (!textures[i].data) { id=i; break; }
    if (id<0) return false;
    FILE *file=fopen(path,"rb");
    if (!file) return false;
    if (fseek(file,0,SEEK_END)) { fclose(file); return false; }
    long size=ftell(file);
    if (size<33 || size>TEXTURE_MAX_BYTES || totalBytes-textures[id].size+size>TEXTURE_TOTAL_BYTES) { fclose(file); return false; }
    rewind(file);
    unsigned char *data=MemAlloc(size);
    if (!data) { fclose(file); return false; }
    bool read=fread(data,1,size,file)==(size_t)size;
    fclose(file);
    int width=0,height=0;
    if (!read || !Texture_ValidatePNG(data,size,&width,&height) ||
        totalPixels-textures[id].width*textures[id].height+width*height>TEXTURE_TOTAL_PIXELS ||
        (terrainId==id && (width!=256 || height!=256))) { MemFree(data); return false; }
    if (id==breakingId && (width!=height*10 || height>64)) { MemFree(data); return false; }
    if (ServerHudBars_UsesTexture(id) && (width != 9 || height != 9)) { MemFree(data); return false; }
    // Preserve compatibility with items and models already using this texture.
    if (width > 64 || height > 64) {
        for (int item=256; item<ITEM_LIMIT; item++) {
            if (serverItems[item].defined && serverItems[item].texture==id) { MemFree(data); return false; }
        }
    }
    for (int m=1;m<256;m++) {
        ModelDefinition *d=serverWorld.modelDefinitions[m];
        if (!d || d->texture!=id) continue;
        for (int p=0;p<d->partCount;p++) for (int f=0;f<6;f++) {
            int16_t *uv=d->parts[p].uv[f];
            if (uv[0]>width || uv[0]+uv[2]>width || uv[1]>height || uv[1]+uv[3]>height) { MemFree(data); return false; }
        }
    }
    Image decoded=LoadImageFromMemory(".png",data,size);
    if (!decoded.data || decoded.width!=width || decoded.height!=height) {
        if (decoded.data) UnloadImage(decoded);
        MemFree(data); return false;
    }
    UnloadImage(decoded);
    ServerTexture *t=&textures[id];
    if (t->revision==UINT32_MAX) { MemFree(data); return false; }
    totalBytes+=size-t->size; totalPixels+=width*height-t->width*t->height;
    MemFree(t->data);
    t->data=data; t->size=size; t->width=width; t->height=height; t->revision++;
    strcpy(t->name,name);
    if (serverWorld.entities) for (int i=0; i<WORLD_MAX_ENTITIES; i++) {
        Entity *e = &serverWorld.entities[i];
        if (e->active && !strcmp(e->texture,name)) e->textureDirty = true;
    }
    return true;
}
void ServerTextures_SendTerrain(Player *p) {
    unsigned char *packet=MemAlloc(TERRAIN_TEXTURE_PACKET_SIZE);
    if (!packet) return;
    packet[0]=PACKET_TERRAIN_TEXTURE; Texture_Write16(packet+1,terrainId);
    ServerNetwork_Send(p,packet);
}
bool ServerTextures_SetTerrain(int id) {
    if (id!=1 && (id<2 || id>=TEXTURE_LIMIT || !textures[id].data || textures[id].width!=256 || textures[id].height!=256)) return false;
    terrainId=id;
    if (serverWorld.players) for(int p=0;p<WORLD_MAX_PLAYERS;p++)
        if(serverWorld.players[p] && !serverWorld.players[p]->disconnected) ServerTextures_SendTerrain(serverWorld.players[p]);
    return true;
}
void ServerTextures_Acknowledge(Player *p, int id, uint32_t revision, uint32_t offset) {
    if (!p->textureWaiting || id!=p->textureId || revision!=p->textureRevision) return;
    if (offset==UINT32_MAX) {
        TraceLog(LOG_WARNING,"Player %i rejected texture %i revision %u",p->id,id,revision);
        p->textureSent[id]=revision; p->textureWaiting=false; p->textureId=0; return;
    }
    if (offset!=p->textureOffset) return;
    p->textureWaiting=false;
    if (id>=2 && id<TEXTURE_LIMIT && revision==textures[id].revision && offset==(uint32_t)textures[id].size) {
        p->textureSent[id]=revision; p->textureId=0;
    }
}
void ServerTextures_Update(void) {
    double now=GetTime();
    if (now<nextSend) return;
    nextSend=now+0.016;
    // One acknowledged chunk per client per pass, at most 64 KiB globally.
    static unsigned cursor;
    int budget=15;
    for(int n=0;n<WORLD_MAX_PLAYERS && budget>0;n++) {
        int slot=cursor++%WORLD_MAX_PLAYERS;
        Player *p=serverWorld.players[slot];
        if(!p || p->disconnected) continue;
        if(p->textureWaiting) {
            if(now-p->textureLastSend<15.0) continue;
            p->textureSent[p->textureId]=p->textureRevision;
            p->textureWaiting=false; p->textureId=0;
        }
        if (p->textureId && p->textureRevision!=textures[p->textureId].revision) p->textureId=0;
        if (!p->textureId) {
            int id=0;
            for(int i=2;i<TEXTURE_LIMIT;i++) if(textures[i].data && p->textureSent[i]!=textures[i].revision) { id=i; break; }
            if(!id) continue;
            ServerTexture *t=&textures[id];
            unsigned char *packet=MemAlloc(TEXTURE_BEGIN_SIZE);
            if(!packet) continue;
            packet[0]=PACKET_TEXTURE_BEGIN; Texture_Write16(packet+1,id); Texture_Write32(packet+3,t->revision);
            Texture_Write32(packet+7,t->size); Texture_Write16(packet+11,t->width); Texture_Write16(packet+13,t->height);
            p->textureId=id; p->textureRevision=t->revision; p->textureOffset=0; p->textureWaiting=true;
            p->textureLastSend=now;
            ServerNetwork_Send(p,packet); budget--; continue;
        }
        ServerTexture *t=&textures[p->textureId];
        unsigned count=t->size-p->textureOffset;
        if(count>TEXTURE_CHUNK_BYTES) count=TEXTURE_CHUNK_BYTES;
        unsigned char *packet=MemAlloc(TEXTURE_DATA_SIZE);
        if(!packet) continue;
        memset(packet,0,TEXTURE_DATA_SIZE);
        packet[0]=PACKET_TEXTURE_DATA; Texture_Write16(packet+1,p->textureId); Texture_Write32(packet+3,p->textureRevision);
        Texture_Write32(packet+7,p->textureOffset); Texture_Write16(packet+11,count);
        memcpy(packet+13,t->data+p->textureOffset,count);
        p->textureOffset+=count; p->textureWaiting=true;
        p->textureLastSend=now;
        ServerNetwork_Send(p,packet); budget--;
    }
}
void ServerTextures_Shutdown(void) {
    breakingId=0;
    for(int i=2;i<TEXTURE_LIMIT;i++) { MemFree(textures[i].data); textures[i]=(ServerTexture){0}; }
    totalBytes=totalPixels=0; terrainId=1; nextSend=0;
}

void ServerTextures_SendBreaking(Player *player) {
    unsigned char *packet=MemAlloc(BREAKING_TEXTURE_PACKET_SIZE);
    if (!packet) return;
    packet[0]=PACKET_BREAKING_TEXTURE; packet[1]=breakingId; ServerNetwork_Send(player,packet);
}
bool ServerTextures_SetBreaking(int id) {
    if (id < 2 || id >= TEXTURE_LIMIT || !textures[id].data || textures[id].height <= 0 ||
        textures[id].width != textures[id].height * 10 || textures[id].height > 64)
        return false;
    breakingId = id;
    if (serverWorld.players) {
        for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
            if (serverWorld.players[i])
                ServerTextures_SendBreaking(serverWorld.players[i]);
        }
    }
    return true;
}
