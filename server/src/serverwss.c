/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#if defined(SERVER_WEB_SUPPORT)

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mongoose.h"
#include "serverwss.h"
#include "networkhandler.h"

struct mg_mgr event_manager;

static void ServerWSS_OnOpen(struct mg_connection *client) {
    void *player = Network_InitPlayer(client, true);
    client->fn_data = player;
    Network_Connect(player);
}


static void ServerWSS_OnClose(struct mg_connection *client) {
    void *player = client->fn_data;
    if(player == NULL) return;
    Network_Disconnect(player);
}

static void ServerWSS_OnMessage(struct mg_connection *client, const unsigned char *data, size_t size) {
    void *player = client->fn_data;
    if(player == NULL) return;
    Network_Receive(player, (unsigned char *)data, size);
}

static void ServerWSS_EventHandler(struct mg_connection *client, int ev, void *ev_data, void *fn_data) {
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
        ServerWSS_OnMessage(client, (const unsigned char*)wm->data.ptr, wm->data.len);
    } else if (ev == MG_EV_WS_OPEN) {
        ServerWSS_OnOpen(client);
    } else if (ev == MG_EV_CLOSE) {
        ServerWSS_OnClose(client);
    }

}

static int ServerWSS_GetConnectionsLength(void) {
    int length = 0;
    for (struct mg_connection *c =  event_manager.conns; c != NULL; c = c->next) {
        length++;
    }
    return length - 1;
}

void ServerWSS_Init(void) {
    const char *s_listen_on = "ws://0.0.0.0:8088";
    mg_mgr_init(&event_manager);
    mg_http_listen(&event_manager, s_listen_on, ServerWSS_EventHandler, NULL);
}

void ServerWSS_Poll(void) {
    int amountWSS =  ServerWSS_GetConnectionsLength();
    for(int i = 0; i <= 128 * amountWSS; i++) {
        mg_mgr_poll(&event_manager, 0);
    }
}

void ServerWSS_Send(void *peer, unsigned char* packet, int length) {
    struct mg_connection *connection = (struct mg_connection*)peer;
    mg_ws_send(connection, packet, length, WEBSOCKET_OP_BINARY);
    free(packet);
}

#endif