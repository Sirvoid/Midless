#include "raylib.h"
#include "raymath.h"
#include "player.h"
#include "math.h"
#include "world.h"
#include "raycast.h"

#define MOUSE_SENSITIVITY 0.003f

Vector2 Player_oldMousePos = {0.0f, 0.0f};
Vector2 Player_cameraAngle = {0.0f, 0.0f};

bool Player_cursorEnabled = false;

void Player_Init(Player *player) {

    Camera camera = { 0 };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 70.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    
    player->camera = camera;
    
    player->velocity = (Vector3) {0, 0, 0};
    player->position = (Vector3) { 16.0f, 64.0f, 16.0f };
    player->speed = 0.125f;
    
    player->collisionBox.min = (Vector3) { 0, 0, 0 };
    player->collisionBox.max = (Vector3) { 0.8f, 1.8f, 0.8f };

    SetCameraMode(player->camera, CAMERA_CUSTOM);
    DisableCursor();
}

void Player_CheckInputs(Player *player) {
    Vector2 mousePositionDelta = { 0.0f, 0.0f };
    Vector2 mousePos = GetMousePosition();
    
    mousePositionDelta.x = mousePos.x - Player_oldMousePos.x;
    mousePositionDelta.y = mousePos.y - Player_oldMousePos.y;
    
    Player_oldMousePos = GetMousePosition();
    
    Player_cameraAngle.x -= (mousePositionDelta.x * -MOUSE_SENSITIVITY);
    Player_cameraAngle.y -= (mousePositionDelta.y * -MOUSE_SENSITIVITY);
    
    //Limit head rotation
    float maxCamAngleY = PI - 0.01f;
    float minCamAngleY = 0.01f;
    
    if(Player_cameraAngle.y >= maxCamAngleY) 
        Player_cameraAngle.y = maxCamAngleY;
    else if(Player_cameraAngle.y <= minCamAngleY) 
        Player_cameraAngle.y = minCamAngleY;
    
    
    //Calculate direction vectors of the camera angle
    float cx = cosf(Player_cameraAngle.x);
    float sx = sinf(Player_cameraAngle.x);
    
    float cx90 = cosf(Player_cameraAngle.x + PI / 2);
    float sx90 = sinf(Player_cameraAngle.x + PI / 2);
    
    float sy = sinf(Player_cameraAngle.y);
    float cy = cosf(Player_cameraAngle.y);
    
    float forwardX = cx * sy;
    float forwardY = cy;
    float forwardZ = sx * sy;
    
    if(IsKeyPressed(KEY_ESCAPE)) {
        if(Player_cursorEnabled) {
            DisableCursor();
        } else {
            EnableCursor();
        }
        Player_cursorEnabled = !Player_cursorEnabled;
    }
    
    //Handle keys & mouse
    if(IsKeyDown(KEY_SPACE) && !player->jumped) {
        player->velocity.y += 0.2f;
        player->jumped = true;
    }
    
    if(IsKeyDown(KEY_W)) {
        player->velocity.z += sx * player->speed;
        player->velocity.x += cx * player->speed;
    }
    
    if(IsKeyDown(KEY_S)) {
       player->velocity.z -= sx * player->speed;
       player->velocity.x -= cx * player->speed;
    }
    
    if(IsKeyDown(KEY_A)) {
       player->velocity.z -= sx90 * player->speed;
       player->velocity.x -= cx90 * player->speed;
    }
    
    if(IsKeyDown(KEY_D)) {
       player->velocity.z += sx90 * player->speed;
       player->velocity.x += cx90 * player->speed;
    }
    
    if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        RaycastResult rayResult = Raycast_Do(player->camera.position, (Vector3) { forwardX, forwardY, forwardZ});
        World_SetBlock(rayResult.hitPos, 0);
    } else if(IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        RaycastResult rayResult = Raycast_Do(player->camera.position, (Vector3) { forwardX, forwardY, forwardZ});
        if(rayResult.hitBlockID != -1) {
            World_SetBlock(rayResult.prevPos, 4);
        }
    }
 
    //Place camera's target to the direction looking at.
    player->camera.target.x = player->camera.position.x + forwardX;
    player->camera.target.y = player->camera.position.y + forwardY;
    player->camera.target.z = player->camera.position.z + forwardZ;
    
    UpdateCamera(&player->camera);
}

void Player_Update(Player *player) {
    
    //Gravity
    player->velocity.y -= 0.015f;
    
    //Calculate velocity with delta time
    Vector3 velXdt = Vector3Scale(player->velocity, GetFrameTime() * 60);
    
    //Move X & Test Collisions
    player->position.x += velXdt.x;
    if(Player_TestCollision(player)) player->position.x -= velXdt.x;

    //Move Y & Test Collisions
    player->position.y += velXdt.y;
    if(Player_TestCollision(player)) {
        player->position.y -= velXdt.y;
        if(player->velocity.y <= 0) player->jumped = false;
        player->velocity.y = 0;
    }

    //Move Z & Test Collisions
    player->position.z += velXdt.z;
    if(Player_TestCollision(player)) player->position.z -= velXdt.z;

    //Place Camera
    player->camera.position = player->position;
    player->camera.position.y += 1.8f;
    player->camera.position.x += 0.4f;
    player->camera.position.z += 0.4f;

    player->velocity.x = 0;
    player->velocity.z = 0;

    Player_CheckInputs(player); 
}

bool Player_TestCollision(Player *player) {
    
    BoundingBox pB = player->collisionBox;
    pB.min = Vector3Add(pB.min, player->position);
    pB.max = Vector3Add(pB.max, player->position);
    
    for(int x = (int)(pB.min.x - 1); x < (int)(pB.max.x + 1); x++) {
        for(int z = (int)(pB.min.z - 1); z < (int)(pB.max.z + 1); z++) {
            for(int y = (int)(pB.min.y - 1); y < (int)(pB.max.y + 1); y++) {
                
                if(pB.min.x < 0 || pB.min.y < 0 || pB.min.z < 0 || pB.max.x > WORLD_BLOCK_SIZE_X || pB.max.z > WORLD_BLOCK_SIZE_Z) return true;
                
                int blockID = World_GetBlock((Vector3) {x, y, z});
                if(blockID == 0) continue;
                
                BoundingBox blockB;
                blockB.min = (Vector3) {x, y, z};
                blockB.max = (Vector3) {x + 1, y + 1, z + 1};
                
                if(CheckCollisionBoxes(pB, blockB)) return true;
            }
        }
    }
    
    return false;
}