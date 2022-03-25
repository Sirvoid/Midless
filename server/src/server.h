/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_SERVER_H
#define G_SERVER_H

void *Server_Init(void *state);
void Server_Do(int *state);
void Server_Send(void *peer, unsigned char* packet, int length);

#endif