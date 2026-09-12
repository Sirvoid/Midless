#include "blocktimer.h"
#include "../world.h"
#include "scripthooks.h"
#include "stb_ds.h"
#include <stdlib.h>
#include <string.h>

static uint64_t revision;
static int Find(Chunk *chunk, int index) {
    for (int i = 0; i < chunk->timerCount; i++) if (chunk->timers[i].index == index) return i;
    return -1;
}
bool BlockTimer_Start(Chunk *chunk, int index, float interval) {
    int i = Find(chunk, index);
    if (i < 0) {
        BlockTimer *timers = realloc(chunk->timers, (chunk->timerCount + 1) * sizeof(*timers));
        if (!timers) return false;
        chunk->timers = timers;
        i = chunk->timerCount++;
    }
    chunk->timers[i] = (BlockTimer){.index=index, .interval=interval, .revision=++revision};
    return true;
}
void BlockTimer_Stop(Chunk *chunk, int index) {
    int i = Find(chunk, index);
    if (i >= 0) chunk->timers[i] = chunk->timers[--chunk->timerCount];
}
void BlockTimer_Update(float dt) {
    for (int c = 0; c < hmlen(serverWorld.chunks); c++) {
        Chunk *chunk = serverWorld.chunks[c].value;
        if (chunk->savePending) continue;
        BlockTimer due[CHUNK_SIZE];
        int count = 0;
        for (int i = 0; i < chunk->timerCount; i++) {
            BlockTimer *timer = &chunk->timers[i];
            timer->elapsed += dt;
            if (timer->elapsed >= timer->interval) due[count++] = *timer;
        }
        for (int i = 0; i < count; i++) {
            BlockTimer timer = due[i];
            int current = Find(chunk, timer.index);
            if (current < 0 || chunk->timers[current].revision != timer.revision) continue;
            Vector3 pos = ServerChunk_IndexToPos(timer.index);
            pos.x += chunk->position.x * 16; pos.y += chunk->position.y * 16; pos.z += chunk->position.z * 16;
            bool repeat = ScriptHooks_MetadataTimer(pos, timer.elapsed);
            current = Find(chunk, timer.index);
            // Explicit start/stop from the callback takes precedence over its return value.
            if (current < 0 || chunk->timers[current].revision != timer.revision) continue;
            if (repeat) chunk->timers[current].elapsed = 0;
            else BlockTimer_Stop(chunk, timer.index);
        }
    }
}
