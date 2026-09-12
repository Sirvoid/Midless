#include <math.h>
#include <limits.h>
#include "playerimpulse.h"
#include "player.h"
#include "packet.h"
#include "networkhandler.h"
#include "blockstates.h"
#include "world/world.h"
#include "scripthooks.h"

// Slightly above the client's normal limits, with a bounded allowance for jitter.
#define PLAYER_HORIZONTAL_SPEED 10.0f
#define PLAYER_UP_SPEED 16.0f
#define PLAYER_DOWN_SPEED 75.0f
#define PLAYER_HORIZONTAL_ALLOWANCE 2.0f
#define PLAYER_UP_ALLOWANCE 3.0f
#define PLAYER_DOWN_ALLOWANCE 12.0f
#define PLAYER_HALF_WIDTH 0.3f
#define PLAYER_HEIGHT 1.5f
#define POSITION_TOLERANCE (1.0f / 32.0f)
#define COLLISION_TOLERANCE 0.125f // Allow slight clipping into solid blocks.
#define GROUND_TOLERANCE 0.15f

void ServerPlayer_ResetMovement(Player *player) {
    player->impulseAllowance = (Vector3){0};
    player->impulseExpires = 0;
    player->movementTime = GetTime();
    player->horizontalAllowance = PLAYER_HORIZONTAL_ALLOWANCE;
    player->upAllowance = PLAYER_UP_ALLOWANCE;
    player->downAllowance = PLAYER_DOWN_ALLOWANCE;
    player->fallPeak = serverWorld.entities[player->entityId].position.y;
    player->falling = false;
    player->movementReady = true;
}

bool ServerPlayer_ApplyImpulse(Player *player, Vector3 impulse) {
    if (!player || player->disconnected || !player->movementReady || !PlayerImpulse_Valid(impulse) ||
        !serverWorld.entities || player->entityId < 0 || player->entityId >= WORLD_MAX_ENTITIES ||
        !serverWorld.entities[player->entityId].active) return false;
    if (impulse.x == 0 && impulse.y == 0 && impulse.z == 0) return true;
    unsigned char *packet = ServerPacket_CreatePlayerImpulse(impulse);
    if (!packet) return false;
    double now = GetTime();
    if (now >= player->impulseExpires) player->impulseAllowance = (Vector3){0};
    // Bounded, expiring distance credit for movement caused by the server.
    // Ordinary movement limits and collision checks remain in force.
    player->impulseAllowance.x = fminf(90, player->impulseAllowance.x + hypotf(impulse.x, impulse.z) * PLAYER_IMPULSE_WINDOW);
    player->impulseAllowance.y = fminf(90, player->impulseAllowance.y + fmaxf(0, impulse.y) * PLAYER_IMPULSE_WINDOW);
    player->impulseAllowance.z = fminf(90, player->impulseAllowance.z + fmaxf(0, -impulse.y) * PLAYER_IMPULSE_WINDOW);
    player->impulseExpires = now + PLAYER_IMPULSE_WINDOW;
    ServerNetwork_Send(player, packet);
    return true;
}

static bool GetShape(int x, int y, int z, BlockShape *shape) {
    Vector3 cell = {x, y, z};
    Vector3 chunk = {floorf(x / (float)CHUNK_SIZE_X), floorf(y / (float)CHUNK_SIZE_Y), floorf(z / (float)CHUNK_SIZE_Z)};
    if (!ServerWorld_GetChunkAt(chunk)) return false;
    *shape = ServerBlockStates_Shape(ServerWorld_GetBlock(cell), cell);
    return true;
}

// Sweep the player's feet position against a box expanded by the player's size.
static bool CrossesBox(Vector3 from, Vector3 to, BoundingBox box, float tolerance) {
    float start[] = {from.x, from.y, from.z};
    float delta[] = {to.x - from.x, to.y - from.y, to.z - from.z};
    float min[] = {box.min.x - PLAYER_HALF_WIDTH + tolerance,
        box.min.y - PLAYER_HEIGHT + tolerance, box.min.z - PLAYER_HALF_WIDTH + tolerance};
    float max[] = {box.max.x + PLAYER_HALF_WIDTH - tolerance,
        box.max.y - tolerance, box.max.z + PLAYER_HALF_WIDTH - tolerance};
    float enter = 0, leave = 1;
    for (int axis = 0; axis < 3; axis++) {
        if (fabsf(delta[axis]) < 0.000001f) {
            if (start[axis] <= min[axis] || start[axis] >= max[axis]) return false;
        } else {
            float a = (min[axis] - start[axis]) / delta[axis];
            float b = (max[axis] - start[axis]) / delta[axis];
            enter = fmaxf(enter, fminf(a, b));
            leave = fminf(leave, fmaxf(a, b));
            if (enter >= leave) return false;
        }
    }
    return enter < leave;
}

