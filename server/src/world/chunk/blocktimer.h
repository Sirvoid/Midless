#ifndef MIDLESS_BLOCK_TIMER_H
#define MIDLESS_BLOCK_TIMER_H
#include <stdbool.h>
#include <stdint.h>

typedef struct BlockTimer {
    uint16_t index;
    float interval, elapsed;
    uint64_t revision;
} BlockTimer;
struct Chunk;
bool BlockTimer_Start(struct Chunk *chunk, int index, float interval);
void BlockTimer_Stop(struct Chunk *chunk, int index);
void BlockTimer_Update(float dt);
#endif
