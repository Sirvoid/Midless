#include "version.h"
/**
 * Copyright (c) 2021 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#define RAYGUI_IMPLEMENTATION
#define RAYGUI_SUPPORT_ICONS
#include <pthread.h>
#include <math.h>
#include "raylib.h"
#include "raygui.h"
#include "screens.h"
#include "chat.h"
#include "player.h"
#include "world.h"
#include "block.h"
#include "networkhandler.h"
#include "packet.h"
#include "client.h"
#include "clientws.h"
#include "localserver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif
#include "blockitemrenderer.h"
#include "inventoryscreen.h"


Screen currentScreen = SCREEN_MAIN;
bool screenCursorEnabled = false;
bool screenShowDebug = false;
int screenHeight, screenWidth;
bool *exitGame;
Color uiColBg;
static Screen optionsReturn = SCREEN_MAIN;
static int focus, controlIndex, pendingDistance, fpsChoice;
static bool skipMenuKeys, loadingNextFrame, loadingStarted, intentionalDisconnect;
static bool acceptGeneratorChange;
static Screen disconnectReturn = SCREEN_MAIN;
static double joiningStarted;
static char nameInput[16] = "Player";
static char ipInput[128] = "localhost";
static char portInput[6] = "25565";
static char fullAddress[144];
static char connectionError[160];
static char preferencesError[96];
static const int fpsValues[] = {60, 120, 144, 240, 0};
float screenUIScale = 1.0f, screenSensitivity = 1.0f, screenFOV = 65.0f;
bool screenInvertMouse = false;
int screenKeys[CONTROL_COUNT] = {KEY_W, KEY_S, KEY_A, KEY_D, KEY_SPACE,
    KEY_LEFT_SHIFT, KEY_E, KEY_T, KEY_F5, KEY_F3};
static const int defaultKeys[CONTROL_COUNT] = {KEY_W, KEY_S, KEY_A, KEY_D, KEY_SPACE,
    KEY_LEFT_SHIFT, KEY_E, KEY_T, KEY_F5, KEY_F3};
static const char *controlNames[CONTROL_COUNT] = {"Forward", "Backward", "Left", "Right",
    "Jump", "Sneak", "Inventory", "Chat", "Camera view", "Debug info"};
typedef enum OptionsPage { OPTIONS_HOME, OPTIONS_VIDEO, OPTIONS_CONTROLS, OPTIONS_KEYS } OptionsPage;
static OptionsPage optionsPage;
static int bindingScroll, captureKey = -1;
static bool bindingDragging;
static float bindingDragOffset;
static bool videoVSync, videoFullscreen;
static float menuScale;

static bool ValidBinding(int key) {
    return (key >= KEY_A && key <= KEY_Z) || (key >= KEY_F1 && key <= KEY_F12) ||
        key == KEY_SPACE || (key >= KEY_RIGHT && key <= KEY_UP) ||
        (key >= KEY_LEFT_SHIFT && key <= KEY_RIGHT_SUPER);
}

static void ApplyVideo(void) {
#if !defined(PLATFORM_WEB)
    if (IsWindowFullscreen() != videoFullscreen) ToggleFullscreen();
#endif
    if (videoVSync) SetWindowState(FLAG_VSYNC_HINT);
    else ClearWindowState(FLAG_VSYNC_HINT);
    SetTargetFPS(fpsValues[fpsChoice]);
}
static float hoverAmount[CONTROL_COUNT + 1];
static bool keyboardFocus = true;
static int invalidField = -1;

static Texture2D menuTerrain;
static Texture2D menuVignette;

static bool Menu_Key(int key) { return !skipMenuKeys && IsKeyPressed(key); }

static void SavePreferences(void) {
    FILE *file = fopen("menu-settings.txt", "w");
    if (!file) {
        snprintf(preferencesError, sizeof(preferencesError), "Could not save preferences in the game folder.");
        return;
    }
    int result = fprintf(file, "%s\n%s\n%s\n%d %d %d\n", nameInput, ipInput, portInput,
                         world.drawDistance, fpsChoice, screenShowDebug);
    if (result >= 0) result = fprintf(file, "v2 %g %g %g %d %d %d\n",
        screenUIScale, screenSensitivity, screenFOV, screenInvertMouse, videoVSync, videoFullscreen);
    for (int i = 0; i < CONTROL_COUNT && result >= 0; i++) result = fprintf(file, "%d ", screenKeys[i]);
    int closed = fclose(file);
    if (result < 0 || closed != 0)
        snprintf(preferencesError, sizeof(preferencesError), "Could not save preferences in the game folder.");
    else {
        preferencesError[0] = '\0';
#if defined(PLATFORM_WEB)
        if (!EM_ASM_INT({
            try { localStorage.setItem('midless.menu', FS.readFile('menu-settings.txt', {encoding: 'utf8'})); return 1; }
            catch (error) { return 0; }
        })) snprintf(preferencesError, sizeof(preferencesError), "Browser storage is unavailable; preferences will not persist.");
#endif
    }
}

static void ReadPreferenceLine(FILE *file, char *value, size_t capacity) {
    char line[256];
    if (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) < capacity) snprintf(value, capacity, "%s", line);
    }
}

void Screen_Init(Texture2D terrain, bool *exit) {
    exitGame = exit;
    BlockItemRenderer_Init(terrain);
    menuTerrain = terrain;
    if (menuVignette.id) UnloadTexture(menuVignette);
    menuVignette = (Texture2D){0};
    // A small, smoothly filtered alpha mask gives an elliptical vignette at any aspect ratio.
    Image vignette = GenImageColor(128, 128, BLANK);
    if (vignette.data) {
        for (int y = 0; y < 128; y++) {
            for (int x = 0; x < 128; x++) {
                float dx = (x - 63.5f) / 63.5f;
                float dy = (y - 63.5f) / 63.5f;
                float edge = fminf(1, fmaxf(0, (sqrtf(dx * dx + dy * dy) - 0.25f) / 1.1f));
                edge = edge * edge * (3 - 2 * edge);
                ImageDrawPixel(&vignette, x, y, (Color){0, 0, 0, (unsigned char)(edge * 150)});
            }
        }
        menuVignette = LoadTextureFromImage(vignette);
        SetTextureFilter(menuVignette, TEXTURE_FILTER_BILINEAR);
        UnloadImage(vignette);
    }
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, 0x454545ff);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, 0x191919ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0xffffffff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, 0xffffffff);
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, 0x303030ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, 0xffffffff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, 0xffffffff);
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, 0x424242ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, 0xffffffff);
    GuiSetStyle(TEXTBOX, BASE_COLOR_NORMAL, 0x191919ff);
    GuiSetStyle(TEXTBOX, BASE_COLOR_FOCUSED, 0x383838ff);
    GuiSetStyle(TEXTBOX, TEXT_COLOR_NORMAL, 0xffffffff);
    GuiSetStyle(TEXTBOX, TEXT_COLOR_PRESSED, 0xffffffff);
#if defined(PLATFORM_WEB)
    EM_ASM({
        try {
            var saved = localStorage.getItem('midless.menu');
            if (saved) FS.writeFile('menu-settings.txt', saved);
        } catch (error) { /* Defaults remain usable when browser storage is blocked. */ }
    });