static bool ClearPath(Vector3 from, Vector3 to, bool *liquid) {
    int minX = (int)floorf(fminf(from.x, to.x) - PLAYER_HALF_WIDTH);
    int maxX = (int)floorf(fmaxf(from.x, to.x) + PLAYER_HALF_WIDTH);
    int minY = (int)floorf(fminf(from.y, to.y));
    int maxY = (int)floorf(fmaxf(from.y, to.y) + PLAYER_HEIGHT);
    int minZ = (int)floorf(fminf(from.z, to.z) - PLAYER_HALF_WIDTH);
    int maxZ = (int)floorf(fmaxf(from.z, to.z) + PLAYER_HALF_WIDTH);
    for (int x = minX; x <= maxX; x++)
    for (int y = minY; y <= maxY; y++)
    for (int z = minZ; z <= maxZ; z++) {
        BlockShape shape;
        if (!GetShape(x, y, z, &shape)) return false;
        if (shape.liquid && CrossesBox(from, to, shape.bounds, POSITION_TOLERANCE)) *liquid = true;
        if (!shape.solid) continue;
        for (int i = 0; i < shape.collisionCount; i++)
            if (CrossesBox(from, to, shape.collision[i], COLLISION_TOLERANCE)) return false;
    }
    return true;
}

static bool ClearAxisPath(Vector3 from, Vector3 to, bool *liquid) {
    // Several client frames fit between packets, so either axis may clear a corner first.
    static const int orders[6][3] = {{0,1,2}, {0,2,1}, {1,0,2}, {1,2,0}, {2,0,1}, {2,1,0}};
    for (int i = 0; i < 6; i++) {
        Vector3 start = from;
        bool clear = true, touchesLiquid = false;
        for (int j = 0; j < 3; j++) {
            Vector3 end = start;
            switch (orders[i][j]) {
                case 0: end.x = to.x; break;
                case 1: end.y = to.y; break;
                case 2: end.z = to.z; break;
            }
            if (!ClearPath(start, end, &touchesLiquid)) { clear = false; break; }
            start = end;
        }
        if (clear) { *liquid = touchesLiquid; return true; }
    }
    return false;
}

static bool FindGround(Vector3 position, float *height) {
    bool found = false;
    float halfWidth = PLAYER_HALF_WIDTH + POSITION_TOLERANCE;
    for (int x = (int)floorf(position.x - halfWidth); x <= (int)floorf(position.x + halfWidth); x++)
    for (int y = (int)floorf(position.y - GROUND_TOLERANCE); y <= (int)floorf(position.y + POSITION_TOLERANCE); y++)
    for (int z = (int)floorf(position.z - halfWidth); z <= (int)floorf(position.z + halfWidth); z++) {
        BlockShape shape;
        if (!GetShape(x, y, z, &shape) || !shape.solid) continue;
        for (int i = 0; i < shape.collisionCount; i++) {
            BoundingBox box = shape.collision[i];
            if (position.x + halfWidth <= box.min.x || position.x - halfWidth >= box.max.x ||
                position.z + halfWidth <= box.min.z || position.z - halfWidth >= box.max.z ||
                box.max.y > position.y + POSITION_TOLERANCE || box.max.y < position.y - GROUND_TOLERANCE) continue;
            if (!found || box.max.y > *height) *height = box.max.y;
            found = true;
        }
    }
    return found;
}

static bool FindLanding(Vector3 from, Vector3 to, float *height) {
    // A brief landing on a block edge can happen between position packets.
    // Work backwards to find the last ground contact along the accepted move.
    float length = fmaxf(fabsf(to.x - from.x), fmaxf(fabsf(to.y - from.y), fabsf(to.z - from.z)));
    int steps = (int)ceilf(length / POSITION_TOLERANCE);
    for (int i = 1; i <= steps; i++) {
        float t = i / (float)steps;
        Vector3 position = {to.x + (from.x - to.x) * t,
            to.y + (from.y - to.y) * t, to.z + (from.z - to.z) * t};
        if (FindGround(position, height)) return true;
    }
    return false;
}

