/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_SCREEN_H
#define MIDLESS_CLIENT_SCREEN_H

typedef enum Screen {
    SCREEN_GAME,
    SCREEN_PAUSE,
    SCREEN_LOADING,
    SCREEN_JOINING,
    SCREEN_LOGIN,
    SCREEN_OPTIONS
} Screen;

extern bool screenCursorEnabled;
extern bool screenShowDebug;

void Screen_Init(Texture2D terrain, bool *exit);
void Screen_Shutdown(void);
void Screen_Switch(Screen screen);

void Screen_Draw(void);

void Screen_DrawGame(void);
void Screen_DrawPause(void);
void Screen_DrawOptions(void);
void Screen_DrawLoading(void);
void Screen_DrawJoining(void);
void Screen_DrawLogin(void);

#endif
