#include "raycast.h"
#include "raylib.h"
#include "world.h"
#include "math.h"

#define RAYCAST_PRECISION 0.05f

RaycastResult Raycast_Do(Vector3 position, Vector3 direction) {
    
    float i = 0;
    Vector3 oldPos = position;
    
    while(i < 160) {
        i += 1;
        
        oldPos = position;
        position.x += direction.x * RAYCAST_PRECISION;
        position.y += direction.y * RAYCAST_PRECISION;
        position.z += direction.z * RAYCAST_PRECISION;
        
        int blockID = World_GetBlock(position);
        
        if(blockID != 0) {
            RaycastResult result = (RaycastResult) {position, oldPos, blockID}; 
            return result;
        }
    }
    
    RaycastResult result = (RaycastResult) {position, oldPos, -1}; 
    return result;
}