#endif
    FILE *file = fopen("menu-settings.txt", "r");
    if (file) {
        ReadPreferenceLine(file, nameInput, sizeof(nameInput));
        ReadPreferenceLine(file, ipInput, sizeof(ipInput));
        ReadPreferenceLine(file, portInput, sizeof(portInput));
        int distance, fps, debug;
        if (fscanf(file, "%d %d %d", &distance, &fps, &debug) == 3) {
            if (distance >= 2 && distance <= 32) world.drawDistance = distance;
            if (fps >= 0 && fps < 5) fpsChoice = fps;
            screenShowDebug = debug != 0;
        }
        float scale, sensitivity, fov;
        int invert, vsync, fullscreen;
        if (fscanf(file, " v2 %f %f %f %d %d %d", &scale, &sensitivity, &fov, &invert, &vsync, &fullscreen) == 6) {
            if (isfinite(scale) && scale >= 0.75f && scale <= 1.5f) screenUIScale = scale;
            if (isfinite(sensitivity) && sensitivity >= 0.1f && sensitivity <= 3) screenSensitivity = sensitivity;
            if (isfinite(fov) && fov >= 45 && fov <= 110) screenFOV = fov;
            screenInvertMouse = invert == 1; videoVSync = vsync == 1; videoFullscreen = fullscreen == 1;
            int keys[CONTROL_COUNT];
            bool valid = true;
            for (int i = 0; i < CONTROL_COUNT; i++) {
                if (fscanf(file, "%d", &keys[i]) != 1 || !ValidBinding(keys[i])) { valid = false; break; }
                for (int j = 0; j < i; j++) if (keys[i] == keys[j]) valid = false;
            }
            if (valid) memcpy(screenKeys, keys, sizeof(keys));
        }
        fclose(file);
    }
    ApplyVideo();
    networkName = nameInput;
    SetTargetFPS(fpsValues[fpsChoice]);
    Screen_Switch(SCREEN_MAIN);
}

void Screen_Shutdown(void) {
    SavePreferences();
    Client_Shutdown();
    BlockItemRenderer_Shutdown();
    if (menuVignette.id) UnloadTexture(menuVignette);
    menuVignette = (Texture2D){0};
    menuTerrain = (Texture2D){0};
}

