/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_CLIENT_H
#define G_CLIENT_H

void *Client_Init(void *state);
void Client_Do(int *state);
void Client_Send(unsigned char* packet, int packetLength);

#endif