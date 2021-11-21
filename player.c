/**
 * Copyright (c) 2021 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <limits.h>
#include <stdio.h>
#include "raylib.h"
#include "raymath.h"
#include "player.h"
#include "math.h"
#include "world.h"
#include "raycast.h"
#include "screens.h"
#include "networkhandler.h"
#include "packet.h"
#include "chat.h"

#define MOUSE_SENSITIVITY 0.003f

Vector2 Player_oldMousePos = {0.0f, 0.0f};
Vector2 Player_cameraAngle = {0.0f, 0.0f};
Player player;

void Player_Init() {

    Camera camera = { 0 };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 70.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    
    player.camera = camera;
    
    player.velocity = (Vector3) {0, 0, 0};
    player.position = (Vector3) { world.size.x / 2, 128.0f, world.size.z / 2 };
    player.speed = 0.125f / 6;
    
    player.collisionBox.min = (Vector3) { 0, 0, 0 };
    player.collisionBox.max = (Vector3) { 0.8f, 1.8f, 0.8f };

    player.blockSelected = 1;

    SetCameraMode(player.camera, CAMERA_CUSTOM);
    DisableCursor();
}

void Player_CheckInputs() {
    
    if(world.loaded == INT_MAX) return;

    if(IsKeyPressed(KEY_ESCAPE)) {
        if(Screen_cursorEnabled) {
            DisableCursor();
            Chat_open = false;
            Screen_Switch(SCREEN_GAME);
        } else {
            EnableCursor();
            Screen_Switch(SCREEN_PAUSE);
        }
        Screen_cursorEnabled = !Screen_cursorEnabled;
    } else if(IsKeyPressed(KEY_T)) {
        if(Screen_cursorEnabled && !Chat_open) {
            DisableCursor();
            Screen_cursorEnabled = false;
            Screen_Switch(SCREEN_GAME);
        } else {
            Chat_open = true;
            EnableCursor();
            Screen_cursorEnabled = true;
        }
    }
    
    
    Vector2 mousePositionDelta = { 0.0f, 0.0f };
    Vector2 mousePos = GetMousePosition();
    
    mousePositionDelta.x = mousePos.x - Player_oldMousePos.x;
    mousePositionDelta.y = mousePos.y - Player_oldMousePos.y;
    
    Player_oldMousePos = GetMousePosition();
    
    if(!Screen_cursorEnabled) {
        Player_cameraAngle.x -= (mousePositionDelta.x * -MOUSE_SENSITIVITY);
        Player_cameraAngle.y -= (mousePositionDelta.y * -MOUSE_SENSITIVITY);
        
        //Limit head rotation
        float maxCamAngleY = PI - 0.01f;
        float minCamAngleY = 0.01f;
        
        if(Player_cameraAngle.y >= maxCamAngleY) 
            Player_cameraAngle.y = maxCamAngleY;
        else if(Player_cameraAngle.y <= minCamAngleY) 
            Player_cameraAngle.y = minCamAngleY;
    }
    
    
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
    
    if(!Screen_cursorEnabled) {
        //Handle keys & mouse
        if(IsKeyDown(KEY_SPACE) && player.canJump) {
            player.velocity.y += 0.2f;
            player.canJump = false;
        }
        
        if(IsKeyDown(KEY_W)) {
            player.velocity.z += sx * player.speed;
            player.velocity.x += cx * player.speed;
        }
        
        if(IsKeyDown(KEY_S)) {
           player.velocity.z -= sx * player.speed;
           player.velocity.x -= cx * player.speed;
        }
        
        if(IsKeyDown(KEY_A)) {
           player.velocity.z -= sx90 * player.speed;
           player.velocity.x -= cx90 * player.speed;
        }
        
        if(IsKeyDown(KEY_D)) {
           player.velocity.z += sx90 * player.speed;
           player.velocity.x += cx90 * player.speed;
        }
        
        float wheel = GetMouseWheelMove();
        if(wheel > 0.35f) player.blockSelected++;
        if(wheel < -0.35f) player.blockSelected--;
        if(player.blockSelected > 18) player.blockSelected = 1;
        if(player.blockSelected < 1) player.blockSelected = 18;
        
        player.rayResult = Raycast_Do(player.camera.position, (Vector3) { forwardX, forwardY, forwardZ});

        if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { //Break Block
            if(player.rayResult.hitBlockID != -1) {
                World_SetBlock(player.rayResult.hitPos, 0);
                Network_Send(Packet_SetBlock(0, player.rayResult.hitPos));
            }
        } else if(IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) { //Place Block
            if(player.rayResult.hitBlockID != -1) {
                World_SetBlock(player.rayResult.prevPos, player.blockSelected);
                Network_Send(Packet_SetBlock(player.blockSelected, player.rayResult.prevPos));
            }
        } else if(IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) { //Pick Block
            int pickedID = World_GetBlock(player.rayResult.hitPos);
            player.blockSelected = pickedID;
        }
    }
    //Place camera's target to the direction looking at.
    player.camera.target.x = player.camera.position.x + forwardX;
    player.camera.target.y = player.camera.position.y + forwardY;
    player.camera.target.z = player.camera.position.z + forwardZ;
    
    UpdateCamera(&player.camera);
}

void Player_Update() {
    
    if(world.loaded == INT_MAX) return;
    
    Network_Send(Packet_PlayerPosition(player.position));

    //Gravity
    player.velocity.y -= 0.012f * (GetFrameTime() * 60);
    
    //Calculate velocity with delta time
    Vector3 velXdt = Vector3Scale(player.velocity, GetFrameTime() * 60);
    
    //Move X & Test Collisions
    player.position.x += velXdt.x;
    if(Player_TestCollision()) player.position.x -= velXdt.x;

    //Move Y & Test Collisions
    player.position.y += velXdt.y;
    if(Player_TestCollision()) {
        player.position.y -= velXdt.y;
        if(player.velocity.y <= 0) player.canJump = true;
        player.velocity.y = 0;
    }

    //Move Z & Test Collisions
    player.position.z += velXdt.z;
    if(Player_TestCollision()) player.position.z -= velXdt.z;

    //Place Camera
    player.camera.position = player.position;
    player.camera.position.y += 1.8f;
    player.camera.position.x += 0.4f;
    player.camera.position.z += 0.4f;

    player.velocity.x -= velXdt.x / 6.0f;
    player.velocity.z -=  velXdt.z / 6.0f;
    
    Player_CheckInputs(); 
}

bool Player_TestCollision() {
    
    BoundingBox pB = player.collisionBox;
    pB.min = Vector3Add(pB.min, player.position);
    pB.max = Vector3Add(pB.max, player.position);
    
    for(int x = (int)(pB.min.x - 1); x < (int)(pB.max.x + 1); x++) {
        for(int z = (int)(pB.min.z - 1); z < (int)(pB.max.z + 1); z++) {
            for(int y = (int)(pB.min.y - 1); y < (int)(pB.max.y + 1); y++) {
                
                if(pB.min.x < 0 || pB.min.y < 0 || pB.min.z < 0 || pB.max.x > world.size.x || pB.max.z > world.size.z) return true;
                
                int blockID = World_GetBlock((Vector3) {x, y, z});
                Block blockDef = Block_definition[blockID];
                if(blockDef.colliderType != BlockColliderType_Solid) continue;
                
                BoundingBox blockB;
                blockB.min = (Vector3) {x + (blockDef.minBB.x / 16), y + (blockDef.minBB.y / 16), z + (blockDef.minBB.z / 16)};
                blockB.max = (Vector3) {x + (blockDef.maxBB.x / 16), y + (blockDef.maxBB.y / 16), z + (blockDef.maxBB.z / 16)};
                
                if(CheckCollisionBoxes(pB, blockB)) return true;
            }
        }
    }
    
    return false;
}

Vector3 Player_GetForwardVector(void) {
    float cx = cosf(Player_cameraAngle.x);
    float sx = sinf(Player_cameraAngle.x);
    
    float sy = sinf(Player_cameraAngle.y);
    float cy = cosf(Player_cameraAngle.y);
    
    return (Vector3) {cx * sy, cy, sx * sy};
}