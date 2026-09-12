/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef RUNTIME_PATHS_H
#define RUNTIME_PATHS_H

#include "raylib.h"

static inline bool RuntimePaths_Init(void) {
#if !defined(PLATFORM_WEB)
    const char *directory = GetApplicationDirectory();
    if (directory == NULL || directory[0] == '\0' || !ChangeDirectory(directory)) {
        TraceLog(LOG_ERROR, "Could not set the working directory to the executable folder");
        return false;
    }
#endif
    return true;
}

#endif
