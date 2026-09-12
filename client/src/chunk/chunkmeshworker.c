/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "chunkmeshworker.h"
#include "chunkmeshgeneration.h"
#include "world.h"
#include "streamprofile.h"

#define MESH_WORKERS 2
#define MESH_JOBS 8

typedef enum MeshJobState { MESH_IDLE, MESH_QUEUED, MESH_WORKING, MESH_DONE } MeshJobState;

typedef struct MeshJob {
    MeshJobState state;
    Vector3 position;
    uint64_t identity, revision;
    MeshSnapshot snapshot;
    Block *definitions;
    BlockMeshTemplate *templates;
    int definitionCapacity;
    MeshBuffers *buffers;
    double computeSeconds;
} MeshJob;

static MeshJob jobs[MESH_JOBS];
static pthread_t threads[MESH_WORKERS];
static int threadCount;
static pthread_mutex_t meshMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t meshCondition = PTHREAD_COND_INITIALIZER;
static bool running;
static int nextCompletedJob;
static StreamProfile snapshotProfile, computeProfile, uploadProfile, discardedProfile;

static MeshJob *NextJob(void) {
    for (int i = 0; i < MESH_JOBS; i++)
        if (jobs[i].state == MESH_QUEUED) return &jobs[i];
    return NULL;
}

static void *RunMeshWorker(void *unused) {
    (void)unused;
    pthread_mutex_lock(&meshMutex);
    while (running) {
        MeshJob *job;
        while (running && !(job = NextJob()))
            pthread_cond_wait(&meshCondition, &meshMutex);
        if (!running) break;
        job->state = MESH_WORKING;
        pthread_mutex_unlock(&meshMutex);

        double start = GetTime();
        ChunkMeshGeneration_Compute(job->buffers, &job->snapshot,
            job->definitions, job->templates);
        job->computeSeconds = GetTime() - start;

        pthread_mutex_lock(&meshMutex);
        job->state = MESH_DONE;
    }
    pthread_mutex_unlock(&meshMutex);
    return NULL;
}

void ChunkMeshWorker_Init(void) {
    running = true;
    for (int i = 0; i < MESH_JOBS; i++) jobs[i].buffers = ChunkMeshGeneration_CreateBuffers();
    for (int i = 0; i < MESH_WORKERS; i++) {
        if (pthread_create(&threads[threadCount], NULL, RunMeshWorker, NULL) == 0) threadCount++;
    }
    if (!threadCount) TraceLog(LOG_WARNING, "Mesh jobs unavailable; using synchronous fallback");
}

void ChunkMeshWorker_Shutdown(void) {
    pthread_mutex_lock(&meshMutex);
    running = false;
    pthread_cond_broadcast(&meshCondition);
    pthread_mutex_unlock(&meshMutex);
    for (int i = 0; i < threadCount; i++) pthread_join(threads[i], NULL);
    threadCount = 0;
    for (int i = 0; i < MESH_JOBS; i++) {
        MeshJob *job = &jobs[i];
        ChunkMeshGeneration_FreeBuffers(job->buffers);
        free(job->definitions);
        free(job->templates);
        memset(job, 0, sizeof(*job));
    }
}

bool ChunkMeshWorker_HasSpace(void) {
    bool available = false;
    pthread_mutex_lock(&meshMutex);
    for (int i = 0; i < MESH_JOBS; i++)
        if (jobs[i].buffers && jobs[i].state == MESH_IDLE) available = true;
    pthread_mutex_unlock(&meshMutex);
    return available;
}

void ChunkMeshWorker_CancelQueued(void) {
    pthread_mutex_lock(&meshMutex);
    for (int i = 0; i < MESH_JOBS; i++) {
        if (jobs[i].state == MESH_QUEUED || jobs[i].state == MESH_DONE)
            jobs[i].state = MESH_IDLE;
    }
    pthread_mutex_unlock(&meshMutex);
}

