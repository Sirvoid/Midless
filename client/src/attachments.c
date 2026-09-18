/* Copyright (c) 2026 Sirvoid. Released under the MIT License. */
#include "raylib.h"
#include "raymath.h"
#include "attachments.h"
#include "world.h"
#include "player.h"
#include "networking/packet.h"
#include "networking/networkhandler.h"
#include "gui/screens.h"
#include <string.h>

static bool Resolve(int id, unsigned char *visited) {
    if (visited[id] == 2) return true;
    if (visited[id] == 1) return false;
    visited[id] = 1;
    bool local = id == WORLD_MAX_ENTITIES;
    if (!local && !world.entities[id].type) return false;
    Attachment *a = local ? &player.attachment : &world.entities[id].attachment;
    if (a->parent) {
        int parent = a->parent - 1;
        if (parent < 0 || parent > WORLD_MAX_ENTITIES || !Resolve(parent, visited)) return false;
        Vector3 pp, pr;
        if (parent == WORLD_MAX_ENTITIES) {
            pp = Vector3Add(player.position, (Vector3){0.5f, 0, 0.5f});
            pr = player.attachment.inheritRotation && player.attachment.parent
                     ? player.attachedRotation
                     : Player_GetRotation();
        } else {
            pp = world.entities[parent].position;
            pr = world.entities[parent].rotation;
        }
        Vector3 position = Attachment_Position(pp, pr, a->offset);
        Vector3 rotation = Attachment_Rotation(pr, a->rotation);
        if (local) {
            Player_SetAttachedPosition(Vector3Subtract(position, (Vector3){0.5f, 0, 0.5f}),
                                       rotation);
        } else {
            Entity *e = &world.entities[id];
            e->position = position;
            if (a->inheritRotation) e->rotation = rotation;
            e->animation.walkAmount = 0;
            e->animation.walkSpeed = 0;
            e->animation.lastPosition = position;
        }
    }
    visited[id] = 2;
    return true;
}
void ClientAttachments_Update(void) {
    unsigned char visited[WORLD_MAX_ENTITIES + 1] = {0};
    for (int i = 0; i <= WORLD_MAX_ENTITIES; i++)
        Resolve(i, visited);
    if (player.attachment.parent) Player_RefreshAttachedCamera();
}
void ClientAttachments_Remove(int id) {
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        Entity *e = &world.entities[i];
        if (e->attachment.parent == id + 1) {
            e->attachment = (Attachment){0};
            e->targetPosition = e->position;
        }
    }
    if (player.attachment.parent == id + 1) {
        player.attachment = (Attachment){0};
        Player_Teleport(player.position);
    }
    if (player.controlledEntity == id + 1) player.controlledEntity = 0;
}
void ClientControl_Update(void) {
    static uint32_t lastSession;
    static int lastForward, lastSideways, lastFlags;
    static double lastSend;
    if (!player.controlledEntity) return;
    int forward = 0, sideways = 0, flags = 0;
    if (IsWindowFocused() && !screenCursorEnabled) {
        forward = IsKeyDown(screenKeys[CONTROL_FORWARD]) - IsKeyDown(screenKeys[CONTROL_BACKWARD]);
        sideways = IsKeyDown(screenKeys[CONTROL_RIGHT]) - IsKeyDown(screenKeys[CONTROL_LEFT]);
        flags = (IsKeyDown(screenKeys[CONTROL_JUMP]) ? 1 : 0) | (IsKeyDown(screenKeys[CONTROL_SNEAK]) ? 2 : 0);
    }
    double now = GetTime();
    if (lastSession != player.controlSession || forward != lastForward ||
        sideways != lastSideways || flags != lastFlags || now - lastSend >= 0.05) {
        Network_Send(Packet_CreateControlInput(forward, sideways, flags, Player_GetRotation()));
        lastSession = player.controlSession;
        lastForward = forward;
        lastSideways = sideways;
        lastFlags = flags;
        lastSend = now;
    }
}
