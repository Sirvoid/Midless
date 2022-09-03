/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#define __clang__ true
#define STB_DS_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "stb_ds.h"
#include "world.h"
#include "chunk/chunk.h"
#include "networkhandler.h"
#include "packet.h"
#include "entity.h"
#include "worldgenerator.h"

World world;

void World_Init(void) {
    world.players = MemAlloc(sizeof(Player*) * 256);
    world.entities = MemAlloc(WORLD_MAX_ENTITIES * sizeof(Entity));

    int seed = rand();

    //Create world directory
    struct stat st = {0};
    if (stat("./world", &st) == -1) {
        mkdir("./world");
    }
    
    if(FileExists("./world/seed.dat")) {
        unsigned int bytesRead = 0;
        unsigned char *data = LoadFileData("./world/seed.dat", &bytesRead);
        seed = (int)(data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]); 
        UnloadFileData(data);
    } else {
        char data[4] = {(char)(seed >> 24), (char)(seed >> 16), (char)(seed >> 8), (char)(seed)};
        SaveFileData("./world/seed.dat", data, 4);
    }

    WorldGenerator_Init(seed);
}

void World_Update(void) {

    for(int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *player = world.players[i];
        if(player != NULL) Player_LoadChunks(player);
    }

    for(int i = 0; i < hmlen(world.chunks); i++) {
        Chunk *chunk = world.chunks[i].value; 

        for(int j = arrlen(chunk->players) - 1; j >= 0; j--) {
            Player *player = chunk->players[j];
            Entity entity = world.entities[player->id];
            Vector3 playerChunkPos = (Vector3) {(int)floor(entity.position.x / CHUNK_SIZE_X), (int)floor(entity.position.y / CHUNK_SIZE_Y), (int)floor(entity.position.z / CHUNK_SIZE_Z)};
            if(Vector3Distance(chunk->position, playerChunkPos) >= player->drawDistance + 3) {
                Network_Send(player, Packet_UnloadChunk(chunk->position));
                Chunk_RemovePlayer(chunk, j);
            }
        }

        if(arrlen(chunk->players) == 0) {
            World_RemoveChunk(chunk);
        }
    }
}

void World_RemovePlayerFromChunks(Player *playerToRemove) {
    for(int i = 0; i < hmlen(world.chunks); i++) {
        Chunk *chunk = world.chunks[i].value; 

        for(int j = arrlen(chunk->players) - 1; j >= 0; j--) {
            Player *player = chunk->players[j];
            if(player != playerToRemove) continue;
            Chunk_RemovePlayer(chunk, j);
        }

    }
}

Chunk* World_AddChunk(Vector3 position) {

    long int p = (long)((int)(position.x)&4095)<<20 | (long)((int)(position.z)&4095)<<8 | (long)((int)(position.y)&255);
    int index = hmgeti(world.chunks, p);
    Chunk *chunk;
    if(index == -1) {
        //Add chunk to list
        chunk = MemAlloc(sizeof(Chunk));
        hmput(world.chunks, p, chunk);

        Chunk_Init(chunk, position);
    } else {
        chunk = world.chunks[index].value;
    }

    return chunk;
}

void World_RemoveChunk(Chunk *curChunk) {
    long int p = (long)((int)(curChunk->position.x)&4095)<<20 | (long)((int)(curChunk->position.z)&4095)<<8 | (long)((int)(curChunk->position.y)&255);

    int index = hmgeti(world.chunks, p);
    if(index >= 0) {
        Chunk_Unload(curChunk);
        hmdel(world.chunks, p);
    }
    
}

Chunk* World_GetChunkAt(Vector3 position) {
    long int p = (long)((int)(position.x)&4095)<<20 | (long)((int)(position.z)&4095)<<8 | (long)((int)(position.y)&255);
    int index = hmgeti(world.chunks, p);
    if(index >= 0) {
        return world.chunks[index].value;
    }
    
    return NULL;
}

Chunk* World_RequestChunk(Vector3 position) {
    return World_AddChunk(position);
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
    World_RemovePlayerFromChunks(player);
    Player* curPlayer = (Player*)player;
    for(int i = 0; i < 256; i++) {
        if(world.players[i] == NULL) continue;
        if(world.players[i] == curPlayer) {
            world.players[i] = NULL;
            World_RemoveEntity(i);
            break;
        }
    }
    MemFree(player);
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
    World_BroadcastExcluding(Packet_DespawnEntity(&world.entities[ID]), ID);
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
    //Get Chunk
    Vector3 chunkPos = (Vector3) { floor(blockPos.x / CHUNK_SIZE_X), floor(blockPos.y / CHUNK_SIZE_Y), floor(blockPos.z / CHUNK_SIZE_Z) };
    Chunk* chunk = World_GetChunkAt(chunkPos);
    
    if(chunk == NULL) return 0;
    
    //Get Block
    Vector3 blockPosInChunk = (Vector3) { 
                                floor(blockPos.x) - chunk->blockPosition.x,
                                floor(blockPos.y) - chunk->blockPosition.y, 
                                floor(blockPos.z) - chunk->blockPosition.z 
                               };

    return Chunk_GetBlock(chunk, blockPosInChunk);
}

void World_SetBlock(Vector3 blockPos, int blockID, bool broadcast) {
    
    //Get Chunk
    Vector3 chunkPos = (Vector3) { floor(blockPos.x / CHUNK_SIZE_X), floor(blockPos.y / CHUNK_SIZE_Y), floor(blockPos.z / CHUNK_SIZE_Z) };
    Chunk* chunk = World_GetChunkAt(chunkPos);
    
    if(chunk == NULL) return;

    //Set Block
    Vector3 blockPosInChunk = (Vector3) { 
                                floor(blockPos.x) - chunkPos.x * CHUNK_SIZE_X, 
                                floor(blockPos.y) - chunkPos.y * CHUNK_SIZE_Y, 
                                floor(blockPos.z) - chunkPos.z * CHUNK_SIZE_Z 
                               };
    
    Chunk_SetBlock(chunk, blockPosInChunk, blockID);

    if(broadcast) {
        World_Broadcast(Packet_SetBlock(blockID, blockPos));
    }
}