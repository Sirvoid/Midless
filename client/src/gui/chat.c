/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <raylib.h>
#include <raygui.h>

#include "chat.h"
#include "screens.h"
#include "../player.h"
#include "../networking/networkhandler.h"
#include "../networking/packet.h"

char* chatLines[64];
int currentLine = 0;

void Chat_AddLine(char *line) {
    if(chatLines[currentLine]) MemFree(chatLines[currentLine]);
    chatLines[currentLine++] = line;
    if(currentLine >= 64) {
        currentLine = 0;
    }
}


char Chat_input[64] = "";
bool Chat_editMode = false;
bool Chat_open = false;

void Chat_Draw(Vector2 offset, Color uiColor) {

    if(!Network_connectedToServer) return;

    int chatWidth = 352;
    int fontSize = 10;

    //Draw Background
    if(Chat_editMode) DrawRectangle(offset.x, offset.y - 184 + 46, chatWidth, 184, uiColor);

    //Draw Lines
    int lineAdded = 0;

    int index = currentLine;
    while(lineAdded < 13) {
        if(chatLines[index]) {
            int textLength = TextLength(chatLines[index]);
            int startPos = 0;
            char *drawLines[3];
            int drawLinesCnt = 0;
            for(int i = 0; i < textLength; i++) {
                const char* sub = TextSubtext(chatLines[index], startPos, i - startPos + 1);
                int textWidth = MeasureText(sub, fontSize);
                if(textWidth >= chatWidth - fontSize - 4 || i == textLength - 1) {
                    drawLines[drawLinesCnt] = MemAlloc(64);
                    TextCopy(drawLines[drawLinesCnt], sub);
                    drawLinesCnt++;
                    startPos = i;
                }
            }
            for(int i = drawLinesCnt - 1; i >= 0; i--) {
                if(!drawLines[i]) continue;
                DrawText(drawLines[i], offset.x + 4, offset.y - lineAdded * fontSize, fontSize, WHITE);
                lineAdded++;
            }
        }

        index--;
        if(index < 0) index = 63;
        if(index == currentLine) break;
    }

    //Chat input
    if(Chat_editMode) GuiTextBox((Rectangle) { offset.x, offset.y + 22, chatWidth, 24 }, Chat_input, 64, Chat_editMode);
    
    if(IsKeyPressed(KEY_ENTER)) {
        if(Chat_open) {
            char *message = MemAlloc(64);
            for(int i = 0; i < 64; i++) {
                message[i] = Chat_input[i];
                Chat_input[i] = '\0';
            }
            Network_Send(Packet_SendMessage(message));
            DisableCursor();
            Chat_open = false;
            Screen_cursorEnabled = false;
        }
    }

    if(Chat_open) {
        Chat_editMode = true;
    } else {
        Chat_editMode = false;
    }

}