/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 */

#include "settings.h"
#include <math.h>
#include <string.h>
bool settingsShowDebug;
int fpsChoice;
bool videoVSync, videoFullscreen;
const int fpsValues[] = {60, 120, 144, 240, 0};
float settingsUIScale = 1.0f, settingsSensitivity = 1.0f, settingsFOV = 65.0f;
bool settingsInvertMouse = false;
int settingsKeys[CONTROL_COUNT] = {KEY_W, KEY_S, KEY_A, KEY_D, KEY_SPACE,
    KEY_LEFT_SHIFT, KEY_E, KEY_T, KEY_F5, KEY_F3};
static const int defaultKeys[CONTROL_COUNT] = {KEY_W, KEY_S, KEY_A, KEY_D, KEY_SPACE,
    KEY_LEFT_SHIFT, KEY_E, KEY_T, KEY_F5, KEY_F3};
bool Settings_ValidBinding(int key) {
    return (key >= KEY_A && key <= KEY_Z) || (key >= KEY_F1 && key <= KEY_F12) ||
        key == KEY_SPACE || (key >= KEY_RIGHT && key <= KEY_UP) ||
        (key >= KEY_LEFT_SHIFT && key <= KEY_RIGHT_SUPER);
}

void Settings_ApplyVideo(void) {
#if !defined(PLATFORM_WEB)
    if (IsWindowFullscreen() != videoFullscreen) ToggleFullscreen();
#endif
    if (videoVSync) SetWindowState(FLAG_VSYNC_HINT);
    else ClearWindowState(FLAG_VSYNC_HINT);
    SetTargetFPS(fpsValues[fpsChoice]);
}
void Settings_ResetControls(void) {
    memcpy(settingsKeys, defaultKeys, sizeof(settingsKeys)); settingsSensitivity=1; settingsInvertMouse=false;
}
void Settings_Read(FILE *file) {
        float scale, sensitivity, fov;
        int invert, vsync, fullscreen;
        if (fscanf(file, " v2 %f %f %f %d %d %d", &scale, &sensitivity, &fov, &invert, &vsync, &fullscreen) == 6) {
            if (isfinite(scale) && scale >= 0.75f && scale <= 1.5f) settingsUIScale = scale;
            if (isfinite(sensitivity) && sensitivity >= 0.1f && sensitivity <= 3) settingsSensitivity = sensitivity;
            if (isfinite(fov) && fov >= 45 && fov <= 110) settingsFOV = fov;
            settingsInvertMouse = invert == 1; videoVSync = vsync == 1; videoFullscreen = fullscreen == 1;
            int keys[CONTROL_COUNT];
            bool valid = true;
            for (int i = 0; i < CONTROL_COUNT; i++) {
                if (fscanf(file, "%d", &keys[i]) != 1 || !Settings_ValidBinding(keys[i])) { valid = false; break; }
                for (int j = 0; j < i; j++) if (keys[i] == keys[j]) valid = false;
            }
            if (valid) memcpy(settingsKeys, keys, sizeof(keys));
        }
}
int Settings_Write(FILE *file) {
    int result = fprintf(file, "v2 %g %g %g %d %d %d\n",
        settingsUIScale, settingsSensitivity, settingsFOV, settingsInvertMouse, videoVSync, videoFullscreen);
    for (int i = 0; i < CONTROL_COUNT && result >= 0; i++) result = fprintf(file, "%d ", settingsKeys[i]);
    return result;
}

void Settings_BindKey(int action, int key) {
    if (action < 0 || action >= CONTROL_COUNT || !Settings_ValidBinding(key)) return;
    for (int i = 0; i < CONTROL_COUNT; i++)
        if (i != action && settingsKeys[i] == key) settingsKeys[i] = settingsKeys[action];
    settingsKeys[action] = key;
}