static bool TakeSnapshot(MeshJob *job, Chunk *chunk) {
    MeshSnapshot *snapshot = &job->snapshot;
    for (int i = 0; i < CHUNK_SIZE; i++) {
        snapshot->cells[i] = (MeshCell){chunk->data[i], chunk->lightData[i], chunk->sunlightData[i]};
    }
    for (int face = 0; face < 6; face++) {
        Chunk *neighbor = chunk->neighbours[face];
        snapshot->neighbors[face] = neighbor && neighbor->isBlockDataReady;
        for (int row = 0; row < 16; row++) {
            for (int column = 0; column < 16; column++) {
                MeshCell *cell = &snapshot->cells[CHUNK_SIZE + face * 256 + row * 16 + column];
                *cell = (MeshCell){0};
                if (!snapshot->neighbors[face]) continue;
                int x, y, z;
                if (face < 2) {
                    x = face == 0 ? 15 : 0;
                    y = row;
                    z = column;
                } else if (face < 4) {
                    x = column;
                    y = face == 2 ? 0 : 15;
                    z = row;
                } else {
                    x = column;
                    y = row;
                    z = face == 4 ? 0 : 15;
                }
                int index = y * 256 + z * 16 + x;
                *cell = (MeshCell){neighbor->data[index], neighbor->lightData[index], neighbor->sunlightData[index]};
            }
        }
    }

    // Only definitions used by the center or its adjacent boundary cells are copied.
    int mapping[BLOCK_RUNTIME_COUNT];
    for (int i = 0; i < BLOCK_RUNTIME_COUNT; i++) mapping[i] = -1;
    int definitionCount = 0;
    for (int i = 0; i < MESH_SNAPSHOT_CELLS; i++) {
        int id = snapshot->cells[i].block;
        if (mapping[id] == -1) mapping[id] = definitionCount++;
    }
    if (definitionCount > job->definitionCapacity) {
        Block *definitions = malloc(definitionCount * sizeof(*definitions));
        BlockMeshTemplate *templates = malloc(definitionCount * sizeof(*templates));
        if (!definitions || !templates) {
            free(definitions);
            free(templates);
            return false;
        }
        free(job->definitions);
        free(job->templates);
        job->definitions = definitions;
        job->templates = templates;
        job->definitionCapacity = definitionCount;
    }
    for (int id = 0; id < BLOCK_RUNTIME_COUNT; id++) {
        if (mapping[id] < 0) continue;
        job->definitions[mapping[id]] = blockDefinitions[id];
        job->templates[mapping[id]] = *BlockMesh_GetTemplate(id);
    }
    for (int i = 0; i < MESH_SNAPSHOT_CELLS; i++)
        snapshot->cells[i].block = mapping[snapshot->cells[i].block];
    job->position = chunk->position;
    job->identity = chunk->meshIdentity;
    job->revision = chunk->meshRevision;
    return true;
}

bool ChunkMeshWorker_Submit(Chunk *chunk) {
    MeshJob *job = NULL;
    pthread_mutex_lock(&meshMutex);
    for (int i = 0; i < MESH_JOBS; i++) {
        if (jobs[i].buffers && jobs[i].state == MESH_IDLE) {
            job = &jobs[i];
            break;
        }
    }
    pthread_mutex_unlock(&meshMutex);
    // Only the main thread submits jobs. An idle slot stays idle while copied.
    double start = GetTime();
    if (!job || !TakeSnapshot(job, chunk)) return false;
    StreamProfile_Add(&snapshotProfile, "snapshot", GetTime() - start);
    chunk->meshPending = true;
    chunk->isLightDirty = false;
    if (!threadCount) {
        start = GetTime();
        ChunkMeshGeneration_Compute(job->buffers, &job->snapshot, job->definitions, job->templates);
        job->computeSeconds = GetTime() - start;
    }
    pthread_mutex_lock(&meshMutex);
    job->state = threadCount ? MESH_QUEUED : MESH_DONE;
    pthread_cond_broadcast(&meshCondition);
    pthread_mutex_unlock(&meshMutex);
    return true;
}

bool ChunkMeshWorker_ReceiveOne(void) {
    for (int offset = 0; offset < MESH_JOBS; offset++) {
        int i = (nextCompletedJob + offset) % MESH_JOBS;
        MeshJob *job = &jobs[i];
        pthread_mutex_lock(&meshMutex);
        bool done = job->state == MESH_DONE;
        pthread_mutex_unlock(&meshMutex);
        if (!done) continue;
        nextCompletedJob = (i + 1) % MESH_JOBS;
        StreamProfile_Add(&computeProfile, "mesh compute", job->computeSeconds);
        bool uploaded = false;
        Chunk *chunk = World_GetChunkAt(job->position);
        // Identity also rejects results from chunks unloaded and reloaded at the same position.
        if (chunk && chunk->meshIdentity == job->identity) {
            chunk->meshPending = false;
            if (chunk->meshRevision == job->revision && !chunk->isLightDirty && job->buffers->valid) {
                double start = GetTime();
                ChunkMeshGeneration_Upload(job->buffers, chunk);
                StreamProfile_Add(&uploadProfile, "mesh upload", GetTime() - start);
                uploaded = true;
            }
            else World_QueueChunk(chunk, false);
        }
        if (!uploaded) StreamProfile_Add(&discardedProfile, "discarded mesh", job->computeSeconds);
        pthread_mutex_lock(&meshMutex);
        job->state = MESH_IDLE;
        pthread_mutex_unlock(&meshMutex);
        return true;
    }
    return false;
}

void ChunkMeshWorker_Receive(double deadline) {
    while (GetTime() < deadline && ChunkMeshWorker_ReceiveOne()) { }
}