static void CorrectPosition(Player *player, const char *reason) {
    double now = GetTime();
    if (now >= player->movementLogTime) {
        TraceLog(LOG_WARNING, "Movement correction for player %d: %s", player->id, reason);
        player->movementLogTime = now + 2.0;
    }
    Entity entity = serverWorld.entities[player->entityId];
    entity.id = USHRT_MAX;
    Vector3 position = {entity.position.x - 0.5f, entity.position.y, entity.position.z - 0.5f};
    ServerNetwork_Send(player, ServerPacket_CreateTeleportEntity(&entity, position, entity.rotation));
    // A correction must not erase an ongoing fall or refill movement allowances.
}

void ServerPlayer_UpdatePositionRotation(Player *player, Vector3 position, Vector3 rotation) {
    if (!serverWorld.entities || player->entityId < 0 || player->entityId >= WORLD_MAX_ENTITIES) return;
    if (!player->movementReady) ServerPlayer_ResetMovement(player);
    Vector3 previous = serverWorld.entities[player->entityId].position;
    double now = GetTime();
    // Streaming may delay processing a batch of packets. Refill credit using
    // server arrival times, so that queued movement retains its original pacing.
    double sampleTime = player->movementReceivedTime > 0 ? player->movementReceivedTime : now;
    player->movementReceivedTime = 0;
    float elapsed = fmax(0, sampleTime - player->movementTime);
    if (now >= player->impulseExpires) player->impulseAllowance = (Vector3){0};
    player->movementTime = fmax(player->movementTime, sampleTime);
    player->horizontalAllowance = fminf(PLAYER_HORIZONTAL_ALLOWANCE, player->horizontalAllowance + elapsed * PLAYER_HORIZONTAL_SPEED);
    player->upAllowance = fminf(PLAYER_UP_ALLOWANCE, player->upAllowance + elapsed * PLAYER_UP_SPEED);
    player->downAllowance = fminf(PLAYER_DOWN_ALLOWANCE, player->downAllowance + elapsed * PLAYER_DOWN_SPEED);
    Vector3 delta = {position.x - previous.x, position.y - previous.y, position.z - previous.z};
    float horizontal = sqrtf(delta.x * delta.x + delta.z * delta.z);
    if (!isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z) ||
        fabsf(position.x) > 1000000 || fabsf(position.y) > 1000000 || fabsf(position.z) > 1000000 ||
        horizontal > player->horizontalAllowance + player->impulseAllowance.x ||
        delta.y > player->upAllowance + player->impulseAllowance.y ||
        -delta.y > player->downAllowance + player->impulseAllowance.z) {
        CorrectPosition(player, "speed allowance or invalid coordinates");
        return;
    }

    bool liquid = false;
    bool clear = ClearPath(previous, position, &liquid);
    if (!clear) clear = ClearAxisPath(previous, position, &liquid);
    if (!clear && delta.y >= 0 && delta.y <= 0.6f) {
        // The client can step onto slabs/stairs. Check a short up/across/down path.
        float ground;
        if (FindGround(previous, &ground)) {
            Vector3 raisedStart = previous, raisedEnd = position;
            raisedStart.y = raisedEnd.y = fmaxf(previous.y, position.y) + 0.1f;
            liquid = false;
            clear = ClearPath(previous, raisedStart, &liquid) &&
                ClearPath(raisedStart, raisedEnd, &liquid) && ClearPath(raisedEnd, position, &liquid);
        }
    }
    if (!clear) {
        CorrectPosition(player, "collision or unloaded terrain");
        return;
    }

    player->impulseAllowance.x -= fmaxf(0, horizontal - player->horizontalAllowance);
    player->impulseAllowance.y -= fmaxf(0, delta.y - player->upAllowance);
    player->impulseAllowance.z -= fmaxf(0, -delta.y - player->downAllowance);
    player->horizontalAllowance = fmaxf(0, player->horizontalAllowance - horizontal);
    player->upAllowance = fmaxf(0, player->upAllowance - fmaxf(0, delta.y));
    player->downAllowance = fmaxf(0, player->downAllowance - fmaxf(0, -delta.y));
    ServerWorld_TeleportEntity(player->entityId, position, rotation);
    float ground = position.y;
    bool grounded = delta.y <= 0 && FindGround(position, &ground);
    if (liquid) {
        player->falling = false;
        player->fallPeak = position.y;
    } else if (grounded || (delta.y <= 0 && FindLanding(previous, position, &ground))) {
        float distance = player->fallPeak - ground;
        bool landed = player->falling && distance >= 1.0f / 64;
        player->falling = !grounded;
        player->fallPeak = fmaxf(ground, position.y);
        if (landed) ScriptHooks_PlayerLand(player->id, distance);
    } else {
        player->falling = true;
        player->fallPeak = fmaxf(player->fallPeak, position.y);
    }
}