void Screen_DrawGame(void) {

    //Draw debug infos
    if (screenShowDebug) {
        const char* coordText = TextFormat("X: %i Y: %i Z: %i", (int)player.position.x, (int)player.position.y, (int)player.position.z);
        const char* debugText;

        if (networkConnectedToServer) {
            debugText = TextFormat("%2i FPS %2i PING", GetFPS(), networkPing);
        } else {
            debugText = TextFormat("%2i FPS", GetFPS());
        }
    
        const char* versionText = GAME_VERSION_TEXT;
        DrawText(versionText, 9, 9, 20, BLACK);
        DrawText(versionText, 8, 8, 20, WHITE);

        DrawText(debugText, 9, 29, 20, BLACK);
        DrawText(coordText, 9, 49, 20, BLACK);
        DrawText(debugText, 8, 28, 20, WHITE);
        DrawText(coordText, 8, 48, 20, WHITE);
    }

    //Draw crosshair
    DrawRectangle(screenWidth / 2 - 8, screenHeight / 2 - 2, 16, 4, uiColBg);
    DrawRectangle(screenWidth / 2 - 2, screenHeight / 2 + 2,  4, 6, uiColBg);
    DrawRectangle(screenWidth / 2 - 2, screenHeight / 2 - 8,  4, 6, uiColBg);

    ClientInventory_Draw(false);

    //Draw Chat
    Chat_Draw((Vector2){16, screenHeight - 82}, uiColBg);
}

// The default pixel font has a 10-pixel cell. Whole multiples keep its edges crisp.
static int Menu_Font(int size) {
    return 10 * (int)fmaxf(1, roundf(size * menuScale / 10));
}

static Rectangle Menu_Rect(float y, float height) {
    float width = currentScreen == SCREEN_OPTIONS ? 440 : 320;
    return (Rectangle){roundf(screenWidth / 2.0f - width * menuScale / 2),
        roundf(screenHeight / 2.0f + y * menuScale), roundf(width * menuScale), roundf(height * menuScale)};
}

static void Menu_Text(const char *text, float y, int size, Color color) {
    int fontSize = Menu_Font(size);
    DrawText(text, (screenWidth - MeasureText(text, fontSize)) / 2,
             roundf(screenHeight / 2 + y * menuScale), fontSize, color);
}

static void Menu_Background(const char *title, Screen screen) {
    bool inWorld = screen == SCREEN_PAUSE ||
                   (screen == SCREEN_OPTIONS && optionsReturn == SCREEN_PAUSE);
    if (inWorld) {
        DrawRectangle(0, 0, screenWidth, screenHeight, (Color){0, 0, 0, 210});
    } else {
        ClearBackground((Color){24, 24, 24, 255});
        if (menuTerrain.id && menuTerrain.width >= 32 && menuTerrain.height >= 16) {
            // Stone is tile 1 in the built-in atlas. Integer pixel enlargement prevents seams.
            int tileSize = 16 * (int)fmaxf(4, roundf(4 * menuScale));
            Rectangle stone = {16, 0, 16, 16};
            for (int y = 0; y < screenHeight; y += tileSize) {
                for (int x = 0; x < screenWidth; x += tileSize) {
                    DrawTexturePro(menuTerrain, stone,
                        (Rectangle){x, y, tileSize, tileSize}, (Vector2){0}, 0,
                        (Color){64, 64, 64, 255});
                }
            }
        }
    }
    if (menuVignette.id) DrawTexturePro(menuVignette,
        (Rectangle){0, 0, menuVignette.width, menuVignette.height},
        (Rectangle){0, 0, screenWidth, screenHeight}, (Vector2){0}, 0, WHITE);

    float width = (screen == SCREEN_OPTIONS ? 504 : 384) * menuScale;
    int titleWidth = MeasureText(title, Menu_Font(screen == SCREEN_MAIN ? 80 : 30));
    width = fmaxf(width, titleWidth + 64 * menuScale);
    width = fminf(width, screenWidth - 32);
    float top = screen == SCREEN_OPTIONS ? -240 : screen == SCREEN_LOGIN ? -246 : -208;
    Rectangle panel = {roundf((screenWidth - width) / 2),
        roundf(screenHeight / 2 + top * menuScale), roundf(width), roundf(((screen == SCREEN_OPTIONS ? 280 : 228) - top) * menuScale)};
    DrawRectangleRec(panel, (Color){0, 0, 0, 145});
    DrawRectangleLinesEx(panel, 1, (Color){115, 115, 115, 45});
}

static void Menu_Begin(const char *title, int controls) {
    float fit = fminf(screenWidth / 640.0f, screenHeight / (currentScreen == SCREEN_OPTIONS ? 640.0f : 560.0f));
    float base = fminf(2.0f, fminf(screenWidth / 800.0f, screenHeight / 700.0f));
    menuScale = fminf(fit, base * screenUIScale);
    GuiSetStyle(DEFAULT, TEXT_SIZE, Menu_Font(20));
    GuiSetStyle(DEFAULT, BORDER_WIDTH, 1);
    GuiSetStyle(TEXTBOX, TEXT_PADDING, (int)(12 * menuScale));
    Menu_Background(title, currentScreen);
    Menu_Text(title, currentScreen == SCREEN_OPTIONS ? -212 : currentScreen == SCREEN_LOGIN ? -220 : -180,
              currentScreen == SCREEN_MAIN ? 80 : 30, WHITE);
    controlIndex = 0;
    if (Menu_Key(KEY_TAB)) {
        keyboardFocus = true;
        focus = (focus + ((IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? controls - 1 : 1)) % controls;
    } else if (currentScreen != SCREEN_LOGIN || focus >= 3) {
        if (Menu_Key(KEY_DOWN)) { focus = (focus + 1) % controls; keyboardFocus = true; }
        if (Menu_Key(KEY_UP)) { focus = (focus + controls - 1) % controls; keyboardFocus = true; }
    }
    if (preferencesError[0]) Menu_Text(preferencesError, 238, 12, GRAY);
}

static bool Menu_Focus(Rectangle bounds) {
    int index = controlIndex++;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), bounds)) {
        focus = index;
        keyboardFocus = false;
    }
    return focus == index;
}

