#include <math.h>
#include <string.h>
#define __clang__ 1
#include "stb_ds.h"
#include "lighting.h"
#include "blockstates.h"
#include "world/world.h"
#include "networkhandler.h"
#include "packet.h"
#include "streamprofile.h"

static const Vector3 offsets[6] = {{-1,0,0},{1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
static Chunk *Neighbor(Chunk *c, int face) {
    Vector3 d=offsets[face];
    return ServerWorld_GetChunkAt((Vector3){c->position.x+d.x,c->position.y+d.y,c->position.z+d.z});
}
typedef struct LightQueue { unsigned short indices[CHUNK_SIZE]; bool queued[CHUNK_SIZE]; int head,tail,count; } LightQueue;
typedef struct LightJob {
    Chunk *chunk;
    unsigned int revision;
    unsigned char light[CHUNK_SIZE], flags[CHUNK_SIZE];
    unsigned short properties[BLOCK_RUNTIME_COUNT];
    LightQueue flood;
    int stage, cursor;
    double seconds;
} LightJob;
static LightJob lightJob;
static Chunk *firstDirty[2], *lastDirty[2];
static struct { long int key; Chunk *value; } *dirtyColumns;

static long int ColumnKey(Vector3 position) {
    return ServerChunk_GetPackedPos((Vector3){position.x, 0, position.z});
}

static void AddDirtyColumn(Chunk *chunk) {
    long int key = ColumnKey(chunk->position);
    int index = hmgeti(dirtyColumns, key);
    if (index < 0) {
        hmput(dirtyColumns, key, chunk);
        chunk->lightColumnNext = NULL;
        return;
    }
    // Highest first: sunlight can travel arbitrarily far downward.
    Chunk **link = &dirtyColumns[index].value;
    while (*link && (*link)->position.y > chunk->position.y) link = &(*link)->lightColumnNext;
    chunk->lightColumnNext = *link;
    *link = chunk;
}

static void RemoveDirtyColumn(Chunk *chunk) {
    long int key = ColumnKey(chunk->position);
    int index = hmgeti(dirtyColumns, key);
    if (index < 0) return;
    Chunk **link = &dirtyColumns[index].value;
    while (*link && *link != chunk) link = &(*link)->lightColumnNext;
    if (*link) *link = chunk->lightColumnNext;
    chunk->lightColumnNext = NULL;
    if (!dirtyColumns[index].value) {
        (void)hmdel(dirtyColumns, key);
        if (!hmlen(dirtyColumns)) hmfree(dirtyColumns);
    }
}
typedef struct LightView { Vector3 center; int distance; } LightView;
static LightView lightViews[WORLD_MAX_PLAYERS];
static int lightViewCount;

static bool NearLightView(Chunk *chunk) {
    for (int i = 0; i < lightViewCount; i++) {
        Vector3 center = lightViews[i].center;
        float dx = chunk->position.x - center.x, dz = chunk->position.z - center.z;
        float radius = lightViews[i].distance + 4;
        // Include upper chunks: sunlight may propagate arbitrarily far down.
        if (dx * dx + dz * dz <= radius * radius && chunk->position.y >= center.y - 5)
            return true;
    }
    return false;
}

static void QueueLight(Chunk *chunk) {
    if (!chunk || chunk->loadFailed) return;
    chunk->lightRevision++;
    if (chunk->lightDirty) return;
    AddDirtyColumn(chunk);
    chunk->lightPriority = NearLightView(chunk);
    int queue = chunk->lightPriority;
    chunk->lightDirty = true;
    chunk->lightPrevious = lastDirty[queue];
    chunk->lightNext = NULL;
    if (lastDirty[queue]) lastDirty[queue]->lightNext = chunk;
    else firstDirty[queue] = chunk;
    lastDirty[queue] = chunk;
}

void ServerLighting_Forget(Chunk *chunk) {
    if (!chunk->lightDirty) return;
    RemoveDirtyColumn(chunk);
    if (lightJob.chunk == chunk) lightJob.chunk = NULL;
    int queue = chunk->lightPriority;
    if (chunk->lightPrevious) chunk->lightPrevious->lightNext = chunk->lightNext;
    else firstDirty[queue] = chunk->lightNext;
    if (chunk->lightNext) chunk->lightNext->lightPrevious = chunk->lightPrevious;
    else lastDirty[queue] = chunk->lightPrevious;
    chunk->lightNext = chunk->lightPrevious = NULL;
    chunk->lightDirty = false;
}

bool ServerLighting_IsSettled(void) {
    return !firstDirty[0] && !firstDirty[1];
}

bool ServerLighting_IsReady(Chunk *chunk) {
    if (!chunk || !chunk->lightReady) return false;
    // Light travels at most 15 cells sideways/upward, but sky can travel
    // arbitrarily far downward. Unrelated distant work must not block a view.
    for (int x = -1; x <= 1; x++) {
        for (int z = -1; z <= 1; z++) {
            Vector3 position = {chunk->position.x + x, 0, chunk->position.z + z};
            int index = hmgeti(dirtyColumns, ColumnKey(position));
            if (index >= 0 && dirtyColumns[index].value->position.y >= chunk->position.y - 1) return false;
        }
    }
    return true;
}

void ServerLighting_Prioritize(Chunk *chunk) {
    for (int face = -1; face < 6; face++) {
        Chunk *next = face < 0 ? chunk : Neighbor(chunk, face);
        if (!next || !next->lightDirty || next == firstDirty[1] || next == lightJob.chunk) continue;
        ServerLighting_Forget(next);
        AddDirtyColumn(next);
        next->lightDirty = true;
        next->lightPriority = true;
        next->lightNext = firstDirty[1];
        if (firstDirty[1]) firstDirty[1]->lightPrevious = next;
        else lastDirty[1] = next;
        firstDirty[1] = next;
    }
}

static void RefreshLightViews(void) {
    LightView views[WORLD_MAX_PLAYERS];
    int count = 0;
    for (int i = 0; serverWorld.players && i < WORLD_MAX_PLAYERS; i++) {
        Player *player = serverWorld.players[i];
        if (!player || player->disconnected || player->entityId < 0) continue;
        Vector3 pos = serverWorld.entities[player->entityId].position;
        views[count++] = (LightView){{floorf(pos.x / 16), floorf(pos.y / 16), floorf(pos.z / 16)}, player->drawDistance};
    }
    bool changed = count != lightViewCount;
    for (int i = 0; i < count && !changed; i++) {
        changed = views[i].distance != lightViews[i].distance ||
                  views[i].center.x != lightViews[i].center.x ||
                  views[i].center.y != lightViews[i].center.y ||
                  views[i].center.z != lightViews[i].center.z;
    }
    if (!changed) return;
    memcpy(lightViews, views, count * sizeof(LightView));
    lightViewCount = count;
    // Reclassify only when a view moves, joins, leaves, or changes distance.
    Chunk *pending[2] = {firstDirty[0], firstDirty[1]};
    memset(firstDirty, 0, sizeof(firstDirty));
    memset(lastDirty, 0, sizeof(lastDirty));
    for (int queue = 0; queue < 2; queue++) {
        for (Chunk *chunk = pending[queue]; chunk;) {
            Chunk *next = chunk->lightNext;
            RemoveDirtyColumn(chunk);
            chunk->lightDirty = false;
            QueueLight(chunk);
            chunk = next;
        }
    }
}

void ServerLighting_Changed(Chunk *chunk) {
    if (!chunk) return;
    QueueLight(chunk);
    for (int face = 0; face < 6; face++) QueueLight(Neighbor(chunk, face));
}

void ServerLighting_Removed(Vector3 position) {
    for (int face = 0; face < 6; face++) {
        Vector3 offset = offsets[face];
        QueueLight(ServerWorld_GetChunkAt((Vector3){position.x + offset.x,
            position.y + offset.y, position.z + offset.z}));
    }
}

void ServerLighting_Invalidate(void) {
    for (int i = 0; i < hmlen(serverWorld.chunks); i++) QueueLight(serverWorld.chunks[i].value);
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
static int FaceIndex(int face, int cell) {
    int row = cell / 16, column = cell % 16;
    switch (face) {
        case 0: return row * 256 + column * 16;
        case 1: return row * 256 + column * 16 + 15;
        case 2: return 15 * 256 + cell;
        case 3: return cell;
        case 4: return row * 256 + 15 * 16 + column;
        default: return row * 256 + column;
    }
}

static unsigned short LightProperties(LightJob *job, int id, int state) {
    int key = id >= 0 && id < 256 && state >= 0 && state < BLOCK_MAX_STATES ? state * 256 + id : -1;
    if (key >= 0 && job->properties[key]) return job->properties[key];
    BlockDefinition fallback = {0};
    const BlockDefinition *definition = ServerBlockStates_Definition(id, state);
    if (!definition) {
        BlockShape_Default(id, &fallback);
        fallback.renderType = (id == 0 || (id >= 11 && id <= 15)) ? BLOCK_RENDER_TRANSPARENT :
                              id == 5 ? BLOCK_RENDER_TRANSLUCENT : BLOCK_RENDER_OPAQUE;
        fallback.lightType = (id == 15 || id == 16) ? BLOCK_LIGHT_EMIT : BLOCK_LIGHT_NONE;
        definition = &fallback;
    }
    // Geometry and emission depend on the block variant, not the cell. Cache
    // each used variant once per job; a definition edit restarts the whole job.
    unsigned short properties = Flags(definition) | 512;
    if (definition->lightType == BLOCK_LIGHT_EMIT) properties |= 256;
    if (key >= 0) job->properties[key] = properties;
    return properties;
}

// One cell of work. The committed chunk light is untouched until all stages
// finish, and changes to a dependency restart the private job on the next update.
static bool StepLight(LightJob *job) {
    Chunk *c = job->chunk;
    if (job->stage == 0) {
        int i = job->cursor++;
        unsigned short properties = LightProperties(job, c->data[i], ServerBlockStates_Resolve(c, i));
        job->flags[i] = properties & 255;
        if (properties & 256) job->light[i] = 15;
        int column=i%CHUNK_SIZE_XZ;
        if (i>=CHUNK_SIZE-CHUNK_SIZE_XZ && !Neighbor(c, 2) &&
            (c->skyMask[column>>3]&(1<<(column&7))) && !(job->flags[i]&64))
            job->light[i]|=(job->flags[i]&128 ? 15 : 14)<<4;
        if (job->light[i]) Push(&job->flood,i);

        if (job->cursor == CHUNK_SIZE) { job->stage = 1; job->cursor = 0; }
        return false;
    }
    if (job->stage == 1) {
        int face = job->cursor / CHUNK_SIZE_XZ;
        int cell = job->cursor % CHUNK_SIZE_XZ;
        int i = FaceIndex(face, cell), j = FaceIndex(face ^ 1, cell);
        Chunk *neighbor = Neighbor(c, face);
        if (neighbor && neighbor->lightReady && (neighbor->lightFlags[j] & (1 << (face ^ 1))))
            Offer(job->light, i, neighbor->lightData[j], job->flags[i], face ^ 1, &job->flood);
        if (++job->cursor == 6 * CHUNK_SIZE_XZ) job->stage = 2;
        return false;
    }
    LightQueue *q = &job->flood;
    if (!q->count) return true;
    int i = q->indices[q->head];
    q->head = (q->head + 1) % CHUNK_SIZE;
    q->count--;
    q->queued[i] = false;
    int x = i % 16, y = i / 256, z = (i / 16) % 16;
    for (int face = 0; face < 6; face++) {
        if (!(job->flags[i] & (1 << face))) continue;
        int nx = x + offsets[face].x, ny = y + offsets[face].y, nz = z + offsets[face].z;
        if ((unsigned)nx >= 16 || (unsigned)ny >= 16 || (unsigned)nz >= 16) continue;
        Offer(job->light, ny * 256 + nz * 16 + nx, job->light[i], job->flags[ny * 256 + nz * 16 + nx], face, q);
    }
    return false;
}

static int CommitLight(LightJob *job) {
    Chunk *chunk = job->chunk;
    int changedFaces = 0;
    for (int face = 0; face < 6; face++) {
        for (int cell = 0; cell < CHUNK_SIZE_XZ; cell++) {
            int i = FaceIndex(face, cell);
            int previous = (chunk->lightFlags[i] & (1 << face)) ? chunk->lightData[i] : 0;
            int current = (job->flags[i] & (1 << face)) ? job->light[i] : 0;
            if (previous != current) { changedFaces |= 1 << face; break; }
        }
    }
    memcpy(chunk->lightData, job->light, sizeof(job->light));
    memcpy(chunk->lightFlags, job->flags, sizeof(job->flags));
    chunk->lightReady = true;
    return changedFaces;
}
void ServerLighting_Send(Chunk *c,Player *player) {
    // Sending never performs lighting work. The streaming queue waits for it.
    if (!ServerLighting_IsReady(c)) return;
    unsigned short light[CHUNK_SIZE];
    for (int i=0; i<CHUNK_SIZE; i++) light[i]=c->lightData[i];
    int length=0;
    unsigned short *compressed=ChunkData_CreateCompressed(light,&length);
    if (!compressed) return;
    int packetLength=CHUNK_LIGHT_HEADER_SIZE+length*2;
    unsigned char *packet=MemAlloc(packetLength);
    if (!packet) { MemFree(compressed); return; }
    packet[0]=PACKET_CHUNK_LIGHT;
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
    static StreamProfile profile;
    RefreshLightViews();
    double deadline = GetTime() + 0.002;
    int completed = 0;
    // The cell limit bounds work even without a clock. Time is only a fallback,
    // checked between small batches, rather than after a whole chunk rebuild.
    for (int cells = 0; cells < 65536 && !ServerLighting_IsSettled() && completed < 8;) {
        if (lightJob.chunk && !lightJob.chunk->lightPriority && firstDirty[1]) lightJob.chunk = NULL;
        Chunk *chunk = lightJob.chunk ? lightJob.chunk : (firstDirty[1] ? firstDirty[1] : firstDirty[0]);
        if (!lightJob.chunk || lightJob.revision != chunk->lightRevision) {
            // Resolve the vertical sky input first. Otherwise a newly arrived,
            // unlit upper chunk makes us publish darkness, then rebuild the
            // whole column again when its sky light finally arrives.
            Chunk *above = Neighbor(chunk, 2);
            while (above && above->lightDirty) {
                chunk = above;
                above = Neighbor(chunk, 2);
            }
        }
        if (lightJob.chunk != chunk || lightJob.revision != chunk->lightRevision) {
            memset(&lightJob, 0, sizeof(lightJob));
            lightJob.chunk = chunk;
            lightJob.revision = chunk->lightRevision;
        }
        double start = GetTime();
        bool done = false;
        for (int batch = 0; batch < 256 && !done; batch++, cells++) done = StepLight(&lightJob);
        lightJob.seconds += GetTime() - start;
        if (done) {
            int changedFaces = CommitLight(&lightJob);
            StreamProfile_Add(&profile, "server lighting", lightJob.seconds);
            ServerLighting_Forget(chunk);
            for (int face = 0; face < 6; face++) {
                if (changedFaces & (1 << face)) QueueLight(Neighbor(chunk, face));
            }
            completed++;
        }
        if (GetTime() >= deadline) break;
    }
}
bool ServerLighting_Get(Vector3 pos,int *block,int *sky,int *level) {
    if (!isfinite(pos.x) || !isfinite(pos.y) || !isfinite(pos.z)) return false;
    Chunk *c=ServerWorld_GetChunkAt((Vector3){floorf(pos.x/16),floorf(pos.y/16),floorf(pos.z/16)});
    if (!c || !c->lightReady) return false;
    // Never expose intermediate relaxation values to spawning or other gameplay.
    if (!ServerLighting_IsReady(c)) return false;
    int index=ServerChunk_PosToIndex((Vector3){floorf(pos.x)-c->blockPosition.x,floorf(pos.y)-c->blockPosition.y,floorf(pos.z)-c->blockPosition.z});
    *block=c->lightData[index]&15; *sky=c->lightData[index]>>4;
    *level=(int)ceilf(fmaxf(*block,*sky*WorldTime_Sunlight(serverWorld.time)));
    return true;
}
