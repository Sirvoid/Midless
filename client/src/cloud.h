#ifndef MIDLESS_CLIENT_CLOUD_H
#define MIDLESS_CLIENT_CLOUD_H

#include "raylib.h"

void Cloud_Init(void);
void Cloud_Shutdown(void);
void Cloud_Update(float deltaTime);
void Cloud_Draw(Vector3 cameraPosition, float sunlightStrength);

#endif
