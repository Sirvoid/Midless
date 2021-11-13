#include "raycast.h"
#include "raylib.h"
#include "world.h"

#define RAYCAST_PRECISION 0.05f
#define RAYCAST_REACH 8

RaycastResult Raycast_Do(Vector3 position, Vector3 direction) {
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
            return (RaycastResult) {position, oldPos, blockID};
        }
    }
    
    return (RaycastResult) {position, oldPos, -1};
}