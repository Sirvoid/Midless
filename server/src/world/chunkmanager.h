/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SERVER_CHUNK_MANAGER_H
#define MIDLESS_SERVER_CHUNK_MANAGER_H

void ServerChunkManager_Init(void);
void ServerChunkManager_Update(void);
void ServerChunkManager_CancelUnusedRequests(void);
void ServerChunkManager_Shutdown(void);

#endif
