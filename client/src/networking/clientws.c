/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#if defined(PLATFORM_WEB)

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <emscripten/websocket.h>
#include "clientws.h"
#include "networkhandler.h"

EMSCRIPTEN_WEBSOCKET_T socket;

EM_BOOL WebSocketOpen(int eventType, const EmscriptenWebSocketOpenEvent *e, void *userData)
{
	printf("open(eventType=%d, userData=%ld)\n", eventType, (long)userData);
	Network_Connect();

	return 0;
}

EM_BOOL WebSocketClose(int eventType, const EmscriptenWebSocketCloseEvent *e, void *userData)
{
	printf("close(eventType=%d, wasClean=%d, code=%d, reason=%s, userData=%ld)\n", eventType, e->wasClean, e->code, e->reason, (long)userData);
	Network_Disconnect();
	Network_connectedToServer = 0;
	return 0;
}

EM_BOOL WebSocketError(int eventType, const EmscriptenWebSocketErrorEvent *e, void *userData)
{
	printf("error(eventType=%d, userData=%ld)\n", eventType, (long)userData);
	return 0;
}

EM_BOOL WebSocketMessage(int eventType, const EmscriptenWebSocketMessageEvent *e, void *userData)
{
	Network_Receive((unsigned char*)e->data, e->numBytes);
	return 0;
}

void ClientWS_Disconnect(void) {
	emscripten_websocket_close(socket, 1000, "");
}

void *ClientWS_Init(void *state) {
    
    Network_Internal_Client_Send = &ClientWS_Send;
	Network_Internal_Client_Disconnect = &ClientWS_Disconnect;
	Network_Init();
    ClientWS_Do();

    return NULL;
}

void ClientWS_Do(void) {
    if (!emscripten_websocket_is_supported())
	{
		printf("WebSockets are not supported, cannot continue!\n");
		exit(1);
	}

	EmscriptenWebSocketCreateAttributes attr;
	emscripten_websocket_init_create_attributes(&attr);

    char url[128] = "wss://";
	strcat(url, Network_fullAddress);
	strcat(url, "/");
	attr.url = url;
	attr.protocols = "binary";

	socket = emscripten_websocket_new(&attr);
	if (socket <= 0)
	{
		printf("WebSocket creation failed, error code %d!\n", (EMSCRIPTEN_RESULT)socket);
		exit(1);
	}

	emscripten_websocket_set_onopen_callback(socket,  NULL, WebSocketOpen);
	emscripten_websocket_set_onclose_callback(socket,  NULL, WebSocketClose);
	emscripten_websocket_set_onerror_callback(socket,  NULL, WebSocketError);
	emscripten_websocket_set_onmessage_callback(socket, NULL, WebSocketMessage);
}

void ClientWS_Send(unsigned char* packet, int packetLength) {
	emscripten_websocket_send_binary(socket, packet, packetLength);
}

#endif