static bool Menu_Button(float y, const char *text, bool enabled) {
    Rectangle bounds = Menu_Rect(y, 44);
    if (currentScreen == SCREEN_OPTIONS) {
        bounds.x += roundf(60 * menuScale);
        bounds.width -= roundf(120 * menuScale);
    }
    bool selected = Menu_Focus(bounds);
    int slot = controlIndex - 1;
    bool hovered = enabled && CheckCollisionPointRec(GetMousePosition(), bounds);
    float target = hovered ? 1 : 0;
    hoverAmount[slot] += (target - hoverAmount[slot]) * fminf(1, GetFrameTime() * 14);
    bool primary = (currentScreen == SCREEN_MAIN || currentScreen == SCREEN_PAUSE) && slot == 0;
    bool quiet = !strcmp(text, "Quit") || !strcmp(text, "Back") || !strcmp(text, "Settings");
    int shade = (primary ? 65 : quiet ? 10 : 24) + (int)(hoverAmount[slot] * 25);
    if (hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) shade += 15;
    if (!enabled) GuiDisable();
    bool pressed = GuiButton(bounds, text);
    GuiEnable();
    // Keep raygui's click behavior, with a smoothly interpolated monochrome surface.
    DrawRectangleRec(bounds, (Color){shade, shade, shade, 255});
    Color border = selected && keyboardFocus && enabled ? WHITE : (Color){55, 55, 55, 255};
    DrawRectangleLinesEx(bounds, 1, border);
    int font = Menu_Font(20);
    if (MeasureText(text, font) > bounds.width - 20 * menuScale) font = Menu_Font(12);
    DrawText(text, roundf(bounds.x + (bounds.width - MeasureText(text, font)) / 2),
             roundf(bounds.y + (bounds.height - font) / 2), font,
             !enabled ? GRAY : quiet && !hovered ? LIGHTGRAY : WHITE);
    return enabled && (pressed || (selected && (Menu_Key(KEY_ENTER) || Menu_Key(KEY_SPACE))));
}

static void Menu_Field(float y, const char *label, char *value, int capacity) {
    Rectangle bounds = Menu_Rect(y, 40);
    DrawText(label, bounds.x, roundf(bounds.y - 20 * menuScale), Menu_Font(14), LIGHTGRAY);
    bool selected = Menu_Focus(bounds);
    char previous[128];
    snprintf(previous, sizeof(previous), "%s", value);
    GuiTextBox(bounds, value, capacity, selected);
    if (invalidField == controlIndex - 1) {
        if (strcmp(previous, value)) { invalidField = -1; connectionError[0] = '\0'; }
        else DrawText(connectionError, bounds.x, roundf(bounds.y + bounds.height + 5 * menuScale), Menu_Font(12), LIGHTGRAY);
    }
}

static void ApplyDistance(void) {
    if (pendingDistance == world.drawDistance) return;
    bool increasing = pendingDistance > world.drawDistance;
    world.drawDistance = pendingDistance;
    if (networkConnectedToServer) {
        if (increasing) World_LoadChunks();
        else World_Reload();
        Network_Send(Packet_CreateSetDrawDistance(world.drawDistance));
    }
    SavePreferences();
}

static void BeginJoin(void) {
    if (Client_IsBusy()) return;
    bool validName = false;
    for (const char *c = nameInput; *c; c++) if (!isspace((unsigned char)*c)) validName = true;
    char *end;
    long port = strtol(portInput, &end, 10);
    bool validAddress = ipInput[0] != '\0';
    for (const char *c = ipInput; *c; c++) if (isspace((unsigned char)*c)) validAddress = false;
    bool validPort = portInput[0] != '\0';
    for (const char *c = portInput; *c; c++) if (!isdigit((unsigned char)*c)) validPort = false;
    if (!validName || !validAddress || !validPort || *end || port < 1 || port > 65535) {
        snprintf(connectionError, sizeof(connectionError), "%s", !validName ? "Enter a player name." :
                 !validAddress ? "Enter a hostname or IP address." : "Enter a port from 1 to 65535.");
        invalidField = !validName ? 0 : !validAddress ? 1 : 2;
        focus = invalidField;
        return;
    }
    invalidField = -1;
    networkName = nameInput;
    networkIp = ipInput;
    networkPort = (int)port;
    snprintf(fullAddress, sizeof(fullAddress), "%s:%s", ipInput, portInput);
    networkFullAddress = fullAddress;
    SavePreferences();
    connectionError[0] = '\0';
    intentionalDisconnect = false;
    joiningStarted = GetTime();
    Screen_Switch(SCREEN_JOINING);
    if (!Client_Start()) {
        snprintf(connectionError, sizeof(connectionError), "Could not start the network client.");
        Screen_Switch(SCREEN_CONNECTION_ERROR);
    }
}

