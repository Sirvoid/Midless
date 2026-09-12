/**
 * Copyright (c) 2026 Sirvoid
 * Released under the MIT License. https://opensource.org/licenses/MIT
 */
#include "serverconfig.h"
#include "world/world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

ServerConfig serverConfig = {64, 16, 25565};

static char *Trim(char *text) {
    while (isspace((unsigned char)*text)) {
        text++;
    }

    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        end--;
        *end = 0;
    }

    return text;
}

bool ServerConfig_Load(const char *path) {
    ServerConfig parsed = {64, 16, 25565};
    FILE *file = fopen(path, "r");
    if (!file) {
        if (errno != ENOENT) {
            fprintf(stderr, "Cannot read %s: %s\n", path, strerror(errno));
            return false;
        }
        file = fopen(path, "w");
        if (!file) {
            fprintf(stderr, "Cannot create %s: %s\n", path, strerror(errno));
            return false;
        }

        bool ok = fprintf(file,
            "# Dedicated server settings; restart the server after editing.\n"
            "# max_players: 1-%d; max_render_distance: 2-32 chunks; port: 1-65535\n"
            "max_players=64\nmax_render_distance=16\nport=25565\n", WORLD_MAX_PLAYERS) >= 0;
        if (fclose(file)) {
            ok = false;
        }
        if (!ok) {
            fprintf(stderr, "Cannot write %s\n", path);
            return false;
        }

        serverConfig = parsed;
        return true;
    }

    char line[512];
    int lineNumber = 0;
    bool ok = true;
    while (fgets(line, sizeof(line), file)) {
        lineNumber++;
        if (!strchr(line, '\n') && !feof(file)) {
            ok = false;
            break;
        }

        char *comment = strchr(line, '#');
        if (comment) {
            *comment = 0;
        }

        char *key = Trim(line);
        if (!*key) {
            continue;
        }

        char *value = strchr(key, '=');
        if (!value) {
            ok = false;
            break;
        }

        *value = 0;
        value++;
        key = Trim(key);
        value = Trim(value);

        int *target;
        int minimum = 1;
        int maximum;
        if (!strcmp(key, "max_players")) {
            target = &parsed.maxPlayers;
            maximum = WORLD_MAX_PLAYERS;
        } else if (!strcmp(key, "max_render_distance")) {
            target = &parsed.maxRenderDistance;
            minimum = 2;
            maximum = 32;
        } else if (!strcmp(key, "port")) {
            target = &parsed.port;
            maximum = 65535;
        } else {
            ok = false;
            break;
        }

        errno = 0;
        char *end;
        long number = strtol(value, &end, 10);
        if (errno || end == value || *end || number < minimum || number > maximum) {
            ok = false;
            break;
        }

        *target = (int)number;
    }

    if (ferror(file)) {
        ok = false;
    }
    if (fclose(file)) {
        ok = false;
    }
    if (!ok) {
        fprintf(stderr, "Invalid or unreadable %s at line %d\n", path, lineNumber);
        return false;
    }

    serverConfig = parsed;
    return true;
}
