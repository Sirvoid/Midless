/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "chunksave.h"
#include "../savefile.h"
#include "streamprofile.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define SAVE_JOBS CHUNK_SAVE_JOBS
#define SAVE_WORKERS 4
#define SAVE_BYTES (16 * 1024 * 1024)

typedef enum SaveState { SAVE_IDLE, SAVE_QUEUED, SAVE_WRITING, SAVE_DONE } SaveState;
typedef struct SaveJob {
    SaveState state;
    Chunk *chunk; // Only the main thread dereferences this pointer.
    char path[160];
    BinaryWriter snapshot;
    bool success;
    double seconds;
} SaveJob;

static SaveJob jobs[SAVE_JOBS];
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
static pthread_cond_t completion = PTHREAD_COND_INITIALIZER;
static pthread_t threads[SAVE_WORKERS];
static int threadCount;
static bool running, started;
static size_t pendingBytes;
static int writeCursor, completionCursor;

static void *WriteChunks(void *unused) {
    pthread_mutex_lock(&mutex);
    for (;;) {
        SaveJob *job = NULL;
        for (int checked = 0; checked < SAVE_JOBS; checked++) {
            int i = (writeCursor + checked) % SAVE_JOBS;
            if (jobs[i].state == SAVE_QUEUED) {
                job = &jobs[i];
                writeCursor = (i + 1) % SAVE_JOBS;
                break;
            }
        }
        if (!job) {
            if (!running) break;
            pthread_cond_wait(&condition, &mutex);
            continue;
        }
        job->state = SAVE_WRITING;
        pthread_mutex_unlock(&mutex);
        double start = GetTime();
        job->success = SaveFile_WriteAtomic(job->path, job->snapshot.data, job->snapshot.size);
        job->seconds = GetTime() - start;
        pthread_mutex_lock(&mutex);
        job->state = SAVE_DONE;
        pthread_cond_signal(&completion);
    }
    pthread_mutex_unlock(&mutex);
    return NULL;
}

bool ChunkSave_Init(void) {
    running = true;
    threadCount = 0;
    for (int i = 0; i < SAVE_WORKERS; i++) {
        if (pthread_create(&threads[threadCount], NULL, WriteChunks, NULL) == 0) threadCount++;
    }
    started = threadCount > 0;
    if (!started) running = false;
    return started;
}

bool ChunkSave_PathPending(const char *path) {
    bool pending = false;
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < SAVE_JOBS; i++)
        if (jobs[i].state != SAVE_IDLE && !strcmp(jobs[i].path, path)) pending = true;
    pthread_mutex_unlock(&mutex);
    return pending;
}

static SaveJob *AvailableJob(const char *path) {
    if (!started || ChunkSave_PathPending(path)) return NULL;
    SaveJob *job = NULL;
    pthread_mutex_lock(&mutex);
    if (pendingBytes < SAVE_BYTES) {
        for (int i = 0; i < SAVE_JOBS; i++) {
            if (jobs[i].state == SAVE_IDLE) { job = &jobs[i]; break; }
        }
    }
    pthread_mutex_unlock(&mutex);
    return job;
}

static void PublishJob(SaveJob *job, const char *path, BinaryWriter snapshot, Chunk *chunk) {
    job->chunk = chunk;
    job->snapshot = snapshot;
    strcpy(job->path, path);
    pthread_mutex_lock(&mutex);
    pendingBytes += snapshot.capacity;
    job->state = SAVE_QUEUED;
    pthread_cond_signal(&condition);
    pthread_mutex_unlock(&mutex);
}

bool ChunkSave_QueueSnapshot(const char *path, BinaryWriter *snapshot) {
    if (strlen(path) >= sizeof(jobs[0].path) || snapshot->failed) return false;
    SaveJob *job = AvailableJob(path);
    if (!job || snapshot->capacity > SAVE_BYTES - pendingBytes) return false;
    PublishJob(job, path, *snapshot, NULL);
    *snapshot = (BinaryWriter){0};
    return true;
}

