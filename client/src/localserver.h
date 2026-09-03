#ifndef G_LOCALSERVER_H
#define G_LOCALSERVER_H

#include <stdbool.h>

bool LocalServer_Start(void);
void LocalServer_Stop(void);
bool LocalServer_IsRunning(void);

#endif
