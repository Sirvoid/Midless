#include "raylib.h"
#include "player.h"

void Player_Init(Player *player) {

    Camera camera = { 0 };
    camera.position = (Vector3){ -1.0f, 16.0f, -1.0f };
    camera.target = (Vector3){ 0.0f, 1.8f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    player->camera = camera;

    SetCameraMode(player->camera, CAMERA_FIRST_PERSON);
}