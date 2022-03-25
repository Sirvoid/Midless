/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_SCREEN_H
#define G_SCREEN_H

typedef enum Screen {
    SCREEN_GAME,
    SCREEN_PAUSE,
    SCREEN_JOINING,
    SCREEN_LOGIN,
    SCREEN_OPTIONS
} Screen;

extern bool Screen_cursorEnabled;

void Screens_init(Texture2D terrain, bool *exit);
void Screen_Switch(Screen screen);

void Screen_Make(void);

void Screen_MakeGame(void);
void Screen_MakePause(void);
void Screen_MakeOptions(void);
void Screen_MakeJoining(void);
void Screen_MakeLogin(void);

#endif