static void LeaveConnection(Screen destination) {
    intentionalDisconnect = true;
    disconnectReturn = destination;
    Client_Stop();
    Network_Disconnect();
}

void Screen_ConnectionEnded(bool wasLocal) {
    if (intentionalDisconnect || wasLocal) {
        intentionalDisconnect = false;
        Screen_Switch(wasLocal ? SCREEN_MAIN : disconnectReturn);
    } else {
        if (!connectionError[0]) snprintf(connectionError, sizeof(connectionError), "%s",
            currentScreen == SCREEN_JOINING ? "Could not join. Check the address and server availability." : "The connection to the server was lost.");
        Screen_Switch(SCREEN_CONNECTION_ERROR);
    }
}

static void Screen_DrawMain(void) {
    Menu_Begin("MIDLESS", 4);
    bool ready = !Client_IsBusy();
    if (Menu_Button(-74, "Singleplayer", ready)) { networkName = nameInput; Screen_Switch(SCREEN_LOADING); return; }
    if (Menu_Button(-20, "Multiplayer", ready)) { connectionError[0] = '\0'; Screen_Switch(SCREEN_LOGIN); return; }
    if (Menu_Button(46, "Settings", true)) { Screen_Switch(SCREEN_OPTIONS); return; }
    if (Menu_Button(100, "Quit", true)) *exitGame = true;
    Menu_Text(GAME_VERSION_TEXT, 192, 12, GRAY);
}

void Screen_DrawPause(void) {
    Menu_Begin("Paused", 3);
    if (Menu_Button(-74, "Resume", true) || Menu_Key(KEY_ESCAPE)) { Screen_Switch(SCREEN_GAME); return; }
    if (Menu_Button(-20, "Settings", true)) { Screen_Switch(SCREEN_OPTIONS); return; }
    const char *leaveLabel = LocalServer_IsRunning() ? "Save & Quit" : "Disconnect";
    if (Menu_Button(46, leaveLabel, true)) { LeaveConnection(SCREEN_MAIN); return; }
}

static Rectangle Menu_SettingsRow(float y, const char *label) {
    Rectangle row = Menu_Rect(y, 40);
    int font = Menu_Font(20);
    DrawText(label, row.x, roundf(row.y + (row.height - font) / 2), font, LIGHTGRAY);
    Rectangle control = {roundf(row.x + 220 * menuScale), row.y,
                         roundf(220 * menuScale), row.height};
    DrawRectangleRec(control, (Color){25, 25, 25, 255});
    DrawRectangleLinesEx(control, 1, (Color){69, 69, 69, 255});
    return control;
}

static void Options_Open(OptionsPage page) {
    optionsPage = page; focus = 0; captureKey = -1;
    bindingScroll = 0; bindingDragging = false;
    skipMenuKeys = true;
    memset(hoverAmount, 0, sizeof(hoverAmount));
}

static int Option_Choice(float y, const char *label, const char *value) {
    Rectangle bounds = Menu_SettingsRow(y, label);
    bool selected = Menu_Focus(bounds);
    Rectangle left = {bounds.x, bounds.y, 30 * menuScale, bounds.height};
    Rectangle right = {bounds.x + bounds.width - left.width, bounds.y, left.width, bounds.height};
    int delta = 0;
    if (GuiButton(left, "<") || (selected && Menu_Key(KEY_LEFT))) delta = -1;
    if (GuiButton(right, ">") || (selected && (Menu_Key(KEY_RIGHT) || Menu_Key(KEY_ENTER) || Menu_Key(KEY_SPACE)))) delta = 1;
    int font = Menu_Font(20);
    DrawText(value, roundf(bounds.x + (bounds.width - MeasureText(value, font)) / 2),
        roundf(bounds.y + (bounds.height - font) / 2), font, WHITE);
    if (selected && keyboardFocus) DrawRectangleLinesEx(bounds, 1, WHITE);
    return delta;
}

static const char *Binding_Name(int key) {
    if (key >= KEY_A && key <= KEY_Z) return TextFormat("%c", key);
    if (key >= KEY_F1 && key <= KEY_F12) return TextFormat("F%d", key - KEY_F1 + 1);
    switch (key) {
        case KEY_SPACE: return "Space";
        case KEY_LEFT_SHIFT: return "Left Shift"; case KEY_RIGHT_SHIFT: return "Right Shift";
        case KEY_LEFT_CONTROL: return "Left Ctrl"; case KEY_RIGHT_CONTROL: return "Right Ctrl";
        case KEY_LEFT_ALT: return "Left Alt"; case KEY_RIGHT_ALT: return "Right Alt";
        case KEY_LEFT_SUPER: return "Left Super"; case KEY_RIGHT_SUPER: return "Right Super";
        case KEY_UP: return "Up"; case KEY_DOWN: return "Down";
        case KEY_LEFT: return "Left"; case KEY_RIGHT: return "Right";
        default: return "Unknown";
    }
}

