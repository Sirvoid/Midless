/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 */

#ifndef MIDLESS_SETTINGS_H
#define MIDLESS_SETTINGS_H
#include "raylib.h"
#include <stdio.h>
extern bool settingsShowDebug;
extern float settingsUIScale, settingsSensitivity, settingsFOV;
extern bool settingsInvertMouse;
typedef enum ControlAction {
    CONTROL_FORWARD, CONTROL_BACKWARD, CONTROL_LEFT, CONTROL_RIGHT,
    CONTROL_JUMP, CONTROL_SNEAK, CONTROL_INVENTORY, CONTROL_CHAT,
    CONTROL_CAMERA, CONTROL_DEBUG, CONTROL_COUNT
} ControlAction;
extern int settingsKeys[CONTROL_COUNT];

extern int fpsChoice;
extern const int fpsValues[5];
extern bool videoVSync, videoFullscreen;
bool Settings_ValidBinding(int key);
void Settings_BindKey(int action, int key);
void Settings_ApplyVideo(void);
void Settings_ResetControls(void);
void Settings_Read(FILE *file);
int Settings_Write(FILE *file);
#endif
