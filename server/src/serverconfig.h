/**
 * Copyright (c) 2026 Sirvoid
 * Released under the MIT License. https://opensource.org/licenses/MIT
 */
#ifndef MIDLESS_SERVER_CONFIG_H
#define MIDLESS_SERVER_CONFIG_H
#include <stdbool.h>
typedef struct ServerConfig {
    int maxPlayers, maxRenderDistance, port;
} ServerConfig;
extern ServerConfig serverConfig;
bool ServerConfig_Load(const char *path);
#endif
