#ifndef MIDLESS_CLIENT_PARTICLE_H
#define MIDLESS_CLIENT_PARTICLE_H

#include "raylib.h"

void Particle_Clear(void);
void Particle_SpawnBlockBreak(Vector3 blockPosition, int blockId);
void Particle_Update(float deltaTime);
void Particle_Draw(Camera camera, Texture2D texture);

#endif
