#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "particle.h"
#include "world.h"
#include "block.h"

#define MAX_BLOCK_PARTICLES 1024
#define BLOCK_BREAK_PARTICLE_COUNT 32
#define PARTICLE_GRAVITY 18.0f

typedef struct BlockParticle {
    Vector3 position;
    Vector3 velocity;
    Rectangle textureRegion;
    float size;
    float lifetime;
    bool active;
} BlockParticle;

static BlockParticle particles[MAX_BLOCK_PARTICLES];
static int nextParticle;

static float Particle_RandomFloat(float min, float max) {
    return min + (max - min) * (GetRandomValue(0, 10000) / 10000.0f);
}

static bool Particle_Collides(Vector3 position, float size) {
    float halfSize = size * 0.5f;
    BoundingBox particleBox = {
        {position.x - halfSize, position.y - halfSize, position.z - halfSize},
        {position.x + halfSize, position.y + halfSize, position.z + halfSize}
    };
    int minX = (int)floorf(particleBox.min.x), maxX = (int)floorf(particleBox.max.x);
    int minY = (int)floorf(particleBox.min.y), maxY = (int)floorf(particleBox.max.y);
    int minZ = (int)floorf(particleBox.min.z), maxZ = (int)floorf(particleBox.max.z);

    for (int y = minY; y <= maxY; y++) {
        for (int z = minZ; z <= maxZ; z++) {
            for (int x = minX; x <= maxX; x++) {
                int blockId = World_GetBlock((Vector3){x, y, z});
                const Block *block = Block_GetDefinition(blockId);
                if (block->colliderType != BLOCK_COLLIDER_SOLID) continue;
                BoundingBox blockBox = {
                    {x + block->minBB.x / 16.0f, y + block->minBB.y / 16.0f, z + block->minBB.z / 16.0f},
                    {x + block->maxBB.x / 16.0f, y + block->maxBB.y / 16.0f, z + block->maxBB.z / 16.0f}
                };
                if (CheckCollisionBoxes(particleBox, blockBox)) return true;
            }
        }
    }
    return false;
}

void Particle_Clear(void) {
    for (int i = 0; i < MAX_BLOCK_PARTICLES; i++) particles[i].active = false;
    nextParticle = 0;
}

void Particle_SpawnBlockBreak(Vector3 blockPosition, int blockId) {
    const Block *block = Block_GetDefinition(blockId);
    Vector3 base = {floorf(blockPosition.x), floorf(blockPosition.y), floorf(blockPosition.z)};
    Vector3 min = Vector3Add(base, Vector3Scale(block->minBB, 1.0f / 16.0f));
    Vector3 max = Vector3Add(base, Vector3Scale(block->maxBB, 1.0f / 16.0f));

    for (int i = 0; i < BLOCK_BREAK_PARTICLE_COUNT; i++) {
        BlockParticle *particle = &particles[nextParticle];
        nextParticle = (nextParticle + 1) % MAX_BLOCK_PARTICLES;
        int gx = i % 3, gy = (i / 9) % 3, gz = (i / 3) % 3;
        float fx = (gx + Particle_RandomFloat(0.25f, 0.75f)) / 3.0f;
        float fy = (gy + Particle_RandomFloat(0.25f, 0.75f)) / 3.0f;
        float fz = (gz + Particle_RandomFloat(0.25f, 0.75f)) / 3.0f;
        int textureId = block->textures[GetRandomValue(0, 5)];

        particle->position = (Vector3){
            min.x + (max.x - min.x) * fx,
            min.y + (max.y - min.y) * fy,
            min.z + (max.z - min.z) * fz
        };
        particle->velocity = (Vector3){
            Particle_RandomFloat(-2.2f, 2.2f), Particle_RandomFloat(2.0f, 5.0f), Particle_RandomFloat(-2.2f, 2.2f)
        };
        particle->textureRegion = (Rectangle){
            (float)((textureId % 16) * 16 + GetRandomValue(0, 3) * 4),
            (float)((textureId / 16) * 16 + GetRandomValue(0, 3) * 4), 4.0f, 4.0f
        };
        particle->size = Particle_RandomFloat(0.12f, 0.2f);
        particle->lifetime = Particle_RandomFloat(0.8f, 1.4f);
        particle->active = true;
    }
}

void Particle_Update(float deltaTime) {
    for (int i = 0; i < MAX_BLOCK_PARTICLES; i++) {
        BlockParticle *particle = &particles[i];
        if (!particle->active) continue;
        particle->lifetime -= deltaTime;
        if (particle->lifetime <= 0.0f) { particle->active = false; continue; }
        particle->velocity.y -= PARTICLE_GRAVITY * deltaTime;

        Vector3 next = particle->position;
        next.x += particle->velocity.x * deltaTime;
        if (Particle_Collides(next, particle->size)) particle->velocity.x *= -0.25f;
        else particle->position.x = next.x;

        next = particle->position;
        next.y += particle->velocity.y * deltaTime;
        if (Particle_Collides(next, particle->size)) {
            particle->velocity.y = particle->velocity.y < 0.0f ? -particle->velocity.y * 0.2f : 0.0f;
            particle->velocity.x *= 0.75f;
            particle->velocity.z *= 0.75f;
        } else particle->position.y = next.y;

        next = particle->position;
        next.z += particle->velocity.z * deltaTime;
        if (Particle_Collides(next, particle->size)) particle->velocity.z *= -0.25f;
        else particle->position.z = next.z;
    }
}

void Particle_Draw(Camera camera, Texture2D texture) {
    for (int i = 0; i < MAX_BLOCK_PARTICLES; i++) {
        const BlockParticle *particle = &particles[i];
        if (!particle->active) continue;
        unsigned char alpha = (unsigned char)(255.0f * Clamp(particle->lifetime / 0.25f, 0.0f, 1.0f));
        DrawBillboardRec(camera, texture, particle->textureRegion, particle->position,
            (Vector2){particle->size, particle->size}, (Color){255, 255, 255, alpha});
    }
}
