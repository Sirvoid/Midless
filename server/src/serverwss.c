/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#if defined(SERVER_WEB_SUPPORT)

#if defined(OS_WINDOWS)
    // Mongoose needs Winsock, but it collides with Raylib
    #define WIN32_LEAN_AND_MEAN
    #define NOGDI
    #define NOUSER
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mongoose.h"
#include "serverwss.h"
#include "networkhandler.h"
#include "player.h"

struct mg_mgr eventManager;

static void ServerWss_OnOpen(struct mg_connection *client) {
    void *player = ServerPlayer_Create(client, true);
    client->fn_data = player;
    ServerNetwork_Connect(player);
}


static void ServerWss_OnClose(struct mg_connection *client) {
    void *player = client->fn_data;
    if(player == NULL) return;
    ServerNetwork_Disconnect(player);
}

static void ServerWss_OnMessage(struct mg_connection *client, const unsigned char *data, size_t size) {
    void *player = client->fn_data;
    if(player == NULL) return;
    ServerNetwork_Receive(player, (unsigned char *)data, size);
}

static void ServerWss_EventHandler(struct mg_connection *client, int ev, void *ev_data, void *fn_data) {
    if (ev == MG_EV_ACCEPT) {
        struct mg_tls_opts opts = {
            .cert = "cert.pem",    // Certificate file
            .certkey = "key.pem",  // Private key file
        };
        mg_tls_init(client, &opts);
    } else if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        mg_ws_upgrade(client, hm, NULL);
    } else if (ev == MG_EV_WS_MSG) {
        struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
        ServerWss_OnMessage(client, (const unsigned char*)wm->data.ptr, wm->data.len);
    } else if (ev == MG_EV_WS_OPEN) {
        ServerWss_OnOpen(client);
    } else if (ev == MG_EV_CLOSE) {
        ServerWss_OnClose(client);
    }

}

static int ServerWss_GetConnectionsLength(void) {
    int length = 0;
    for (struct mg_connection *c =  eventManager.conns; c != NULL; c = c->next) {
        length++;
    }
    return length - 1;
}

void ServerWss_Init(void) {
    const char *listenAddress = "ws://0.0.0.0:8088";
    mg_mgr_init(&eventManager);
    mg_http_listen(&eventManager, listenAddress, ServerWss_EventHandler, NULL);
}

void ServerWss_Poll(void) {
    int connectionCount =  ServerWss_GetConnectionsLength();
    for(int i = 0; i <= 128 * connectionCount; i++) {
        mg_mgr_poll(&eventManager, 0);
    }
}

void ServerWss_Send(void *peer, unsigned char* packet, int length) {
    struct mg_connection *connection = (struct mg_connection*)peer;
    mg_ws_send(connection, packet, length, WEBSOCKET_OP_BINARY);
}

#endif
