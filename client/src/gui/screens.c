/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#define RAYGUI_IMPLEMENTATION
#define RAYGUI_SUPPORT_ICONS
#include <pthread.h>
#include <math.h>
#include <raylib.h>
#include <raygui.h>
#include "screens.h"
#include "chat.h"
#include "../player.h"
#include "../world.h"
#include "../block/block.h"
#include "../networking/networkhandler.h"
#include "../networking/client.h"
#include "../worldgenerator.h"

Screen Screen_Current = SCREEN_LOGIN;
bool Screen_cursorEnabled = false;
bool Screen_showDebug = false;
int screenHeight;
int screenWidth;
bool *exitGame;
Color uiColBg;

Texture2D mapTerrain;

void Screens_init(Texture2D terrain, bool *exit) {
    mapTerrain = terrain;
    exitGame = exit;

    //Set UI colors
    GuiSetStyle(2,0,0xfffcfcff); //BUTTON_BORDER_COLOR_NORMAL 
    GuiSetStyle(2,1,0x00000000); //BUTTON_BASE_COLOR_NORMAL 
    GuiSetStyle(2,2,0xffffffff); //BUTTON_TEXT_COLOR_NORMAL 
    GuiSetStyle(2,3,0x010101ff); //BUTTON_BORDER_COLOR_FOCUSED 
    GuiSetStyle(2,4,0xfafafa00); //BUTTON_BASE_COLOR_FOCUSED 
    GuiSetStyle(2,5,0x000000ff); //BUTTON_TEXT_COLOR_FOCUSED 
    GuiSetStyle(2,6,0xfcffffff); //BUTTON_BORDER_COLOR_PRESSED 
    GuiSetStyle(2,7,0x00000000); //BUTTON_BASE_COLOR_PRESSED 
    GuiSetStyle(2,8,0xffffffff); //BUTTON_TEXT_COLOR_PRESSED 
    GuiSetStyle(5,0,0xfffdfdff); //PROGRESSBAR_BORDER_COLOR_NORMAL 
    GuiSetStyle(5,6,0xfbf8f8ff); //PROGRESSBAR_BORDER_COLOR_PRESSED 
    GuiSetStyle(5,7,0xf8fbfbff); //PROGRESSBAR_BASE_COLOR_PRESSED 
    GuiSetStyle(9,0,0xf9f9f9ff); //TEXTBOX_BORDER_COLOR_NORMAL 
    GuiSetStyle(9,1,0xfbfbfbff); //TEXTBOX_BASE_COLOR_NORMAL 
    GuiSetStyle(9,2,0xfdf9f9ff); //TEXTBOX_TEXT_COLOR_NORMAL 
    GuiSetStyle(9,4,0xc7effeff); //TEXTBOX_BASE_COLOR_FOCUSED 
    GuiSetStyle(9,6,0x0392c7ff); //TEXTBOX_BORDER_COLOR_PRESSED 
    GuiSetStyle(9,8,0x338bafff); //TEXTBOX_TEXT_COLOR_PRESSED 
}

void Screen_MakeGame(void) {

    //Draw debug infos
    const char* coordText = TextFormat("X: %i Y: %i Z: %i", (int)player.position.x, (int)player.position.y, (int)player.position.z);
    const char* debugText;

    if(Screen_showDebug) {
        if(Network_connectedToServer) {
            debugText = TextFormat("%2i FPS %2i PING", GetFPS(), Network_ping);
        } else {
            debugText = TextFormat("%2i FPS", GetFPS());
        }
    
        int backgroundWidth = fmax(MeasureText(coordText, 20), MeasureText(debugText, 20));

        DrawRectangle(13, 15, backgroundWidth + 6, 39, uiColBg);
        DrawText(debugText, 16, 16, 20, WHITE);
        DrawText(coordText, 16, 36, 20, WHITE);
    }

    //Draw crosshair
    DrawRectangle(screenWidth / 2 - 8, screenHeight / 2 - 2, 16, 4, uiColBg);
    DrawRectangle(screenWidth / 2 - 2, screenHeight / 2 + 2,  4, 6, uiColBg);
    DrawRectangle(screenWidth / 2 - 2, screenHeight / 2 - 8,  4, 6, uiColBg);

    //Draw Block Selected
    Block blockDef = Block_GetDefinition(player.blockSelected);
    int texI = blockDef.textures[4];
    int texX = texI % 16 * 16;
    int texY = texI / 16 * 16;

    Rectangle texRec = (Rectangle) {
        texX, 
        texY, 
        texX + 16, 
        texY + 16
    };

    Rectangle destRec = (Rectangle) { 
        screenWidth - 80 - (blockDef.minBB.x * 4), 
        16 + ((16 - blockDef.maxBB.y) * 4), 
        (blockDef.maxBB.x - blockDef.minBB.x) * 4, 
        (blockDef.maxBB.y - blockDef.minBB.y) * 4
    };

    DrawTextureTiled(mapTerrain, texRec, destRec, (Vector2) {0, 0}, 0, 4, WHITE);

    //Draw Chat
    Chat_Draw((Vector2){16, screenHeight - 52}, uiColBg);
}

