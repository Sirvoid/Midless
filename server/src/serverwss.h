/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef S_SERVERWSS_H
#define S_SERVERWSS_H

void ServerWSS_Init(void);
void ServerWSS_Poll(void);
void ServerWSS_Send(void *peer, unsigned char* packet, int length);

#endif