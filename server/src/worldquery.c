#include <math.h>
#include <float.h>
#include "worldquery.h"
#include "world/world.h"
#include "blockstates.h"
#include "entityphysics.h"

static bool PositionValid(Vector3 p) {
    return isfinite(p.x) && isfinite(p.y) && isfinite(p.z) &&
        fabsf(p.x) <= 999900 && fabsf(p.y) <= 999900 && fabsf(p.z) <= 999900;
}
bool ServerQuery_Block(Vector3 cell, BlockShape *shape) {
    if (!PositionValid(cell)) return false;
    Chunk *chunk = ServerWorld_GetChunkAt((Vector3){floorf(cell.x / 16), floorf(cell.y / 16), floorf(cell.z / 16)});
    if (!chunk || chunk->loadFailed) return false;
    *shape = ServerBlockStates_Shape(ServerWorld_GetBlock(cell), cell);
    return true;
}
static bool Overlaps(BoundingBox a, BoundingBox b) {
    return a.min.x < b.max.x && a.max.x > b.min.x && a.min.y < b.max.y &&
        a.max.y > b.min.y && a.min.z < b.max.z && a.max.z > b.min.z;
}
bool ServerQuery_Clear(EntityBody body, Vector3 position) {
    if (!PositionValid(position) || !EntityBody_Validate(&body)) return false;
    BoundingBox bounds = EntityBody_Bounds(&body, position);
    for (int x = floorf(bounds.min.x); x < ceilf(bounds.max.x); x++)
    for (int y = floorf(bounds.min.y); y < ceilf(bounds.max.y); y++)
    for (int z = floorf(bounds.min.z); z < ceilf(bounds.max.z); z++) {
        BlockShape shape;
        if (!ServerQuery_Block((Vector3){x,y,z}, &shape) || shape.liquid) return false;
        if (shape.solid) for (int i = 0; i < shape.collisionCount; i++)
            if (Overlaps(bounds, shape.collision[i])) return false;
    }
    return true;
}
static bool RayBox(Vector3 from, Vector3 direction, BoundingBox box, float *distance, Vector3 *normal) {
    float near = 0, far = *distance;
    float origin[] = {from.x,from.y,from.z}, delta[] = {direction.x,direction.y,direction.z};
    float low[] = {box.min.x,box.min.y,box.min.z}, high[] = {box.max.x,box.max.y,box.max.z};
    Vector3 face = {0};
    for (int axis = 0; axis < 3; axis++) {
        if (fabsf(delta[axis]) < 0.000001f) {
            if (origin[axis] < low[axis] || origin[axis] > high[axis]) return false;
            continue;
        }
        float a = (low[axis]-origin[axis])/delta[axis], b = (high[axis]-origin[axis])/delta[axis];
        if (a > b) { float swap = a; a = b; b = swap; }
        if (a > near) {
            near = a; face = (Vector3){0};
            float sign = delta[axis] > 0 ? -1 : 1;
            if (axis == 0) face.x = sign;
            if (axis == 1) face.y = sign;
            if (axis == 2) face.z = sign;
        }
        far = fminf(far, b);
        if (near > far) return false;
    }
    *distance = near; *normal = face;
    return true;
}
WorldHit ServerQuery_Raycast(Vector3 from, Vector3 to, bool entities, int ignoreId) {
    WorldHit hit = {.type = HIT_NOTHING, .position = to, .entityId = -1};
    Vector3 direction = {to.x-from.x,to.y-from.y,to.z-from.z};
    float length = sqrtf(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);
    if (!PositionValid(from) || !PositionValid(to) || !isfinite(length) || length > 128) {
        hit.type = HIT_UNLOADED; hit.position = from; return hit;
    }
    hit.distance = length;
    if (length <= 0.000001f) return hit;
    direction.x /= length; direction.y /= length; direction.z /= length;
    int cell[] = {floorf(from.x),floorf(from.y),floorf(from.z)}, step[3];
    float origin[] = {from.x,from.y,from.z}, delta[] = {direction.x,direction.y,direction.z};
    float next[3], stride[3];
    for (int i = 0; i < 3; i++) {
        step[i] = delta[i] >= 0 ? 1 : -1;
        stride[i] = delta[i] == 0 ? FLT_MAX : fabsf(1/delta[i]);
        next[i] = delta[i] == 0 ? FLT_MAX : (cell[i]+(step[i]>0)-origin[i])/delta[i];
    }
    float entered = 0;
    for (int visits = 0; visits < 400 && entered <= hit.distance; visits++) {
        Vector3 block = {cell[0],cell[1],cell[2]};
        BlockShape shape;
        if (!ServerQuery_Block(block, &shape)) {
            hit.type = HIT_UNLOADED; hit.distance = entered; hit.block = block; break;
        }
        if (shape.solid) for (int i = 0; i < shape.collisionCount; i++) {
            float distance = hit.distance; Vector3 normal;
            if (RayBox(from, direction, shape.collision[i], &distance, &normal)) {
                hit.type = HIT_BLOCK; hit.distance = distance; hit.block = block; hit.normal = normal;
            }
        }
        int axis = next[0] <= next[1] ? 0 : 1;
        if (next[2] < next[axis]) axis = 2;
        entered = next[axis]; next[axis] += stride[axis]; cell[axis] += step[axis];
    }
    if (entities) {
        BoundingBox area = {{fminf(from.x,to.x),fminf(from.y,to.y),fminf(from.z,to.z)},
            {fmaxf(from.x,to.x),fmaxf(from.y,to.y),fmaxf(from.z,to.z)}};
        int ids[WORLD_MAX_ENTITIES], count = ServerPhysics_QueryEntities(area, ids, WORLD_MAX_ENTITIES);
        for (int i = 0; i < count; i++) {
            Entity *e = &serverWorld.entities[ids[i]];
            if (e->id == ignoreId || (e->definitionId < 0 && e->ownerPlayerId < 0)) continue;
            float distance = hit.distance; Vector3 normal;
            if (RayBox(from, direction, EntityBody_Bounds(&e->body,e->position), &distance, &normal) &&
                (distance < hit.distance || hit.type == HIT_NOTHING)) {
                hit.type = HIT_ENTITY; hit.distance = distance; hit.normal = normal; hit.entityId = e->id;
            }
        }
    }
    hit.position = (Vector3){from.x+direction.x*hit.distance,from.y+direction.y*hit.distance,from.z+direction.z*hit.distance};
    return hit;
}

