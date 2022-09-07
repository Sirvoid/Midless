/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "player.h"
#include "world.h"
#include "chunk/chunk.h"
#include "networkhandler.h"
#include "packet.h"

void Player_UpdatePositionRotation(Player* player, Vector3 position, Vector3 rotation) {
    World_TeleportEntity(player->id, position, rotation);
}

void Player_LoadChunks(Player* player) {

    Entity entity = world.entities[player->id];

    Vector3 playerChunkPos = (Vector3) {(int)floor(entity.position.x / CHUNK_SIZE_X), (int)floor(entity.position.y / CHUNK_SIZE_Y), (int)floor(entity.position.z / CHUNK_SIZE_Z)};

    int loadingHeight = fmin(player->drawDistance, 4);
    for (int y = loadingHeight; y >= -loadingHeight; y--) {
        for (int x = -player->drawDistance; x <= player->drawDistance; x++) {
            for (int z = -player->drawDistance; z <= player->drawDistance; z++) {
                Vector3 chunkPos = (Vector3) {playerChunkPos.x + x, playerChunkPos.y + y, playerChunkPos.z + z};
                
                if (Vector3Distance(chunkPos, playerChunkPos) < player->drawDistance + 3) {
                    Chunk* chunk = World_RequestChunk(chunkPos);
                    if (!Chunk_PlayerInChunk(chunk, player)) {
                        Chunk_AddPlayer(chunk, player);

                        int compressedLength = 0;
                        unsigned short *compressedChunk = Chunk_Compress(chunk, CHUNK_SIZE, &compressedLength);

                        Network_Send(player, Packet_LoadChunk(compressedChunk, compressedLength, chunkPos));

                        MemFree(compressedChunk);
                    }
                }
            }
        }
    }
    
}

