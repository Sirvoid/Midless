/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_CLIENTWS_H
#define G_CLIENTWS_H

void ClientWS_Disconnect(void);
void *ClientWS_Init(void *state);
void ClientWS_Do(void);
void ClientWS_Send(unsigned char* packet, int packetLength);

#endif