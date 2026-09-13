/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_LOCAL_SERVER_H
#define MIDLESS_CLIENT_LOCAL_SERVER_H

#include <stdbool.h>
#include "../../server/src/savedatabase.h"

bool LocalServer_Start(bool acceptGeneratorChange);
bool LocalServer_GetGeneratorChange(SavedGenerator *previous, SavedGenerator *current);
void LocalServer_Stop(void);
bool LocalServer_IsRunning(void);

#endif
