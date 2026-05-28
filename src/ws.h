#pragma once

#include <libwebsockets.h>
#include <stdbool.h>

#define EXAMPLE_TX_BUFFER_BYTES 10

enum protocols
{
	PROTOCOL_WALLBOX = 0,
	PROTOCOL_COUNT
};

typedef struct {
	struct lws *web_socket;
	struct lws_context_creation_info info;
	struct lws_context *context;
	struct lws_client_connect_info ccinfo;
	const char *address;
	int port;
	const char *path;
	//--------------------
	bool on_boot;
} ws_client;

extern ws_client client;

void ws_client_init();
int ws_connect();
void ws_on_writable();
int ws_is_connected();
struct lws_context *ws_get_context();
