/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_WEB_SOCKET_H
#define MIDLESS_CLIENT_WEB_SOCKET_H

void ClientWs_Disconnect(void);
void *ClientWs_Init(void *state);
void ClientWs_Do(void);
void ClientWs_Send(unsigned char* packet, int packetLength);

#endif