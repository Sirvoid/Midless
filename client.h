#ifndef G_CLIENT_H
#define G_CLIENT_H

void *Client_Init(void *state);
void Client_Do(int *state);
void Client_Send(unsigned char* packet, int packetLength);

#endif