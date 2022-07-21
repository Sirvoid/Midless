/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "raycast.h"
#include "raylib.h"
#include "raymath.h"
#include "world.h"
#include "math.h"
#include "block/block.h"

#define RAYCAST_PRECISION 0.05f
#define RAYCAST_REACH 8

RaycastResult Raycast_Do(Vector3 position, Vector3 direction, bool ignoreLiquid) {
    float i = 0;
    Vector3 oldPos = position;
    
    while(i < RAYCAST_REACH / RAYCAST_PRECISION) {
        i += 1;
        
        oldPos = position;
        position.x += direction.x * RAYCAST_PRECISION;
        position.y += direction.y * RAYCAST_PRECISION;
        position.z += direction.z * RAYCAST_PRECISION;
        
        int blockID = World_GetBlock(position);
        
        if(blockID != 0) {
            Block block = Block_GetDefinition(blockID);
            if (ignoreLiquid && block.colliderType == BlockColliderType_Liquid) {
                continue;
            }
            Vector3 blockPos = (Vector3){floor(position.x), floor(position.y), floor(position.z)};
            if (position.x > blockPos.x + block.minBB.x / 16 &&
                position.y > blockPos.y + block.minBB.y / 16 &&
                position.z > blockPos.z + block.minBB.z / 16 &&
                position.x < blockPos.x + block.maxBB.x / 16 &&
                position.y < blockPos.y + block.maxBB.y / 16 &&
                position.z < blockPos.z + block.maxBB.z / 16) {
                Vector3 loc = Vector3Subtract(position, Vector3Add(blockPos, Vector3Scale(block.minBB, 1.0f / 16)));
                Vector3 bbSize = Vector3Scale(block.maxBB, 1.0f / 16);
                float entryX = -loc.x;
                if (loc.x > bbSize.x - loc.x)
                    entryX = bbSize.x - loc.x;
                
                float entryY = -loc.y;
                if (loc.y > bbSize.y - loc.y)
                    entryY = bbSize.y - loc.y;
                
                float entryZ = -loc.z;
                if (loc.z > bbSize.z - loc.z)
                    entryZ = bbSize.z - loc.z;
                
                Vector3 normal = (Vector3){0,0,0};
                
                if (fabs(entryX) < fabs(entryY)) {
                    if (fabs(entryX) < fabs(entryZ)) {
                        normal.x = (entryX > 0) ? 1 : ((entryX < 0) ? -1 : 0);
                        normal.y = 0.0f;
                        normal.z = 0.0f;
                    } else {
                        normal.z = (entryZ > 0) ? 1 : ((entryZ < 0) ? -1 : 0);
                        normal.x = 0.0f;
                        normal.y = 0.0f;
                    }
                } else {
                    if (fabs(entryY) < fabs(entryZ)) {
                        normal.y = (entryY > 0) ? 1 : ((entryY < 0) ? -1 : 0);
                        normal.x = 0.0f;
                        normal.z = 0.0f;

                    } else {
                        normal.z = (entryZ > 0) ? 1 : ((entryZ < 0) ? -1 : 0);
                        normal.x = 0.0f;
                        normal.y = 0.0f;
                    }
                }
                
                return (RaycastResult) {position, oldPos, blockID, normal};
            }
            else
                continue;
        }
    }
    
    return (RaycastResult) {position, oldPos, -1, (Vector3){0,0,0}};
}