// Ground A*: eight neighbors, at most one block up/down, and 512 visited nodes.
// Keep the bounded search local; Lua decides when to replan a moving target.
#define PATH_NODES 512
typedef struct PathNode { Vector3 position; float cost, score; int parent; bool closed; } PathNode;
static bool Ground(EntityBody body, Vector3 near, Vector3 *result) {
    float feet = near.y + body.localBounds.min.y;
    for (int y = floorf(feet+1); y >= floorf(feet-2); y--) {
        Vector3 cell = {floorf(near.x),y,floorf(near.z)};
        BlockShape shape;
        if (!ServerQuery_Block(cell,&shape) || !shape.solid || shape.liquid) continue;
        for (int i = 0; i < shape.collisionCount; i++) {
            BoundingBox support = shape.collision[i];
            Vector3 p = {cell.x+0.5f,support.max.y-body.localBounds.min.y,cell.z+0.5f};
            BoundingBox bounds = EntityBody_Bounds(&body,p);
            if (fabsf(p.y-near.y) > 1.01f || bounds.min.x < support.min.x || bounds.max.x > support.max.x ||
                bounds.min.z < support.min.z || bounds.max.z > support.max.z) continue;
            if (ServerQuery_Clear(body,p)) { *result = p; return true; }
        }
    }
    return false;
}
static bool SegmentClear(EntityBody body, Vector3 a, Vector3 b) {
    // Exact for axis-aligned travel; diagonals use a conservative bounding prism.
    BoundingBox first = EntityBody_Bounds(&body,a), last = EntityBody_Bounds(&body,b);
    EntityBody sweep = body;
    Vector3 center = {(a.x+b.x)/2,(a.y+b.y)/2,(a.z+b.z)/2};
    sweep.localBounds.min = (Vector3){fminf(first.min.x,last.min.x)-center.x,fminf(first.min.y,last.min.y)-center.y,fminf(first.min.z,last.min.z)-center.z};
    sweep.localBounds.max = (Vector3){fmaxf(first.max.x,last.max.x)-center.x,fmaxf(first.max.y,last.max.y)-center.y,fmaxf(first.max.z,last.max.z)-center.z};
    return ServerQuery_Clear(sweep,center);
}
static bool Transition(EntityBody body, Vector3 a, Vector3 b) {
    Vector3 up = a, across = b;
    up.y = across.y = fmaxf(a.y,b.y);
    // Leave headroom for the jump needed to reach an elevated neighbor.
    if (b.y > a.y+0.05f) up.y = across.y = b.y+0.25f;
    return SegmentClear(body,a,up) && SegmentClear(body,up,across) && SegmentClear(body,across,b);
}
// Flat shortcuts only. Check short swept prisms, including every piece of floor
// beneath them, so diagonal travel cannot clip corners or cross a narrow gap.
bool ServerQuery_CanWalk(EntityBody body, Vector3 from, Vector3 to) {
    if (!PositionValid(from) || !PositionValid(to) || !body.enabled ||
        !EntityBody_Validate(&body) || fabsf(to.y-from.y) > 0.001f) return false;
    float length = hypotf(to.x-from.x,to.z-from.z);
    if (length > 32) return false;
    int steps = (int)ceilf(length/0.25f);
    if (steps < 1) steps = 1;
    Vector3 previous = from;
    for (int step = 1; step <= steps; step++) {
        float t = (float)step/steps;
        Vector3 next = {from.x+(to.x-from.x)*t,from.y,from.z+(to.z-from.z)*t};
        if (!SegmentClear(body,previous,next)) return false;
        BoundingBox a = EntityBody_Bounds(&body,previous), b = EntityBody_Bounds(&body,next);
        float minX = fminf(a.min.x,b.min.x), maxX = fmaxf(a.max.x,b.max.x);
        float minZ = fminf(a.min.z,b.min.z), maxZ = fmaxf(a.max.z,b.max.z);
        int y = (int)floorf(a.min.y-0.001f);
        for (int x = floorf(minX); x < ceilf(maxX); x++)
        for (int z = floorf(minZ); z < ceilf(maxZ); z++) {
            BlockShape shape;
            if (!ServerQuery_Block((Vector3){x,y,z},&shape) || !shape.solid || shape.liquid) return false;
            bool supported = false;
            for (int i = 0; i < shape.collisionCount; i++) {
                BoundingBox floor = shape.collision[i];
                if (fabsf(floor.max.y-a.min.y) <= 0.001f &&
                    floor.min.x <= fmaxf(minX,x) && floor.max.x >= fminf(maxX,x+1) &&
                    floor.min.z <= fmaxf(minZ,z) && floor.max.z >= fminf(maxZ,z+1)) supported = true;
            }
            if (!supported) return false;
        }
        previous = next;
    }
    return true;
}
static float PathEstimate(Vector3 from, Vector3 to) {
    float x = fabsf(from.x-to.x), z = fabsf(from.z-to.z);
    return fmaxf(x,z) + (sqrtf(2)-1)*fminf(x,z);
}
static bool DiagonalClear(EntityBody body, Vector3 from, Vector3 to) {
    Vector3 sideX, sideZ;
    // Both adjacent ground cells must be traversable: no squeezing between
    // touching corners or taking shortcuts diagonally across unsupported gaps.
    return Ground(body,(Vector3){to.x,from.y,from.z},&sideX) &&
        Ground(body,(Vector3){from.x,from.y,to.z},&sideZ) &&
        fabsf(sideX.y-to.y) <= 1.01f && fabsf(sideZ.y-to.y) <= 1.01f &&
        Transition(body,from,sideX) && Transition(body,sideX,to) &&
        Transition(body,from,sideZ) && Transition(body,sideZ,to);
}
int ServerQuery_FindPath(EntityBody body, Vector3 from, Vector3 to, Vector3 *path, int capacity) {
    if (!path || capacity < 1 || !PositionValid(from) || !PositionValid(to) ||
        !body.enabled || !EntityBody_Validate(&body) || fabsf(from.x-to.x)>32 || fabsf(from.z-to.z)>32) return 0;
    Vector3 start, goal;
    if (!Ground(body,from,&start) || !Ground(body,to,&goal)) return 0;
    PathNode nodes[PATH_NODES] = {{.position=start,.parent=-1}};
    int count = 1;
    for (int expanded = 0; expanded < PATH_NODES; expanded++) {
        int best = -1;
        for (int i = 0; i < count; i++) if (!nodes[i].closed && (best < 0 || nodes[i].score < nodes[best].score)) best = i;
        if (best < 0) return 0;
        PathNode *current = &nodes[best];
        if (fabsf(current->position.x-goal.x)<0.1f && fabsf(current->position.z-goal.z)<0.1f && fabsf(current->position.y-goal.y)<0.1f) {
            int reversed[PATH_NODES], length = 0;
            for (int i = best; i >= 0; i = nodes[i].parent) reversed[length++] = i;
            if (length > capacity) return 0;
            for (int i = 0; i < length; i++) path[i] = nodes[reversed[length-i-1]].position;
            return length;
        }
        current->closed = true;
        const int offsets[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
        for (int side = 0; side < 8; side++) {
            Vector3 candidate = current->position, p;
            candidate.x += offsets[side][0]; candidate.z += offsets[side][1];
            if (fabsf(candidate.x-start.x)>32 || fabsf(candidate.z-start.z)>32 || !Ground(body,candidate,&p) || !Transition(body,current->position,p)) continue;
            bool diagonal = offsets[side][0] != 0 && offsets[side][1] != 0;
            if (diagonal && !DiagonalClear(body,current->position,p)) continue;
            int index = -1;
            for (int i = 0; i < count; i++) if (nodes[i].position.x == p.x && nodes[i].position.y == p.y && nodes[i].position.z == p.z) { index = i; break; }
            float cost = current->cost+(diagonal ? sqrtf(2) : 1)+fabsf(p.y-current->position.y);
            if (index >= 0 && (nodes[index].closed || cost >= nodes[index].cost)) continue;
            if (index < 0) { if (count == PATH_NODES) continue; index = count++; }
            nodes[index] = (PathNode){.position=p,.cost=cost,.score=cost+PathEstimate(p,goal),.parent=best};
        }
    }
    return 0;
}
