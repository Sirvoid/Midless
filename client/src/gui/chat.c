/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"
#include "raygui.h"
#include "chat.h"
#include "screens.h"
#include "player.h"
#include "networkhandler.h"
#include "packet.h"
#include "formattedtext.h"

char* chatLines[64];
int currentLine = 0;

void Chat_AddOwnedLine(char *line) {
    if (chatLines[currentLine]) MemFree(chatLines[currentLine]);
    chatLines[currentLine++] = line;
    if (currentLine >= 64) {
        currentLine = 0;
    }
}

void Chat_Shutdown(void) {
    for (int i = 0; i < 64; i++) {
        MemFree(chatLines[i]);
        chatLines[i] = NULL;
    }
    currentLine = 0;
}

char chatInput[64] = "";
bool chatEditMode = false;
bool chatOpen = false;

void Chat_Draw(Vector2 offset, Color uiColor) {

    int chatWidth = 352;
    int fontSize = 10;

    //Draw Background
    if (chatEditMode) DrawRectangle(offset.x, offset.y - 184 + 46, chatWidth, 184, uiColor);

    float opacity = chatEditMode ? 1.0f : 150.0f / 255;

    //Draw Lines
    int lineAdded = 0;

    int index = currentLine == 0 ? 63 : currentLine - 1;
    while (lineAdded < 13) {
        if (chatLines[index]) {
            int capacity = strlen(chatLines[index]) + 1;
            TextGlyph *glyphs = MemAlloc(capacity * sizeof(*glyphs));
            if (!glyphs) break;
            int lines;
            float width;
            int count = FormattedText_Layout(chatLines[index], WHITE, fontSize, chatWidth - fontSize - 4,
                glyphs, capacity, &lines, &width);
            for (int i = 0; i < count; i++) {
                int row = lineAdded + lines - 1 - glyphs[i].line;
                if (row >= 13) continue;
                FormattedText_DrawGlyph(glyphs[i], (Vector2){offset.x + 4 + glyphs[i].x,
                    offset.y - row * fontSize}, fontSize, opacity);
            }
            lineAdded += lines;
            MemFree(glyphs);
        }

        index--;
        if (index < 0) index = 63;
        if (index == currentLine) break;
    }

    //Chat input
    if (chatEditMode) GuiTextBox((Rectangle) { offset.x, offset.y + 22, chatWidth, 24 }, chatInput, 64, chatEditMode);
    
    if (IsKeyPressed(KEY_ENTER)) {
        if (chatOpen) {
            char *message = MemAlloc(64);
            for (int i = 0; i < 64; i++) {
                message[i] = chatInput[i];
                chatInput[i] = '\0';
            }
            if (networkConnectedToServer) {
                Network_Send(Packet_CreateMessage(message));
                MemFree(message);
            } else {
                Chat_AddOwnedLine(message);
            }
            DisableCursor();
            chatOpen = false;
            screenCursorEnabled = false;
        }
    }

    if (chatOpen) {
        chatEditMode = true;
    } else {
        chatEditMode = false;
    }

}

void Chat_AppendOwnedLine(char *text) {
    int index = currentLine == 0 ? 63 : currentLine - 1;
    if (chatLines[index] == NULL) {
        Chat_AddOwnedLine(text);
        return;
    }

    int lineLength = TextLength(chatLines[index]);
    int textLength = TextLength(text);
    char *combined = MemAlloc(lineLength + textLength + 1);
    if (combined == NULL) {
        MemFree(text);
        return;
    }
    memcpy(combined, chatLines[index], lineLength);
    memcpy(combined + lineLength, text, textLength + 1);
    MemFree(chatLines[index]);
    MemFree(text);
    chatLines[index] = combined;
}
