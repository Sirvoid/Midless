/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "world.h"
#include "networkhandler.h"
#include "packet.h"
#include "raylib.h"
#include "math.h"
#include "entity.h"
#include "time.h"

#define WORLD_MAX_ENTITIES 1028

World world;

void World_Init(void) {
    world.players = MemAlloc(sizeof(Player*) * 256);
    
    world.entities = MemAlloc(WORLD_MAX_ENTITIES * sizeof(Entity));
    
}

void World_AddPlayer(void *player) {
    
    Player* p = (Player*)player;
    
    for(int i = 0; i < 256; i++) {
        if(world.players[i]) continue;
        world.players[i] = p;
        world.players[i]->id = i;
        World_AddEntity(i, 1, (Vector3) {0, 0, 0});
        break;
    }
    
    for(int i = 0; i < 256; i++) {
        if(!world.players[i] || i == p->id) continue;
        Network_Send(player, Packet_SpawnEntity(&world.entities[i]));
    }
    
    World_SendMessage(TextFormat("%s joined the game!", p->name));
}

void World_RemovePlayer(void *player) {
    Player* curPlayer = (Player*)player;
    for(int i = 0; i < 256; i++) {
        if(!world.players[i]) continue;
        if(world.players[i]->id == curPlayer->id) {
            world.players[i] = NULL;
            World_RemoveEntity(i);
            break;
        }
    }
}

void World_TeleportEntity(int ID, Vector3 position, Vector3 rotation) {
    if(world.entities[ID].type == 0) return;
    world.entities[ID].position = position;
    world.entities[ID].rotation = rotation;
    World_BroadcastExcluding(Packet_TeleportEntity(&world.entities[ID], position, rotation), ID);
}

void World_AddEntity(int ID, int type, Vector3 position) {
    world.entities[ID].ID = ID;
    world.entities[ID].type = type;
    world.entities[ID].position = position;
    World_BroadcastExcluding(Packet_SpawnEntity(&world.entities[ID]), ID);
}

void World_RemoveEntity(int ID) {
    world.entities[ID].type = 0;
}

void World_SendMessage(const char* message) {
    int parts = TextLength(message) / 64;
    
    for(int i = 0; i <= parts; i++) {
        const char *messageChunk = TextSubtext(message, i * 64, 64);
        World_Broadcast(Packet_SendMessage(messageChunk));
    }
    
}

void World_Broadcast(unsigned char* packet) {
    for(int i = 0; i < 256; i++) {
        if(!world.players[i]) continue;
        Network_Send((void*)world.players[i], packet);
    }
}

void World_BroadcastExcluding(unsigned char* packet, int excludedPlayerID) {
    for(int i = 0; i < 256; i++) {
        if(!world.players[i]) continue;
        if(world.players[i]->id == excludedPlayerID) continue;
        Network_Send((void*)world.players[i], packet);
    }
}

int World_GetBlock(Vector3 blockPos) {
    return 0;
}

void World_SetBlock(Vector3 blockPos, int blockID, bool broadcast) {
    
    if(broadcast) {
        World_Broadcast(Packet_SetBlock(blockID, blockPos));
    }
}