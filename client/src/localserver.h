/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_LOCAL_SERVER_H
#define MIDLESS_CLIENT_LOCAL_SERVER_H

#include <stdbool.h>

bool LocalServer_Start(void);
void LocalServer_Stop(void);
bool LocalServer_IsRunning(void);

#endif