static void BindKey(int action, int key) {
    for (int i = 0; i < CONTROL_COUNT; i++)
        if (i != action && screenKeys[i] == key) screenKeys[i] = screenKeys[action];
    screenKeys[action] = key;
}

static void Bindings_Scroll(bool capturing) {
    const int visible = 6, maximum = CONTROL_COUNT - visible;
    Rectangle list = Menu_Rect(-150, 290);
    Rectangle track = {list.x + list.width + 8 * menuScale, list.y, 16 * menuScale, list.height};
    float thumbHeight = track.height * visible / CONTROL_COUNT;
    float travel = track.height - thumbHeight;
    Vector2 mouse = GetMousePosition();
    if (!capturing) {
        if (Menu_Key(KEY_TAB) || Menu_Key(KEY_UP) || Menu_Key(KEY_DOWN)) {
            if (focus < CONTROL_COUNT) {
                if (focus < bindingScroll) bindingScroll = focus;
                if (focus >= bindingScroll + visible) bindingScroll = focus - visible + 1;
            }
        }
        if (CheckCollisionPointRec(mouse, list) || CheckCollisionPointRec(mouse, track)) {
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                bindingScroll -= (int)(wheel > 0 ? ceilf(wheel) : floorf(wheel));
                keyboardFocus = false;
            }
        }
        bindingScroll = (int)fminf(maximum, fmaxf(0, bindingScroll));
        float thumbY = track.y + travel * bindingScroll / maximum;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, track)) {
            bindingDragging = true;
            keyboardFocus = false;
            bindingDragOffset = mouse.y >= thumbY && mouse.y <= thumbY + thumbHeight ? mouse.y - thumbY : thumbHeight / 2;
        }
        if (bindingDragging) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
                bindingScroll = (int)fminf(maximum, fmaxf(0, roundf((mouse.y - track.y - bindingDragOffset) * maximum / travel)));
            else bindingDragging = false;
        }
    } else bindingDragging = false;
    DrawRectangleRec(track, (Color){25, 25, 25, 255});
    DrawRectangleLinesEx(track, 1, (Color){69, 69, 69, 255});
    Rectangle thumb = {track.x + 2, track.y + travel * bindingScroll / maximum, track.width - 4, thumbHeight};
    DrawRectangleRec(thumb, capturing ? DARKGRAY : bindingDragging ? WHITE : GRAY);
}

