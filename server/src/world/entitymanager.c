#include "world.h"
#include "../networkhandler.h"
#include "../packet.h"

void ServerWorld_TeleportEntity(int id, Vector3 position, Vector3 rotation) {
    if (serverWorld.entities[id].type == 0) return;
    serverWorld.entities[id].position = position;
    serverWorld.entities[id].rotation = rotation;
    ServerWorld_BroadcastExcluding(
        ServerPacket_CreateTeleportEntity(&serverWorld.entities[id], position, rotation), id);
}

void ServerWorld_AddEntity(int id, int type, int model, Vector3 position) {
    serverWorld.entities[id].id = id;
    serverWorld.entities[id].type = type;
    serverWorld.entities[id].model = (unsigned char)model;
    serverWorld.entities[id].position = position;
    ServerWorld_BroadcastExcluding(ServerPacket_CreateSpawnEntity(&serverWorld.entities[id]), id);
}

void ServerWorld_RemoveEntity(int id) {
    serverWorld.entities[id].type = 0;
    ServerWorld_BroadcastExcluding(ServerPacket_CreateDespawnEntity(&serverWorld.entities[id]), id);
}
