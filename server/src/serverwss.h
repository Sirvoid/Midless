/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SERVER_WEB_SOCKET_H
#define MIDLESS_SERVER_WEB_SOCKET_H

void ServerWss_Init(void);
void ServerWss_Poll(void);
void ServerWss_Send(void *peer, unsigned char* packet, int length);

#endif