void Screen_DrawOptions(void) {
    bool capturing = captureKey >= 0;
    if (capturing) {
        if (IsKeyPressed(KEY_ESCAPE)) captureKey = -1;
        else {
            int key = GetKeyPressed();
            if (ValidBinding(key)) { BindKey(captureKey, key); captureKey = -1; SavePreferences(); }
        }
        skipMenuKeys = true;
        GuiDisable();
    }
    Menu_Begin(optionsPage == OPTIONS_VIDEO ? "Video" : optionsPage == OPTIONS_CONTROLS ? "Controls" :
        optionsPage == OPTIONS_KEYS ? "Key bindings" : "Settings",
        optionsPage == OPTIONS_VIDEO ? 8 : optionsPage == OPTIONS_KEYS ? CONTROL_COUNT + 1 : optionsPage == OPTIONS_CONTROLS ? 5 : 3);
    if (optionsPage == OPTIONS_HOME) {
        if (Menu_Button(-74, "Controls", true)) { Options_Open(OPTIONS_CONTROLS); return; }
        if (Menu_Button(-20, "Video", true)) { Options_Open(OPTIONS_VIDEO); return; }
        if (Menu_Button(100, "Back", true) || Menu_Key(KEY_ESCAPE)) { SavePreferences(); Screen_Switch(optionsReturn); }
    } else if (optionsPage == OPTIONS_VIDEO) {
        int delta = Option_Choice(-150, "Draw distance", TextFormat("%d", pendingDistance));
        pendingDistance = (int)fminf(32, fmaxf(2, pendingDistance + delta));
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) ApplyDistance();
        delta = Option_Choice(-100, "FPS limit", fpsChoice == 4 ? "Unlimited" : TextFormat("%d", fpsValues[fpsChoice]));
        if (delta) { fpsChoice = (fpsChoice + delta + 5) % 5; SetTargetFPS(fpsValues[fpsChoice]); }
#if !defined(PLATFORM_WEB)
        videoFullscreen = IsWindowFullscreen();
        if (Option_Choice(-50, "Fullscreen", videoFullscreen ? "On" : "Off")) {
            videoFullscreen = !videoFullscreen; ApplyVideo();
        }
#else
        Option_Choice(-50, "Fullscreen", "Browser (F11)");
#endif
        if (Option_Choice(0, "VSync", videoVSync ? "On" : "Off")) { videoVSync = !videoVSync; ApplyVideo(); }
        screenFOV = fminf(110, fmaxf(45, screenFOV + 5 * Option_Choice(50, "Field of view", TextFormat("%.0f", screenFOV))));
        screenUIScale = fminf(1.5f, fmaxf(0.75f, screenUIScale + 0.25f * Option_Choice(100, "UI scale", TextFormat("%.0f%%", screenUIScale * 100))));
        if (Option_Choice(150, "Debug info", screenShowDebug ? "On" : "Off")) screenShowDebug = !screenShowDebug;
        if (Menu_Button(220, "Back", true) || Menu_Key(KEY_ESCAPE)) { ApplyDistance(); SavePreferences(); Options_Open(OPTIONS_HOME); }
    } else if (optionsPage == OPTIONS_CONTROLS) {
        screenSensitivity = fminf(3, fmaxf(0.1f, screenSensitivity + 0.1f * Option_Choice(-130, "Sensitivity", TextFormat("%.0f%%", screenSensitivity * 100))));
        if (Option_Choice(-80, "Invert mouse Y", screenInvertMouse ? "On" : "Off")) screenInvertMouse = !screenInvertMouse;
        if (Menu_Button(-10, "Key bindings", true)) { Options_Open(OPTIONS_KEYS); return; }
        if (Menu_Button(44, "Reset controls", true)) {
            memcpy(screenKeys, defaultKeys, sizeof(screenKeys)); screenSensitivity = 1; screenInvertMouse = false; SavePreferences();
        }
        if (Menu_Button(220, "Back", true) || Menu_Key(KEY_ESCAPE)) { SavePreferences(); Options_Open(OPTIONS_HOME); }
    } else {
        Bindings_Scroll(capturing);
        for (int row = 0; row < 6; row++) {
            int action = bindingScroll + row;
            controlIndex = action;
            Rectangle bounds = Menu_SettingsRow(-150 + row * 50, controlNames[action]);
            bool selected = Menu_Focus(bounds);
            bool pressed = GuiButton(bounds, captureKey == action ? "Press a key..." : Binding_Name(screenKeys[action]));
            if (!capturing && (pressed || (selected && (Menu_Key(KEY_ENTER) || Menu_Key(KEY_SPACE))))) {
                while (GetKeyPressed()) { }
                captureKey = action;
            }
            if (selected && keyboardFocus) DrawRectangleLinesEx(bounds, 1, WHITE);
        }
        controlIndex = CONTROL_COUNT;
        if (Menu_Button(220, "Back", !capturing) || Menu_Key(KEY_ESCAPE)) { SavePreferences(); Options_Open(OPTIONS_CONTROLS); }
    }
    GuiEnable();
}

void Screen_DrawLogin(void) {
    Menu_Begin("Multiplayer", 5);
    bool busy = Client_IsBusy();
    if (busy) GuiDisable();
    Menu_Field(-140, "Player name", nameInput, sizeof(nameInput));
    Menu_Field(-60, "Server address", ipInput, sizeof(ipInput));
    Menu_Field(20, "Port", portInput, sizeof(portInput));
    GuiEnable();
    if (Menu_Button(85, busy ? "Finishing previous connection..." : "Join Server", !busy) ||
        (!busy && focus < 3 && Menu_Key(KEY_ENTER))) { BeginJoin(); return; }
    if (Menu_Button(143, "Back", true) || Menu_Key(KEY_ESCAPE)) { SavePreferences(); Screen_Switch(SCREEN_MAIN); return; }
    if (connectionError[0] && invalidField < 0) Menu_Text(connectionError, 206, 12, LIGHTGRAY);
}

void Screen_DrawJoining(void) {
    Menu_Begin("Joining Server...", 1);
    // Keep long hostnames inside the panel without changing the actual destination.
    Menu_Text(TextFormat("%.38s", fullAddress), -90, 17, WHITE);
    Menu_Text(TextFormat("Waiting for the world... %.0f s", GetTime() - joiningStarted), -45, 16, LIGHTGRAY);
    if (Menu_Button(35, "Cancel", true) || Menu_Key(KEY_ESCAPE)) { LeaveConnection(SCREEN_LOGIN); return; }
    if (GetTime() - joiningStarted > 15 && !intentionalDisconnect) {
        snprintf(connectionError, sizeof(connectionError), "The server did not finish joining within 15 seconds.");
        LeaveConnection(SCREEN_CONNECTION_ERROR);
    }
}

static void Screen_DrawConnectionError(void) {
    Menu_Begin("Connection interrupted", 2);
    Menu_Text(connectionError, -110, 12, WHITE);
    Menu_Text(TextFormat("%.38s", fullAddress), -65, 16, LIGHTGRAY);
    bool busy = Client_IsBusy();
    if (Menu_Button(0, busy ? "Finishing previous connection..." : "Retry", !busy)) { BeginJoin(); return; }
    if (Menu_Button(58, "Back", true) || Menu_Key(KEY_ESCAPE)) { Screen_Switch(SCREEN_LOGIN); }
}