static bool QueueChunk(Chunk *chunk, bool autosave) {
    if (chunk->savePending) return false;
    char path[160];
    snprintf(path, sizeof(path), "world/%i.%i.%i.dat",
             (int)chunk->position.x, (int)chunk->position.y, (int)chunk->position.z);
    SaveJob *job = AvailableJob(path);
    if (!job) return false;
    // Submission and completion run on the main thread; an idle slot is ours
    // until publication. Lua and entity metadata never run on the disk thread.
    BinaryWriter snapshot = {0};
    if (!EntityPersistence_Encode(chunk, &snapshot) || snapshot.capacity > SAVE_BYTES - pendingBytes) {
        free(snapshot.data);
        return false;
    }
    // Autosaves own bytes, not live chunks, and must not freeze entity/timer updates.
    if (!autosave) chunk->savePending = true;
    PublishJob(job, path, snapshot, autosave ? NULL : chunk);
    return true;
}

bool ChunkSave_Queue(Chunk *chunk) { return QueueChunk(chunk, false); }
bool ChunkSave_Autosave(Chunk *chunk) { return QueueChunk(chunk, true); }

void ChunkSave_Poll(void (*completed)(Chunk *, bool, const BinaryWriter *)) {
    ChunkSave_PollUntil(completed, INFINITY);
}

void ChunkSave_PollUntil(void (*completed)(Chunk *, bool, const BinaryWriter *), double deadline) {
    static StreamProfile profile;
    // Limit snapshot validation and entity unloading on the main thread too.
    for (int checked = 0; checked < SAVE_JOBS; checked++) {
        int i = completionCursor;
        completionCursor = (completionCursor + 1) % SAVE_JOBS;
        SaveJob *job = &jobs[i];
        pthread_mutex_lock(&mutex);
        bool done = job->state == SAVE_DONE;
        pthread_mutex_unlock(&mutex);
        if (!done) continue;
        if (job->chunk) job->chunk->savePending = false;
        StreamProfile_Add(&profile, "disk save", job->seconds);
        if (!job->success) TraceLog(LOG_ERROR, "Could not save %s; live chunk retained", job->path);
        if (completed && job->chunk) completed(job->chunk, job->success, &job->snapshot);
        pthread_mutex_lock(&mutex);
        pendingBytes -= job->snapshot.capacity;
        free(job->snapshot.data);
        *job = (SaveJob){0};
        pthread_mutex_unlock(&mutex);
        if (GetTime() >= deadline) break;
    }
}

void ChunkSave_Flush(void (*completed)(Chunk *, bool, const BinaryWriter *)) {
    if (!started) return;
    // Used only after simulation stops. Regular updates keep polling without waiting.
    while (pendingBytes) {
        ChunkSave_Poll(completed);
        pthread_mutex_lock(&mutex);
        bool ready = false;
        for (int i = 0; i < SAVE_JOBS; i++) ready |= jobs[i].state == SAVE_DONE;
        if (pendingBytes && !ready) pthread_cond_wait(&completion, &mutex);
        pthread_mutex_unlock(&mutex);
    }
}

void ChunkSave_Shutdown(void) {
    if (!started) return;
    pthread_mutex_lock(&mutex);
    running = false;
    pthread_cond_broadcast(&condition);
    pthread_mutex_unlock(&mutex);
    for (int i = 0; i < threadCount; i++) pthread_join(threads[i], NULL);
    threadCount = 0;
    for (int i = 0; i < SAVE_JOBS; i++) {
        if (jobs[i].chunk) jobs[i].chunk->savePending = false;
        free(jobs[i].snapshot.data);
        jobs[i] = (SaveJob){0};
    }
    pendingBytes = 0;
    writeCursor = completionCursor = 0;
    started = false;
}
