#ifndef MIDLESS_CLIENT_LOCAL_SERVER_H
#define MIDLESS_CLIENT_LOCAL_SERVER_H

#include <stdbool.h>

bool LocalServer_Start(void);
void LocalServer_Stop(void);
bool LocalServer_IsRunning(void);

#endif
