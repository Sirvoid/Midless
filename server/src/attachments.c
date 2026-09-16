/* Copyright (c) 2026 Sirvoid. Released under the MIT License. */
#include "raylib.h"
#include "raymath.h"
#include "attachments.h"
#include "world/world.h"
#include "world/entitypersistence.h"
#include "entityphysics.h"
#include "networkhandler.h"
#include "packet.h"
#include "mobs.h"
#include <math.h>

static Entity *Controlled(Player *p);
static void Revoke(Entity *e);

static bool Live(Entity *e) {
    if (!e || !e->active || e->pendingRemoval || e->dead || e->relationshipBusy) return false;
    if (e->ownerPlayerId >= 0) {
        Player *p = serverWorld.players[e->ownerPlayerId];
        return p && p->movementReady && !p->disconnected && !p->leaveInvoked &&
               p->entityId == e->id;
    }
    return !EntityPersistence_IsSaving(e);
}
Entity *ServerAttachment_Parent(Entity *e) {
    int id = e->attachment.parent - 1;
    if (id < 0 || id >= WORLD_MAX_ENTITIES) return NULL;
    Entity *parent = &serverWorld.entities[id];
    return parent->active && !parent->pendingRemoval && parent->generation == e->parentGeneration
               ? parent
               : NULL;
}
static void Stop(Entity *e) {
    e->body.velocity = e->moveVelocity = (Vector3){0};
    e->moveEnabled = e->recovering = false;
    e->body.sleeping = e->body.grounded = false;
    if (e->mob) {
        e->mob->pathCount = 0;
        e->mob->hasWaypoint = false;
    }
}
static void ResetPlayer(Entity *e, bool teleport) {
    if (e->ownerPlayerId < 0) return;
    Player *p = serverWorld.players[e->ownerPlayerId];
    if (!p || p->entityId != e->id) return;
    p->attachmentEpoch++;
    ServerPlayer_ResetMovement(p);
    if (p->disconnected) return;
    ServerNetwork_Send(p, ServerPacket_CreateAttachment(e, p));
    if (teleport) {
        Entity local = *e;
        local.id = ATTACHMENT_LOCAL;
        Vector3 position = Vector3Subtract(e->position, (Vector3){0.5f, 0, 0.5f});
        ServerNetwork_Send(p, ServerPacket_CreateTeleportEntity(&local, position, e->rotation));
    }
}
bool ServerAttachment_Set(Entity *e, Entity *parent, Attachment a) {
    if (!Live(e) || !Live(parent) || e == parent) return false;
    float values[] = {a.offset.x, a.offset.y, a.offset.z, a.rotation.x, a.rotation.y, a.rotation.z};
    for (int i = 0; i < 6; i++)
        if (!isfinite(values[i]) || fabsf(values[i]) > (i < 3 ? 64 : 100)) return false;
    Entity *ancestor = parent;
    for (int depth = 0; ancestor; depth++) {
        if (ancestor == e || depth >= WORLD_MAX_ENTITIES) return false;
        ancestor = ServerAttachment_Parent(ancestor);
    }
    if (e->controller && !ServerControl_Set(e, NULL)) return false;
    if (!Live(e) || !Live(parent)) return false;
    ancestor = parent;
    for (int depth = 0; ancestor; depth++) {
        if (ancestor == e || depth >= WORLD_MAX_ENTITIES) return false;
        ancestor = ServerAttachment_Parent(ancestor);
    }
    a.parent = parent->id + 1;
    e->attachment = a;
    e->parentGeneration = parent->generation;
    Stop(e);
    e->position = Attachment_Position(parent->position, parent->rotation, a.offset);
    if (a.inheritRotation) e->rotation = Attachment_Rotation(parent->rotation, a.rotation);
    e->attachmentDirty = e->dirty = true;
    ServerPhysics_InvalidateIndex();
    ResetPlayer(e, false);
    return true;
}
void ServerAttachment_Detach(Entity *e, const Vector3 *position, bool inheritVelocity) {
    if (!e || !e->attachment.parent) return;
    Entity *parent = ServerAttachment_Parent(e);
    Vector3 velocity = parent && inheritVelocity ? parent->body.velocity : (Vector3){0};
    e->attachment = (Attachment){0};
    e->parentGeneration = 0;
    Stop(e);
    if (position) e->position = *position;
    e->attachmentDirty = e->dirty = true;
    if (e->ownerPlayerId >= 0) {
        Player *p = serverWorld.players[e->ownerPlayerId];
        if (Controlled(p)) Revoke(Controlled(p));
    }
    ResetPlayer(e, true);
    if (inheritVelocity) {
        if (e->ownerPlayerId < 0)
            e->body.velocity = velocity;
        else {
            Player *p = serverWorld.players[e->ownerPlayerId];
            velocity.x = Clamp(velocity.x, -20, 20);
            velocity.y = Clamp(velocity.y, -20, 20);
            velocity.z = Clamp(velocity.z, -20, 20);
            if (p) ServerPlayer_ApplyImpulse(p, velocity);
        }
    }
    ServerPhysics_InvalidateIndex();
}
Player *ServerControl_Player(Entity *e) {
    if (!e || e->controller <= 0 || e->controller > WORLD_MAX_PLAYERS) return NULL;
    Player *p = serverWorld.players[e->controller - 1];
    return p && !p->disconnected && !p->leaveInvoked &&
                   p->connectionId == e->controllerConnection && p->controlledEntity == e->id + 1 &&
                   p->controlledGeneration == e->generation
               ? p
               : NULL;
}
static Entity *Controlled(Player *p) {
    if (!p || p->controlledEntity <= 0 || p->controlledEntity > WORLD_MAX_ENTITIES) return NULL;
    Entity *e = &serverWorld.entities[p->controlledEntity - 1];
    return e->active && e->generation == p->controlledGeneration && e->controller == p->id + 1 &&
                   e->controllerConnection == p->connectionId
               ? e
               : NULL;
}
static bool changingControl;
static void Revoke(Entity *e) {
    if (!e->controller) return;
    Player *p = serverWorld.players[e->controller - 1];
    uint64_t connection = e->controllerConnection;
    e->controller = 0;
    e->controllerConnection = 0;
    e->moveEnabled = false;
    e->moveVelocity = (Vector3){0};
    if (p && p->connectionId == connection && p->controlledEntity == e->id + 1 &&
        p->controlledGeneration == e->generation) {
        p->controlledEntity = 0;
        p->controlledGeneration = 0;
        p->controlForward = p->controlSideways = p->controlFlags = 0;
        p->controlSession++;
        if (!p->disconnected) ServerNetwork_Send(p, ServerPacket_CreateControlState(p));
        bool wasChanging = changingControl;
        changingControl = true;
        ScriptHooks_Control(e, p, 0, -1);
        changingControl = wasChanging;
    }
}
bool ServerControl_Set(Entity *e, Player *p) {
    if (!e || changingControl) return false;
    if (p && (!Live(e) || e->ownerPlayerId >= 0 || e->attachment.parent || p->disconnected ||
              p->leaveInvoked || p->entityId < 0 || !Live(&serverWorld.entities[p->entityId])))
        return false;
    if (p && ServerControl_Player(e) == p) return true;
    changingControl = true;
    Revoke(e);
    if (Controlled(p)) Revoke(Controlled(p));
    bool ok = !p || (Live(e) && !e->attachment.parent && !p->disconnected && !p->leaveInvoked &&
                     Live(&serverWorld.entities[p->entityId]));
    if (p && ok) {
        Stop(e);
        e->controller = p->id + 1;
        e->controllerConnection = p->connectionId;
        p->controlledEntity = e->id + 1;
        p->controlledGeneration = e->generation;
        p->controlSession++;
        p->controlReceived = GetTime();
        p->controlLook = (Vector3){0};
        p->controlForward = p->controlSideways = p->controlFlags = 0;
        ServerNetwork_Send(p, ServerPacket_CreateControlState(p));
        ScriptHooks_Control(e, p, 0, 1);
    }
    changingControl = false;
    return ok;
}
void ServerControl_Input(Player *p, uint32_t session, int forward, int sideways, unsigned flags,
                         Vector3 look) {
    if (!p || p->disconnected || !p->controlledEntity || session != p->controlSession ||
        forward < -1 || forward > 1 || sideways < -1 || sideways > 1 || (flags & ~3u) ||
        !isfinite(look.x) || !isfinite(look.y) || !isfinite(look.z))
        return;
    Entity *e = &serverWorld.entities[p->controlledEntity - 1];
    if (ServerControl_Player(e) != p) return;
    p->controlForward = forward;
    p->controlSideways = sideways;
    p->controlFlags = flags;
    p->controlLook = look;
    p->controlReceived = GetTime();
}
void ServerControl_Step(Entity *e, float dt) {
    Player *p = ServerControl_Player(e);
    if (!p) return;
    if (GetTime() - p->controlReceived > 0.5) {
        p->controlForward = p->controlSideways = p->controlFlags = 0;
        e->moveEnabled = false;
    }
    ScriptHooks_Control(e, p, dt, 0);
}
void ServerAttachments_Cleanup(Entity *e) {
    if (!e || e->relationshipBusy) return;
    e->relationshipBusy = true;
    Revoke(e);
    if (e->ownerPlayerId >= 0) {
        Player *p = serverWorld.players[e->ownerPlayerId];
        if (Controlled(p)) Revoke(Controlled(p));
        if (p && p->controlledEntity) {
            p->controlledEntity = 0;
            p->controlledGeneration = 0;
            p->controlForward = p->controlSideways = p->controlFlags = 0;
            p->controlSession++;
            if (!p->disconnected) ServerNetwork_Send(p, ServerPacket_CreateControlState(p));
        }
    }
    ServerAttachment_Detach(e, NULL, false);
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        Entity *child = &serverWorld.entities[i];
        if (child->active && child->attachment.parent == e->id + 1 &&
            child->parentGeneration == e->generation)
            ServerAttachment_Detach(child, NULL, false);
    }
    e->relationshipBusy = false;
}
static void Resolve(Entity *e, unsigned char *visited, bool *changed) {
    if (visited[e->id] == 2) return;
    if (visited[e->id] == 1) {
        ServerAttachment_Detach(e, NULL, false);
        return;
    }
    visited[e->id] = 1;
    Entity *parent = ServerAttachment_Parent(e);
    if (parent) {
        Resolve(parent, visited, changed);
        Vector3 previousPosition = e->position, previousRotation = e->rotation;
        e->position = Attachment_Position(parent->position, parent->rotation, e->attachment.offset);
        if (e->attachment.inheritRotation)
            e->rotation = Attachment_Rotation(parent->rotation, e->attachment.rotation);
        if (!Vector3Equals(previousPosition, e->position) ||
            !Vector3Equals(previousRotation, e->rotation)) {
            e->dirty = true;
            *changed = true;
        }
    } else if (e->attachment.parent)
        ServerAttachment_Detach(e, NULL, false);
    visited[e->id] = 2;
}
void ServerAttachments_Update(void) {
    unsigned char visited[WORLD_MAX_ENTITIES] = {0};
    bool changed = false;
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        Entity *e = &serverWorld.entities[i];
        if (!e->active) continue;
        if (e->pendingRemoval || e->dead ||
            (e->ownerPlayerId >= 0 && (!serverWorld.players[e->ownerPlayerId] ||
                                       (serverWorld.players[e->ownerPlayerId]->disconnected ||
                                        serverWorld.players[e->ownerPlayerId]->leaveInvoked))))
            ServerAttachments_Cleanup(e);
        if (e->controller && !ServerControl_Player(e)) Revoke(e);
    }
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++)
        if (serverWorld.entities[i].active) Resolve(&serverWorld.entities[i], visited, &changed);
    if (changed) ServerPhysics_InvalidateIndex();
}
void ServerAttachments_Replicate(void) {
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        Entity *e = &serverWorld.entities[i];
        if (!e->active || !e->announced || !e->attachmentDirty) continue;
        for (int j = 0; j < WORLD_MAX_PLAYERS; j++) {
            Player *p = serverWorld.players[j];
            if (p && !p->disconnected) ServerNetwork_Send(p, ServerPacket_CreateAttachment(e, p));
        }
        e->attachmentDirty = false;
    }
}
void ServerAttachments_Send(Player *p) {
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        Entity *e = &serverWorld.entities[i];
        if (e->active && !e->pendingRemoval && e->type == ENTITY_TYPE_DROPPED_ITEM &&
            ServerAttachments_HasLinks(e)) {
            ServerNetwork_Send(p, ServerPacket_CreateDroppedItem(e));
            e->drop.viewers[p->id] = true;
        }
    }
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        Entity *e = &serverWorld.entities[i];
        if (e->active && e->announced && e->attachment.parent)
            ServerNetwork_Send(p, ServerPacket_CreateAttachment(e, p));
    }
    ServerNetwork_Send(p, ServerPacket_CreateControlState(p));
}
bool ServerAttachments_ChunkOccupied(Vector3 chunk) {
    bool occupied[WORLD_MAX_ENTITIES] = {0};
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        Entity *e = &serverWorld.entities[i];
        if (!e->active || e->pendingRemoval) continue;
        if (e->controller) occupied[i] = true;
        Entity *parent = ServerAttachment_Parent(e);
        if (parent) occupied[i] = occupied[parent->id] = true;
    }
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++)
        if (occupied[i]) {
            Vector3 p = serverWorld.entities[i].position;
            if (floorf(p.x / CHUNK_SIZE_X) == chunk.x && floorf(p.y / CHUNK_SIZE_Y) == chunk.y &&
                floorf(p.z / CHUNK_SIZE_Z) == chunk.z)
                return true;
        }
    return false;
}

bool ServerAttachments_HasLinks(Entity *e) {
    if (e->attachment.parent || e->controller) return true;
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        Entity *child = &serverWorld.entities[i];
        if (child->active && !child->pendingRemoval && child->attachment.parent == e->id + 1 &&
            child->parentGeneration == e->generation)
            return true;
    }
    return false;
}
