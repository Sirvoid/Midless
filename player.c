#include "raylib.h"
#include "player.h"
#include "math.h"
#include "world.h"
#include "raycast.h"

Vector2 Player_oldMousePos = {0.0f, 0.0f};
Vector2 Player_cameraAngle = {0.0f, 0.0f};

void Player_Init(Player *player) {

    Camera camera = { 0 };
    camera.position = (Vector3){ -1.0f, 14.0f, -1.0f };
    camera.target = (Vector3){ -1.0f, 1.8f, -1.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    player->camera = camera;

    SetCameraMode(player->camera, CAMERA_CUSTOM);
    DisableCursor();
}

void Player_Update(Player *player) {
    
    Vector2 mousePositionDelta = {0.0f, 0.0f};
    Vector2 mousePos = GetMousePosition();
    
    mousePositionDelta.x = mousePos.x - Player_oldMousePos.x;
    mousePositionDelta.y = mousePos.y - Player_oldMousePos.y;
    
    Player_oldMousePos = GetMousePosition();
    
    Player_cameraAngle.x -= (mousePositionDelta.x * -0.003f);
    Player_cameraAngle.y -= (mousePositionDelta.y * -0.003f);
    
    float maxCamAngleY = PI - 0.01f;
    float minCamAngleY = 0.01f;
    
    if(Player_cameraAngle.y >= maxCamAngleY) Player_cameraAngle.y = maxCamAngleY;
    if(Player_cameraAngle.y <= minCamAngleY) Player_cameraAngle.y = minCamAngleY;
    
    float cx = cosf(Player_cameraAngle.x);
    float sx = sinf(Player_cameraAngle.x);
    
    float cxS = cosf(Player_cameraAngle.x + PI / 2);
    float sxS = sinf(Player_cameraAngle.x + PI / 2);
    
    float sy = sinf(Player_cameraAngle.y);
    float cy = cosf(Player_cameraAngle.y);
    
    float forwardX = cx * sy;
    float forwardY = cy;
    float forwardZ = sx * sy;
    
    if(IsKeyDown(KEY_SPACE)) {
        player->camera.position.y++;
    } else if(IsKeyDown(KEY_LEFT_SHIFT)) {
        player->camera.position.y--;
    }
    
    if(IsKeyDown(KEY_W)) {
        player->camera.position.z += sx;
        player->camera.position.x += cx;
    }
    
    if(IsKeyDown(KEY_S)) {
       player->camera.position.z -= sx;
       player->camera.position.x -= cx;
    }
    
    if(IsKeyDown(KEY_A)) {
       player->camera.position.z -= sxS;
       player->camera.position.x -= cxS;
    }
    
    if(IsKeyDown(KEY_D)) {
       player->camera.position.z += sxS;
       player->camera.position.x += cxS;
    }
    
    if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        RaycastResult rayResult = Raycast_Do(player->camera.position, (Vector3) { forwardX, forwardY, forwardZ});
        World_SetBlock(rayResult.hitPos, 0);
    } else if(IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        RaycastResult rayResult = Raycast_Do(player->camera.position, (Vector3) { forwardX, forwardY, forwardZ});
        World_SetBlock(rayResult.prevPos, 4);
    }
 
    player->camera.target.x = player->camera.position.x + forwardX;
    player->camera.target.z = player->camera.position.z + forwardZ;
    player->camera.target.y = player->camera.position.y + forwardY;
    
    UpdateCamera(&player->camera);
}