void Screen_MakePause(void) {
    DrawRectangle(0, 0, screenWidth, screenHeight, uiColBg);

    int offsetY = screenHeight / 2 - 75;
    int offsetX = screenWidth / 2 - 100;

    int index = 0;

    //Continue Button
    if(GuiButton((Rectangle) {offsetX , offsetY + (index++ * 35), 200, 30 }, "Continue")) {
        Screen_Switch(SCREEN_GAME);
        DisableCursor();
        Screen_cursorEnabled = false;
    }

    //Options Button
    if(GuiButton((Rectangle) {offsetX, offsetY + (index++ * 35), 200, 30 }, "Options")) {
        Screen_Switch(SCREEN_OPTIONS);
    }

    //Main Menu Button
    if(GuiButton((Rectangle) {offsetX, offsetY + (index++ * 35), 200, 30 }, "Main Menu")) {
        Screen_Switch(SCREEN_LOGIN);
        Network_threadState = -1; //End network thread
        Screen_cursorEnabled = false;
        World_Unload();
    }

    //Quit Button
    if(GuiButton((Rectangle) {offsetX, offsetY + (index++ * 35), 200, 30 }, "Quit")) {
        *exitGame = true;
    }
}

void Screen_MakeOptions(void) {
    DrawRectangle(0, 0, screenWidth, screenHeight, uiColBg);

    int offsetY = screenHeight / 2 - 30;
    int offsetX = screenWidth / 2 - 100;

    const char* drawDistanceTxt = TextFormat("Draw Distance: %i", world.drawDistance);

    //Draw distance Button
    if(GuiButton((Rectangle) {offsetX, offsetY - 15, 200, 30 }, drawDistanceTxt)) {
        world.drawDistance += 1;
        if(world.drawDistance > 6) {
            world.drawDistance = 2;
            World_Reload();
        }
    }

    //Draw Debug Button
    const char* debugStateTxt = "OFF";
    if(Screen_showDebug) debugStateTxt = "ON";
    const char* showDebugTxt = TextFormat("Show Debug: %s", debugStateTxt);
    if(GuiButton((Rectangle) {offsetX, offsetY + 20, 200, 30 }, showDebugTxt)) {
        Screen_showDebug = !Screen_showDebug;
    }

    //Back Button
    if(GuiButton((Rectangle) {offsetX, offsetY + 55, 200, 30 }, "Back")) {
        Screen_Switch(SCREEN_PAUSE);
    }

}

void Screen_MakeJoining(void) {
    DrawRectangle(0, 0, screenWidth, screenHeight, BLACK);
    DrawText("Joining Server...", screenWidth / 2 - 80, screenHeight / 2 - 30, 20, WHITE);
}

char name_input[16] = "Player";
char ip_input[16] = "127.0.0.1";
char port_input[5] = "25565";

bool login_editMode = false;
bool ip_editMode = false;
bool port_editMode = false;

void Screen_MakeLogin(void) {
    EnableCursor();
    DrawRectangle(0, 0, screenWidth, screenHeight, BLACK);

    const char *title = "MIDLESS";
    int offsetY = screenHeight / 2;
    int offsetX = screenWidth / 2;

    DrawText(title, offsetX - (MeasureText(title, 80) / 2), offsetY - 100, 80, WHITE);

    //Name Input
    if(GuiTextBox((Rectangle) { offsetX - 80, offsetY - 15, 160, 30 }, name_input, 16, login_editMode)) {
        login_editMode = !login_editMode;
    }

    //IP Input
    if(GuiTextBox((Rectangle) { offsetX - 80, offsetY + 20, 116, 30 }, ip_input, 16, ip_editMode)) {
        ip_editMode = !ip_editMode;
    }

    //Port Input
    if(GuiTextBox((Rectangle) { offsetX + 40, offsetY + 20, 40, 30 }, port_input, 5, port_editMode)) {
        port_editMode = !port_editMode;
    }

    //Login button
    if(GuiButton((Rectangle) { offsetX - 80, offsetY + 55, 160, 30 }, "Login")) {
        DisableCursor();
        Screen_Switch(SCREEN_JOINING);
        Network_threadState = 0;
        Network_name = name_input;
        Network_ip = ip_input;
        Network_port = TextToInteger(port_input);

        //Start Client on a new thread
        pthread_t clientThread_id;
        pthread_create(&clientThread_id, NULL, Client_Init, (void*)&Network_threadState);
    }
    
    //Singleplayer Button
    if(GuiButton((Rectangle) { offsetX - 80, offsetY + 90, 160, 30 }, "Singleplayer")) {
        DisableCursor();
        World_LoadSingleplayer();
    }

}

void Screen_Make(void) {
    screenHeight = GetScreenHeight();
    screenWidth = GetScreenWidth();
    
    uiColBg = (Color){ 0, 0, 0, 80 };
    
    if(Screen_Current == SCREEN_GAME)
        Screen_MakeGame();
    else if(Screen_Current == SCREEN_PAUSE)
        Screen_MakePause();
    else if(Screen_Current == SCREEN_JOINING)
        Screen_MakeJoining();
    else if(Screen_Current == SCREEN_LOGIN)
        Screen_MakeLogin();
    else if(Screen_Current == SCREEN_OPTIONS)
        Screen_MakeOptions();
}

void Screen_Switch(Screen screen) {
    Screen_Current = screen;
}