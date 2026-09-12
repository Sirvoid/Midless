/**
 * Copyright (c) 2021 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_TRANSPORT_H
#define MIDLESS_CLIENT_TRANSPORT_H

#include <stdbool.h>

bool Client_Start(void);
bool Client_IsBusy(void);
void Client_Stop(void);
void Client_Shutdown(void);

void *Client_Init(void *state);
void Client_Do(int *state);
void Client_Send(unsigned char* packet, int packetLength);

#endif