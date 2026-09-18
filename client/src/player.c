/**
 * Copyright (c) 2021 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <limits.h>
#include <stdio.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "player.h"
#include "attachments.h"
#include "playerimpulse.h"
#include "world.h"
#include "raycast.h"
#include "screens.h"
#include "chat.h"
#include "block.h"
#include "networkhandler.h"
#include "packet.h"
#include "particle.h"
#include "entity.h"
#include "inventoryclient.h"

#define MOUSE_SENSITIVITY 0.003f
#define THIRD_PERSON_DISTANCE 4.0f
#define WATER_MOVE_SCALE 0.5f
#define WATER_GRAVITY 0.003f
#define WATER_MAX_FALL_SPEED 0.2f
#define WATER_SWIM_ACCELERATION 0.025f
#define WATER_DRAG 0.8f
#define PLAYER_PHYSICS_STEP (1.0 / 60.0)
#define PLAYER_MAX_CATCHUP_STEPS 6

Vector2 playerOldMousePosition = {0.0f, 0.0f};
Vector2 playerCameraAngle = {0.0f, 0.0f};
double playerLastPositionPacketTime;
Player player;
static float kickPitch, kickRoll, kickDuration;
static double kickStart;
static double physicsAccumulator;
static Vector3 previousPhysicsPosition;
static Vector3 movementInput;
static bool jumpHeld, jumpPending;

static Vector3 Player_RenderPosition(void) {
    if (player.attachment.parent) return player.position;
    float alpha = Clamp((float)(physicsAccumulator / PLAYER_PHYSICS_STEP), 0, 1);
    return Vector3Lerp(previousPhysicsPosition, player.position, alpha);
}

void Player_CameraKick(float pitch, float roll, float duration) {
    if (!isfinite(pitch) || !isfinite(roll) || !isfinite(duration) ||
        fabsf(pitch) > 15 || fabsf(roll) > 15 || duration < 0.01f || duration > 2) return;
    // Replace rapid hits so the view never accumulates an excessive tilt.
    kickPitch = pitch * DEG2RAD;
    kickRoll = roll * DEG2RAD;
    kickDuration = duration;
    kickStart = GetTime();
}

void Player_Init(void) {
    player.attachment = (Attachment){0};
    player.attachmentEpoch = player.controlSession = 0;
    player.controlledEntity = 0;
    kickDuration = 0;
    physicsAccumulator = 0;
    movementInput = (Vector3){0};
    jumpHeld = jumpPending = false;

    Camera camera = { 0 };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 65.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    
    player.camera = camera;
    
    player.velocity = (Vector3) {0, 0, 0};
    player.impulseFlight = false;
    player.waterborne = false;
    player.position = (Vector3) { 0, 80, 0 };
    previousPhysicsPosition = player.position;
    player.speed = 7.5f; // Blocks per second
    
    player.collisionBox.min = (Vector3) { 0.2f, 0, 0.2f };
    player.collisionBox.max = (Vector3) { 0.8f, 1.5f, 0.8f };

    ClientInventory_Reset();
    player.entityType = 0;
    player.modelId = 0;
    player.hasEntityModel = false;
    player.cameraMode = PLAYER_CAMERA_FIRST_PERSON;
    player.flashEnd = 0;
    player.entityModel = (EntityModel){0};
    EntityAnimation_Init(&player.animation, player.position);

    playerLastPositionPacketTime = 0;

    UpdateCamera(&player.camera, CAMERA_CUSTOM);
    DisableCursor();
}

void Player_SetEntityModel(int type, int modelId) {
    if (modelId < 0 || modelId >= 256) return;
    Player_ClearEntityModel();
    player.entityType = (unsigned char)type;
    player.modelId = (unsigned char)modelId;
    EntityModel_CreateTextured(&player.entityModel,modelId,player.textureOverride);
    player.hasEntityModel = true;
}

void Player_ClearEntityModel(void) {
    if (!player.hasEntityModel) return;
    EntityModel_Unload(&player.entityModel);
    EntityModel_Destroy(&player.entityModel);
    player.hasEntityModel = false;
    player.entityType = 0;
}

void Player_Teleport(Vector3 position) {
    physicsAccumulator = 0;
    movementInput = (Vector3){0};
    jumpHeld = jumpPending = false;
    player.position = position;
    previousPhysicsPosition = position;
    player.animation.lastPosition = position;
    player.velocity = (Vector3){0};
    player.impulseFlight = false;
    player.waterborne = false;
    player.canJump = false;

    player.camera.position = position;
    player.camera.position.x += 0.5f;
    player.camera.position.y += 1.5f;
    player.camera.position.z += 0.5f;
}

void Player_Draw(void) {
    if (!player.hasEntityModel) return;

    float pitch = playerCameraAngle.y - PI / 2.0f;

    for (int i = 0; i < player.entityModel.partCount; i++) {
        EntityModelPart *part = &player.entityModel.parts[i];
        if (part->type == PART_TYPE_HEAD) part->rotation.x = pitch;
    }

    Entity localEntity = {0};
    localEntity.type = (char)player.entityType;
    localEntity.modelId = player.modelId;
    localEntity.position = Vector3Add(Player_RenderPosition(), (Vector3){0.5f, 0, 0.5f});
    localEntity.rotation = (Vector3){0, -playerCameraAngle.x + PI / 2.0f, 0};
    localEntity.model = player.entityModel;
    localEntity.flashColor = player.flashColor;
    localEntity.flashEnd = player.flashEnd;
    localEntity.animation = player.animation;
    localEntity.heldBlock = player.blockSelected;
    if (player.cameraMode == PLAYER_CAMERA_FIRST_PERSON) {
        float swingProgress = EntityAnimation_GetSwingProgress(
            &player.animation, ENTITY_ANIMATION_SWING_RIGHT_ARM);
        Entity_DrawFirstPerson(&localEntity, player.camera, swingProgress);
    } else {
        Entity_Draw(&localEntity);
    }
}

void Player_CheckInputs() {
    movementInput = (Vector3){0};
    jumpHeld = false;
    if (screenCursorEnabled) jumpPending = false;
    if (currentScreen != SCREEN_GAME && currentScreen != SCREEN_INVENTORY) {
        jumpPending = false;
        return;
    }
    if (!chatOpen && IsKeyPressed(screenKeys[CONTROL_DEBUG])) {
        screenShowDebug = !screenShowDebug;
    }

    if (!chatOpen && IsKeyPressed(screenKeys[CONTROL_CAMERA])) {
        player.cameraMode = (PlayerCameraMode)((player.cameraMode + 1) % 3);
    }
    
    if ((IsKeyPressed(screenKeys[CONTROL_INVENTORY]) && !chatOpen &&
         (currentScreen == SCREEN_GAME || currentScreen == SCREEN_INVENTORY)) ||
        (IsKeyPressed(KEY_ESCAPE) && currentScreen == SCREEN_INVENTORY)) {
        ClientInventory_Toggle();
    } else if (IsKeyPressed(KEY_ESCAPE)) {
        if (screenCursorEnabled) {
            DisableCursor();
            chatOpen = false;
            Screen_Switch(SCREEN_GAME);
        } else {
            EnableCursor();
            Screen_Switch(SCREEN_PAUSE);
        }
    } else if (IsKeyPressed(screenKeys[CONTROL_CHAT]) && currentScreen != SCREEN_INVENTORY) {
        if (screenCursorEnabled && !chatOpen) {
            DisableCursor();
            screenCursorEnabled = false;
            Screen_Switch(SCREEN_GAME);
        } else {
            chatOpen = true;
            EnableCursor();
            screenCursorEnabled = true;
        }
    }
    
    
    Vector2 mousePositionDelta = { 0.0f, 0.0f };
    Vector2 mousePos = GetMousePosition();
    
    mousePositionDelta.x = mousePos.x - playerOldMousePosition.x;
    mousePositionDelta.y = mousePos.y - playerOldMousePosition.y;
    
    playerOldMousePosition = GetMousePosition();
    
    if (!screenCursorEnabled) {
        playerCameraAngle.x -= (mousePositionDelta.x * -MOUSE_SENSITIVITY * screenSensitivity);
        playerCameraAngle.y -= (mousePositionDelta.y * -MOUSE_SENSITIVITY * screenSensitivity * (screenInvertMouse ? -1 : 1));
        
        //Limit head rotation
        float maxCamAngleY = PI - 0.01f;
        float minCamAngleY = 0.01f;
        
        if (playerCameraAngle.y >= maxCamAngleY) 
            playerCameraAngle.y = maxCamAngleY;
        else if (playerCameraAngle.y <= minCamAngleY) 
            playerCameraAngle.y = minCamAngleY;
    }
    
    
    //Calculate direction vectors of the camera angle
    float cx = cosf(playerCameraAngle.x);
    float sx = sinf(playerCameraAngle.x);
    
    float cx90 = cosf(playerCameraAngle.x + PI / 2);
    float sx90 = sinf(playerCameraAngle.x + PI / 2);
    
    if (!screenCursorEnabled) {
        //Handle keys & mouse
        jumpHeld = IsKeyDown(screenKeys[CONTROL_JUMP]);
        jumpPending = jumpPending || IsKeyPressed(screenKeys[CONTROL_JUMP]);
        Vector3 moveDir = { 0 };
        
        if (IsKeyDown(screenKeys[CONTROL_FORWARD])) {
            moveDir.z += sx;
            moveDir.x += cx;
        }
        
        if (IsKeyDown(screenKeys[CONTROL_BACKWARD])) {
            moveDir.z -= sx;
            moveDir.x -= cx;
        }
        
        if (IsKeyDown(screenKeys[CONTROL_LEFT])) {
            moveDir.z -= sx90;
            moveDir.x -= cx90;
        }
        
        if (IsKeyDown(screenKeys[CONTROL_RIGHT])) {
            moveDir.z += sx90;
            moveDir.x += cx90;
        }

        movementInput = Vector3ClampValue(moveDir, 0.0f, 1.0f);
        if (IsKeyDown(screenKeys[CONTROL_SNEAK]))
            movementInput = Vector3Scale(movementInput, 0.25f);
    }
    if (screenCursorEnabled) jumpPending = false;
}

static void Player_CheckActions(void) {
    if (currentScreen != SCREEN_GAME && currentScreen != SCREEN_INVENTORY) return;
    Vector3 forward = Player_GetForwardVector();
    Vector3 eyePosition = Vector3Add(Player_RenderPosition(), (Vector3){0.5f, 1.5f, 0.5f});
    if (!screenCursorEnabled) {
        float wheel = GetMouseWheelMove();
        if (wheel > 0.35f) ClientInventory_Scroll(-1);
        if (wheel < -0.35f) ClientInventory_Scroll(1);
        for (int slot = 0; slot < INVENTORY_HOTBAR_SLOTS; slot++) {
            if (IsKeyPressed(KEY_ONE + slot)) ClientInventory_Select(slot);
        }
        
        player.rayResult = Raycast_Cast(eyePosition, forward, true);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { //Break Block
            EntityAnimation_Start(&player.animation, ENTITY_ANIMATION_SWING_RIGHT_ARM);
            Network_Send(Packet_CreatePlayerClick(0));
        } else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) { //Place Block
            EntityAnimation_Start(&player.animation, ENTITY_ANIMATION_SWING_RIGHT_ARM);
            Network_Send(Packet_CreatePlayerClick(1));
            ClientInventory_Interact(true, player.rayResult.hitPos, player.rayResult.normal, player.rayResult.hitblockId);
        }
    }

    static double nextDigSwing;
    if (!screenCursorEnabled && IsMouseButtonDown(MOUSE_LEFT_BUTTON) && player.rayResult.hitblockId>0 && GetTime()>=nextDigSwing) {
        EntityAnimation_Start(&player.animation, ENTITY_ANIMATION_SWING_RIGHT_ARM);
        nextDigSwing=GetTime()+0.3;
    }
    ClientInventory_Dig(!screenCursorEnabled && IsMouseButtonDown(MOUSE_LEFT_BUTTON),
        player.rayResult.hitPos, player.rayResult.normal, player.rayResult.hitblockId);
}

static void Player_UpdateCamera(void) {
    Vector3 forward = Player_GetForwardVector();
    Vector3 eyePosition = Vector3Add(Player_RenderPosition(), (Vector3){0.5f, 1.5f, 0.5f});
    player.camera.position = eyePosition;
    if (player.cameraMode != PLAYER_CAMERA_FIRST_PERSON) {
        Vector3 cameraDirection = player.cameraMode == PLAYER_CAMERA_THIRD_PERSON_BEHIND
            ? Vector3Negate(forward)
            : forward;
        Vector3 desiredPosition = Vector3Add(eyePosition, Vector3Scale(cameraDirection, THIRD_PERSON_DISTANCE));
        RaycastResult cameraHit = Raycast_Cast(eyePosition, cameraDirection, true);
        float hitDistance = Vector3Distance(eyePosition, cameraHit.prevPos);
        if (cameraHit.hitblockId != -1 && hitDistance < THIRD_PERSON_DISTANCE) {
            player.camera.position = Vector3Subtract(cameraHit.prevPos, Vector3Scale(cameraDirection, 0.1f));
        } else {
            player.camera.position = desiredPosition;
        }
    }
    player.camera.target = Vector3Add(eyePosition, forward);
    player.camera.up = (Vector3){0, 1, 0};
    float elapsed = (float)(GetTime() - kickStart);
    if (kickDuration > 0 && elapsed >= 0 && elapsed < kickDuration) {
        float t = elapsed / kickDuration;
        float weight = sinf(PI * t) * (1 - t);
        Vector3 view = Vector3Subtract(player.camera.target, player.camera.position);
        Vector3 right = Vector3Normalize(Vector3CrossProduct(view, player.camera.up));
        view = Vector3RotateByAxisAngle(view, right, kickPitch * weight);
        Vector3 up = Vector3RotateByAxisAngle(player.camera.up, right, kickPitch * weight);
        player.camera.up = Vector3RotateByAxisAngle(up, Vector3Normalize(view), kickRoll * weight);
        player.camera.target = Vector3Add(player.camera.position, view);
    }
}



void Player_ApplyImpulse(Vector3 impulse) {
    if (player.attachment.parent) return;
    if (!PlayerImpulse_Valid(impulse)) return;
    player.velocity = PlayerImpulse_Add(player.velocity, impulse);
    if (impulse.x != 0 || impulse.y != 0 || impulse.z != 0) player.impulseFlight = true;
    if (impulse.y > 0) player.canJump = false;
}

static void Player_PhysicsStep(void) {
    player.liquidSubmersion = Player_GetLiquidSubmersion();
    if (player.liquidSubmersion > 0) player.waterborne = true;

    if (player.liquidSubmersion > 0.0f) {
        player.velocity.y -= WATER_GRAVITY;
        if (player.velocity.y < -WATER_MAX_FALL_SPEED) {
            player.velocity.y = -WATER_MAX_FALL_SPEED;
        }
    } else {
        player.velocity.y -= 0.012f;
        if (player.velocity.y <= -1) player.velocity.y = -1;
    }
    
    // Velocity is stored in blocks per fixed 60 Hz tick.
    Vector3 velXdt = player.velocity;
    
    int steps = 8;
    
    //Move Y & Test Collisions
    for (int i = 0; i < steps; i++) {
        player.position.y += velXdt.y / steps;
        if (Player_TestCollision((Vector3){ 0 })) {
            player.position.y -= velXdt.y / steps;
            if (player.velocity.y <= 0) player.canJump = true;
            player.velocity.y = 0;
            break;
        } else {
            player.canJump = false;
        }
    }

    //Move X & Test Collisions
    for (int i = 0; i < steps; i++) {
        player.position.x += velXdt.x / steps;
        if (Player_TestCollision((Vector3){ 0 })) {
            if (player.velocity.y != 0 || Player_TestCollision((Vector3){0,0.51f,0})) {
                player.position.x -= velXdt.x / steps;
                if (player.impulseFlight) player.velocity.x = 0;
            } else {
                player.position.y += 0.1f / steps;
            }
        }
    }

    //Move Z & Test Collisions
    for (int i = 0; i < steps; i++) {
        player.position.z += velXdt.z / steps;
        if (Player_TestCollision((Vector3){ 0 })) {
            if (player.velocity.y != 0 || Player_TestCollision((Vector3){0,0.51f,0})) {
                player.position.z -= velXdt.z / steps;
                if (player.impulseFlight) player.velocity.z = 0;
            } else {
                player.position.y += 0.1f / steps;
                break;
            }
        }
    }

    if (player.canJump || player.liquidSubmersion > 0.0f) player.impulseFlight = false;
    if (player.canJump && Player_GetLiquidSubmersion() == 0) player.waterborne = false;
    if (player.liquidSubmersion > 0.0f) player.velocity.y *= WATER_DRAG;
    // Briefly clearing the surface must not grant land acceleration or reduce drag.
    if (player.waterborne && !player.impulseFlight) {
        player.velocity.x *= WATER_DRAG;
        player.velocity.z *= WATER_DRAG;
    } else if (!player.impulseFlight) {
        player.velocity.x -= player.velocity.x / 6.0f;
        player.velocity.z -= player.velocity.z / 6.0f;
    }
    if (jumpHeld || jumpPending) {
        if (player.liquidSubmersion > 0.0f) {
            player.velocity.y = fminf(0.2f, player.velocity.y + WATER_SWIM_ACCELERATION);
        } else if (player.canJump) {
            player.velocity.y += 0.2f;
            player.canJump = false;
        }
    }
    jumpPending = false;
    // Balance the 1/6 drag per tick while exposing speed in blocks/second.
    Vector3 moveVel = Vector3Scale(movementInput, player.speed / 60.0f / 6.0f);
    if (player.waterborne) moveVel = Vector3Scale(moveVel, WATER_MOVE_SCALE);
    if (!player.impulseFlight) player.velocity = Vector3Add(player.velocity, moveVel);
}

static void Player_AdvancePhysics(float dt) {
    if (player.attachment.parent) {
        physicsAccumulator = 0; player.velocity = movementInput = (Vector3){0};
        player.waterborne = false;
        jumpHeld = jumpPending = false;
        return;
    }
    if (!isfinite(dt) || dt <= 0) return;
    physicsAccumulator += fmin((double)dt, PLAYER_PHYSICS_STEP * PLAYER_MAX_CATCHUP_STEPS);
    int steps = 0;
    while (physicsAccumulator >= PLAYER_PHYSICS_STEP && steps < PLAYER_MAX_CATCHUP_STEPS) {
        physicsAccumulator -= PLAYER_PHYSICS_STEP;
        previousPhysicsPosition = player.position;
        Player_PhysicsStep();
        steps++;
    }
}

void Player_Update(void) {
    player.camera.fovy = screenFOV;
    ClientInventory_Update();
    Player_CheckInputs();
    if (!player.attachment.parent) Player_AdvancePhysics(GetFrameTime());
    else {
        physicsAccumulator = 0; movementInput = (Vector3){0};
        player.waterborne = false;
        jumpHeld = jumpPending = false; player.velocity = (Vector3){0};
        ClientAttachments_Update();
    }
    ClientControl_Update();
    World_LoadChunks();
    Player_CheckActions();
    Player_UpdateCamera();
    if(GetTime() - playerLastPositionPacketTime > 0.05) {
        Network_Send(Packet_CreatePlayerPosition((Vector3) { player.position.x + 0.5f, player.position.y, player.position.z + 0.5f }, (Vector3) {playerCameraAngle.y - PI / 2, -playerCameraAngle.x + PI / 2, 0}));
        playerLastPositionPacketTime = GetTime();
    }
    EntityAnimation_Update(&player.animation, Player_RenderPosition(), GetFrameTime());
    if (player.attachment.parent) player.animation.walkAmount = player.animation.walkSpeed = 0;
    EntityAnimation_UpdatePose(&player.animation, GetFrameTime());
}

bool Player_TestCollision(Vector3 offset) {
    
    BoundingBox pB = player.collisionBox;
    pB.min = Vector3Add(Vector3Add(pB.min, player.position), offset);
    pB.max = Vector3Add(Vector3Add(pB.max, player.position), offset);
    
    for (int x = (int)(pB.min.x - 1); x < (int)(pB.max.x + 1); x++) {
        for (int z = (int)(pB.min.z - 1); z < (int)(pB.max.z + 1); z++) {
            for (int y = (int)(pB.min.y - 1); y < (int)(pB.max.y + 1); y++) {
                Vector3 blockPos = (Vector3) {x, y, z};

                Vector3 chunkPos = (Vector3) { floor(blockPos.x / CHUNK_SIZE_X), floor(blockPos.y / CHUNK_SIZE_Y), floor(blockPos.z / CHUNK_SIZE_Z) };
                Chunk* chunk = World_GetChunkAt(chunkPos);
                if (chunk == NULL || chunk->isBlockDataReady == false) return true;

                int blockId = World_GetBlock(blockPos);
                const Block *blockDef = Block_GetDefinition(blockId);
                if (blockDef->colliderType != BLOCK_COLLIDER_SOLID) continue;
                
                for(int box=0;box<Block_BoxCount(blockDef,false);box++) {
                    BoundingBox blockB=Block_GetBox(blockDef,box,blockPos,false);
                    if(CheckCollisionBoxes(pB,blockB)) return true;
                }
            }
        }
    }
    
    return false;
}

float Player_GetLiquidSubmersion(void) {
    BoundingBox playerBox = player.collisionBox;
    playerBox.min = Vector3Add(playerBox.min, player.position);
    playerBox.max = Vector3Add(playerBox.max, player.position);

    float highestLiquidSurface = -INFINITY;
    for (int x = (int)floorf(playerBox.min.x); x <= (int)floorf(playerBox.max.x - 0.001f); x++) {
        for (int z = (int)floorf(playerBox.min.z); z <= (int)floorf(playerBox.max.z - 0.001f); z++) {
            for (int y = (int)floorf(playerBox.min.y); y <= (int)floorf(playerBox.max.y - 0.001f); y++) {
                Vector3 blockPosition = {(float)x, (float)y, (float)z};
                Vector3 chunkPosition = {
                    floorf(blockPosition.x / CHUNK_SIZE_X),
                    floorf(blockPosition.y / CHUNK_SIZE_Y),
                    floorf(blockPosition.z / CHUNK_SIZE_Z)
                };
                Chunk *chunk = World_GetChunkAt(chunkPosition);
                if (chunk == NULL || !chunk->isBlockDataReady) continue;

                const Block *block = Block_GetDefinition(World_GetBlock(blockPosition));
                if (block->colliderType != BLOCK_COLLIDER_LIQUID) continue;

                BoundingBox liquidBox = {
                    .min = {x + block->minBB.x / 16.0f, y + block->minBB.y / 16.0f,
                            z + block->minBB.z / 16.0f},
                    .max = {x + block->maxBB.x / 16.0f, y + block->maxBB.y / 16.0f,
                            z + block->maxBB.z / 16.0f}
                };
                if (CheckCollisionBoxes(playerBox, liquidBox) && liquidBox.max.y > highestLiquidSurface) {
                    highestLiquidSurface = liquidBox.max.y;
                }
            }
        }
    }

    if (highestLiquidSurface == -INFINITY) return 0.0f;
    float playerHeight = playerBox.max.y - playerBox.min.y;
    return Clamp((highestLiquidSurface - playerBox.min.y) / playerHeight, 0.0f, 1.0f);
}

bool Player_GetCameraLiquidTint(Color *tint) {
    Vector3 blockPosition = {
        floorf(player.camera.position.x),
        floorf(player.camera.position.y),
        floorf(player.camera.position.z)
    };
    Vector3 chunkPosition = {
        floorf(blockPosition.x / CHUNK_SIZE_X),
        floorf(blockPosition.y / CHUNK_SIZE_Y),
        floorf(blockPosition.z / CHUNK_SIZE_Z)
    };
    Chunk *chunk = World_GetChunkAt(chunkPosition);
    if (chunk == NULL || !chunk->isBlockDataReady) return false;

    int blockId = World_GetBlock(blockPosition);
    const Block *block = Block_GetDefinition(blockId);
    if (block->colliderType != BLOCK_COLLIDER_LIQUID) return false;

    float liquidSurface = blockPosition.y + block->maxBB.y / 16.0f;
    if (player.camera.position.y >= liquidSurface) return false;
    if (tint != NULL) *tint = block->liquidTint;
    return true;
}


Vector3 Player_GetForwardVector(void) {
    float cx = cosf(playerCameraAngle.x);
    float sx = sinf(playerCameraAngle.x);
    
    float sy = sinf(playerCameraAngle.y);
    float cy = cosf(playerCameraAngle.y);
    
    return (Vector3) {cx * sy, cy, sx * sy};
}

Vector3 Player_GetChunkPosition(void) {
    return (Vector3) {(int)floor(player.position.x / CHUNK_SIZE_X), (int)floor(player.position.y / CHUNK_SIZE_Y), (int)floor(player.position.z / CHUNK_SIZE_Z)};
}

Vector3 Player_GetRotation(void) {
    return (Vector3){playerCameraAngle.y-PI/2,-playerCameraAngle.x+PI/2,0};
}
void Player_SetAttachedPosition(Vector3 position, Vector3 rotation) {
    player.position=previousPhysicsPosition=position;
    player.attachedRotation=rotation;
    if(player.attachment.inheritRotation) {
        playerCameraAngle.y=rotation.x+PI/2;
        playerCameraAngle.x=-rotation.y+PI/2;
    }
}
void Player_RefreshAttachedCamera(void) { Player_UpdateCamera(); }
