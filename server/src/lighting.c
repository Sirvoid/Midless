#include <math.h>
#include <string.h>
#define __clang__ 1
#include "stb_ds.h"
#include "lighting.h"
#include "blockstates.h"
#include "world/world.h"
#include "networkhandler.h"
#include "packet.h"

static const Vector3 offsets[6] = {{-1,0,0},{1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
static Chunk *Neighbor(Chunk *c, int face) {
    Vector3 d=offsets[face];
    return ServerWorld_GetChunkAt((Vector3){c->position.x+d.x,c->position.y+d.y,c->position.z+d.z});
}
void ServerLighting_Changed(Chunk *c) {
    // Main thread only: generated chunks are marked after publication.
    if (!c || c->loadFailed) return;
    c->lightDirty=true;
    for (int f=0; f<6; f++) { Chunk *n=Neighbor(c,f); if (n) n->lightDirty=true; }
}
void ServerLighting_Removed(Vector3 position) {
    for (int f=0; f<6; f++) {
        Vector3 d=offsets[f];
        Chunk *n=ServerWorld_GetChunkAt((Vector3){position.x+d.x,position.y+d.y,position.z+d.z});
        if (n) n->lightDirty=true;
    }
}
void ServerLighting_Invalidate(void) {
    for (int i=0; i<hmlen(serverWorld.chunks); i++) serverWorld.chunks[i].value->lightDirty=true;
}

static unsigned char Flags(const BlockDefinition *d) {
    BlockBox bounds;
    memcpy(bounds.min,d->min,3); memcpy(bounds.max,d->max,3);
    if (d->geometry.enabled && d->geometry.boxCount) {
        bounds=BlockBox_Rotate(d->geometry.boxes[0].bounds,d->geometry.rotation);
        for (int i=1; i<d->geometry.boxCount; i++) {
            BlockBox box=BlockBox_Rotate(d->geometry.boxes[i].bounds,d->geometry.rotation);
            for (int axis=0; axis<3; axis++) {
                if (box.min[axis]<bounds.min[axis]) bounds.min[axis]=box.min[axis];
                if (box.max[axis]>bounds.max[axis]) bounds.max[axis]=box.max[axis];
            }
        }
    }
    bool x=bounds.min[0]==0 && bounds.max[0]==16, y=bounds.min[1]==0 && bounds.max[1]==16, z=bounds.min[2]==0 && bounds.max[2]==16;
    bool full=x && y && z && (!d->geometry.enabled || d->geometry.boxCount==1);
    int flags=d->renderType==BLOCK_RENDER_TRANSPARENT ? 128 : 0;
    if (d->renderType==BLOCK_RENDER_OPAQUE && full) flags|=64;
    if (d->lightType==BLOCK_LIGHT_EMIT || d->renderType!=BLOCK_RENDER_OPAQUE || (d->geometry.enabled && !full)) return flags|63;
    if (!(y && z)) flags|=3;
    if (!(x && z)) flags|=12;
    if (!(x && y)) flags|=48;
    return flags;
}
typedef struct LightQueue { unsigned short indices[CHUNK_SIZE]; bool queued[CHUNK_SIZE]; int head,tail,count; } LightQueue;
static void Push(LightQueue *q,int i) {
    if (q->queued[i]) return;
    q->queued[i]=true; q->indices[q->tail]=i; q->tail=(q->tail+1)%CHUNK_SIZE; q->count++;
}
static void Offer(unsigned char *light,int i,int value,int flags,int face,LightQueue *q) {
    if (flags&64) return;
    int block=(value&15)-1, sky=(value>>4)-((face==3 && (flags&128)) ? 0 : 1);
    int old=light[i],next=old;
    if (block>(old&15)) next=(next&240)|block;
    if (sky>(old>>4)) next=(next&15)|(sky<<4);
    if (next!=old) { light[i]=next; Push(q,i); }
}
static bool Rebuild(Chunk *c) {
    unsigned char light[CHUNK_SIZE]={0};
    LightQueue q={0};
    bool flagsChanged=false;
    Chunk *neighbors[6]; for (int f=0; f<6; f++) neighbors[f]=Neighbor(c,f);
    for (int i=0; i<CHUNK_SIZE; i++) {
        BlockDefinition fallback={0};
        const BlockDefinition *d=ServerBlockStates_Definition(c->data[i],ServerBlockStates_Resolve(c,i));
        if (!d) {
            int id=c->data[i];
            BlockShape_Default(id,&fallback);
            fallback.renderType=(id==0 || (id>=11 && id<=15)) ? BLOCK_RENDER_TRANSPARENT :
                                id==5 ? BLOCK_RENDER_TRANSLUCENT : BLOCK_RENDER_OPAQUE;
            fallback.lightType=(id==15 || id==16) ? BLOCK_LIGHT_EMIT : BLOCK_LIGHT_NONE;
            d=&fallback;
        }
        unsigned char flags=Flags(d);
        if (c->lightFlags[i]!=flags) flagsChanged=true;
        c->lightFlags[i]=flags;
        if (d->lightType==BLOCK_LIGHT_EMIT) light[i]=15;
        int column=i%CHUNK_SIZE_XZ;
        if (i>=CHUNK_SIZE-CHUNK_SIZE_XZ && !neighbors[2] &&
            (c->skyMask[column>>3]&(1<<(column&7))) && !(c->lightFlags[i]&64))
            light[i]|=(c->lightFlags[i]&128 ? 15 : 14)<<4;
        if (light[i]) Push(&q,i);
    }
    // Start from intrinsic sources and boundary inputs, then flood within this
    // chunk. Neighbor rebuilds converge for both additions and removals.
    for (int i=0; i<CHUNK_SIZE; i++) {
        int x=i%16,y=i/256,z=(i/16)%16;
        for (int f=0; f<6; f++) {
            int nx=x+offsets[f].x,ny=y+offsets[f].y,nz=z+offsets[f].z;
            if ((unsigned)nx<16 && (unsigned)ny<16 && (unsigned)nz<16) continue;
            Chunk *n=neighbors[f]; if (!n || !n->lightReady) continue;
            int j=((ny+16)%16)*256+((nz+16)%16)*16+(nx+16)%16;
            if (n->lightFlags[j]&(1<<(f^1))) Offer(light,i,n->lightData[j],c->lightFlags[i],f^1,&q);
        }
    }
    while (q.count) {
        int i=q.indices[q.head]; q.head=(q.head+1)%CHUNK_SIZE; q.count--; q.queued[i]=false;
        int x=i%16,y=i/256,z=(i/16)%16;
        for (int f=0; f<6; f++) {
            if (!(c->lightFlags[i]&(1<<f))) continue;
            int nx=x+offsets[f].x,ny=y+offsets[f].y,nz=z+offsets[f].z;
            if ((unsigned)nx>=16 || (unsigned)ny>=16 || (unsigned)nz>=16) continue;
            int j=ny*256+nz*16+nx;
            Offer(light,j,light[i],c->lightFlags[j],f,&q);
        }
    }
    bool changed=!c->lightReady || flagsChanged || memcmp(light,c->lightData,sizeof(light));
    memcpy(c->lightData,light,sizeof(light)); c->lightReady=true; c->lightDirty=false;
    return changed;
}
void ServerLighting_Send(Chunk *c,Player *player) {
    // Full chunk snapshots need settled light, including loaded neighbors.
    // Subsequent edits are propagated locally by the client.
    bool pending;
    do {
        pending=false;
        for (int i=0; i<hmlen(serverWorld.chunks); i++) {
            Chunk *chunk=serverWorld.chunks[i].value;
            if (chunk->lightDirty && !chunk->loadFailed) { pending=true; break; }
        }
        if (pending) ServerLighting_Update();
    } while (pending);
    if (!c->lightReady) return;
    unsigned short light[CHUNK_SIZE];
    for (int i=0; i<CHUNK_SIZE; i++) light[i]=c->lightData[i];
    int length=0;
    unsigned short *compressed=ChunkData_CreateCompressed(light,&length);
    if (!compressed) return;
    int packetLength=CHUNK_LIGHT_HEADER_SIZE+length*2;
    unsigned char *packet=MemAlloc(packetLength);
    if (!packet) { MemFree(compressed); return; }
    packet[0]=34;
    int positions[3]={(int)c->position.x,(int)c->position.y,(int)c->position.z};
    for (int axis=0; axis<3; axis++) for (int byte=0; byte<4; byte++)
        packet[1+axis*4+byte]=(uint32_t)positions[axis]>>(24-byte*8);
    packet[13]=length>>8;
    packet[14]=length&255;
    memcpy(packet+CHUNK_LIGHT_HEADER_SIZE,compressed,length*2);
    MemFree(compressed);
    serverPacketLastDynamicLength=packetLength;
    ServerNetwork_Send(player,packet);
}
void ServerLighting_Update(void) {
    static unsigned cursor;
    int count=hmlen(serverWorld.chunks),budget=4;
    for (int checked=0; checked<count && budget; checked++) {
        cursor%=count;
        Chunk *c=serverWorld.chunks[cursor++].value;
        if (!c->lightDirty || c->loadFailed) continue;
        budget--;
        if (!Rebuild(c)) continue;
        for (int f=0; f<6; f++) { Chunk *n=Neighbor(c,f); if (n) n->lightDirty=true; }
    }
}
bool ServerLighting_Get(Vector3 pos,int *block,int *sky,int *level) {
    if (!isfinite(pos.x) || !isfinite(pos.y) || !isfinite(pos.z)) return false;
    Chunk *c=ServerWorld_GetChunkAt((Vector3){floorf(pos.x/16),floorf(pos.y/16),floorf(pos.z/16)});
    if (!c || !c->lightReady) return false;
    // Never expose intermediate relaxation values to spawning or other gameplay.
    for (int i=0; i<hmlen(serverWorld.chunks); i++) if (serverWorld.chunks[i].value->lightDirty) return false;
    int index=ServerChunk_PosToIndex((Vector3){floorf(pos.x)-c->blockPosition.x,floorf(pos.y)-c->blockPosition.y,floorf(pos.z)-c->blockPosition.z});
    *block=c->lightData[index]&15; *sky=c->lightData[index]>>4;
    *level=(int)ceilf(fmaxf(*block,*sky*WorldTime_Sunlight(serverWorld.time)));
    return true;
}