void Screen_DrawSavingWorld(bool serverFinished, int remainingChunks) {
    if (!IsWindowReady()) {
        WaitTime(0.001);
        return;
    }
    screenWidth = GetScreenWidth();
    screenHeight = GetScreenHeight();
    menuScale = fminf(2.0f, fminf(screenWidth / 640.0f, screenHeight / 560.0f));
    const char *title = serverFinished ? "Cleaning up..." : "Saving world...";
    BeginDrawing();
    // Use the stone backdrop even when shutdown was requested from a paused world.
    Menu_Background(title, SCREEN_LOADING);
    Menu_Text(title, -50, 30, WHITE);
    Menu_Text(TextFormat("%d chunks left to clean up", remainingChunks), 10, 20, LIGHTGRAY);
    EndDrawing(); // Presents progress and pumps window events during the shutdown loop.
}

void Screen_DrawLoading(void) {
    Menu_Begin("Loading World...", 1);
    Menu_Text("Preparing singleplayer", -50, 18, WHITE);
    if (loadingStarted) return;
    if (loadingNextFrame) {
        loadingNextFrame = false;
        loadingStarted = true;
        bool acceptChange = acceptGeneratorChange;
        acceptGeneratorChange = false;
        if (!LocalServer_Start(acceptChange)) {
            SavedGenerator previous, current;
            if (LocalServer_GetGeneratorChange(&previous, &current)) {
                Screen_Switch(SCREEN_GENERATOR_CHANGED);
                return;
            }
            snprintf(preferencesError, sizeof(preferencesError), "Could not start singleplayer. Check the game log.");
            Screen_Switch(SCREEN_MAIN);
        }
    } else loadingNextFrame = true;
}

static void Screen_DrawGeneratorChanged(void) {
    SavedGenerator previous, current;
    if (!LocalServer_GetGeneratorChange(&previous, &current)) {
        Screen_Switch(SCREEN_MAIN);
        return;
    }
    Menu_Begin("World generator changed", 2);
    Menu_Text(TextFormat("Saved: %.40s v%d", previous.name, previous.version), -100, 16, LIGHTGRAY);
    Menu_Text(TextFormat("Updated: %.40s v%d", current.name, current.version), -72, 16, WHITE);
    if (!strcmp(previous.name, current.name) && previous.version == current.version)
        Menu_Text("Generation settings changed.", -44, 16, LIGHTGRAY);
    Menu_Text("Saved terrain and builds will be kept.", -8, 18, WHITE);
    Menu_Text("New terrain may have seams along old borders.", 18, 16, LIGHTGRAY);
    if (Menu_Button(64, "Back", true) || Menu_Key(KEY_ESCAPE)) {
        Screen_Switch(SCREEN_MAIN);
        return;
    }
    if (Menu_Button(118, "Use updated generator", true)) {
        acceptGeneratorChange = true;
        Screen_Switch(SCREEN_LOADING);
    }
}

void Screen_Draw(void) {
    screenHeight = GetScreenHeight();
    screenWidth = GetScreenWidth();
    uiColBg = (Color){0, 0, 0, 80};
    int textSize = GuiGetStyle(DEFAULT, TEXT_SIZE);
    switch (currentScreen) {
        case SCREEN_GAME: Screen_DrawGame(); break;
        case SCREEN_INVENTORY: ClientInventory_Draw(true); break;
        case SCREEN_MAIN: Screen_DrawMain(); break;
        case SCREEN_PAUSE: Screen_DrawPause(); break;
        case SCREEN_OPTIONS: Screen_DrawOptions(); break;
        case SCREEN_LOGIN: Screen_DrawLogin(); break;
        case SCREEN_JOINING: Screen_DrawJoining(); break;
        case SCREEN_LOADING: Screen_DrawLoading(); break;
        case SCREEN_GENERATOR_CHANGED: Screen_DrawGeneratorChanged(); break;
        case SCREEN_CONNECTION_ERROR: Screen_DrawConnectionError(); break;
    }
    GuiSetStyle(DEFAULT, TEXT_SIZE, textSize);
    skipMenuKeys = false;
}

void Screen_Switch(Screen screen) {
    if (screen == SCREEN_OPTIONS) { Options_Open(OPTIONS_HOME); optionsReturn = currentScreen; pendingDistance = world.drawDistance; }
    if (screen == SCREEN_LOADING) loadingNextFrame = loadingStarted = false;
    currentScreen = screen;
    memset(hoverAmount, 0, sizeof(hoverAmount));
    keyboardFocus = true;
    focus = 0;
    skipMenuKeys = true;
    screenCursorEnabled = screen != SCREEN_GAME;
    if (screenCursorEnabled) EnableCursor();
    else DisableCursor();
}
