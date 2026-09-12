/**
 * Copyright (c) 2022 Sirvoid
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
#include "client.h"
#include "networkhandler.h"

EMSCRIPTEN_WEBSOCKET_T socket;
static bool webBusy;

bool Client_IsBusy(void) { return webBusy; }
void Client_Stop(void) { ClientWs_Disconnect(); }
void Client_Shutdown(void) { Client_Stop(); }
bool Client_Start(void) {
    if (webBusy) return false;
    webBusy = true;
    ClientWs_Init(NULL);
    return true;
}

EM_BOOL WebSocketOpen(int eventType, const EmscriptenWebSocketOpenEvent *e, void *userData)
{
	printf("open(eventType=%d, userData=%ld)\n", eventType, (long)userData);
	if (e->socket != socket || !webBusy) return 0;
	Network_Connect();

	return 0;
}

EM_BOOL WebSocketClose(int eventType, const EmscriptenWebSocketCloseEvent *e, void *userData)
{
	printf("close(eventType=%d, wasClean=%d, code=%d, reason=%s, userData=%ld)\n", eventType, e->wasClean, e->code, e->reason, (long)userData);
	if (e->socket != socket || !webBusy) return 0;
	Network_Disconnect();
	networkConnectedToServer = 0;
	return 0;
}

EM_BOOL WebSocketError(int eventType, const EmscriptenWebSocketErrorEvent *e, void *userData)
{
	printf("error(eventType=%d, userData=%ld)\n", eventType, (long)userData);
	if (e->socket == socket && webBusy) Network_Disconnect();
	return 0;
}

EM_BOOL WebSocketMessage(int eventType, const EmscriptenWebSocketMessageEvent *e, void *userData)
{
	if (e->socket == socket && webBusy) Network_Receive((unsigned char*)e->data, e->numBytes);
	return 0;
}

void ClientWs_Disconnect(void) {
	webBusy = false;
	if (socket > 0) {
		emscripten_websocket_close(socket, 1000, "");
		emscripten_websocket_delete(socket);
		socket = 0;
	}
}

void *ClientWs_Init(void *state) {
    
    networkClientSend = &ClientWs_Send;
	networkClientDisconnect = &ClientWs_Disconnect;
	Network_Init();
    ClientWs_Do();

    return NULL;
}

void ClientWs_Do(void) {
    if (!emscripten_websocket_is_supported())
	{
		printf("WebSockets are not supported, cannot continue!\n");
		Network_Disconnect();
		return;
	}

	EmscriptenWebSocketCreateAttributes attr;
	emscripten_websocket_init_create_attributes(&attr);

    char url[160];
	snprintf(url, sizeof(url), "wss://%s/", networkFullAddress);
	attr.url = url;
	attr.protocols = "binary";

	socket = emscripten_websocket_new(&attr);
	if (socket <= 0)
	{
		printf("WebSocket creation failed, error code %d!\n", (EMSCRIPTEN_RESULT)socket);
		Network_Disconnect();
		return;
	}

	emscripten_websocket_set_onopen_callback(socket,  NULL, WebSocketOpen);
	emscripten_websocket_set_onclose_callback(socket,  NULL, WebSocketClose);
	emscripten_websocket_set_onerror_callback(socket,  NULL, WebSocketError);
	emscripten_websocket_set_onmessage_callback(socket, NULL, WebSocketMessage);
}

void ClientWs_Send(unsigned char* packet, int packetLength) {
	emscripten_websocket_send_binary(socket, packet, packetLength);
}

#endif