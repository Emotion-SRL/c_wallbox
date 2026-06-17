#include "ws.h"
/*
  I think this is the documentation that you need
  https://libwebsockets.org/lws-api-doc-main/html/group__client.html
 */
#include <json.h>

#include "utils.h"
#include "serial.h"
#include "server_cmd.h"

ws_client client = {0};

static int protocol_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	switch(reason)
	{
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		lws_callback_on_writable(wsi);
		break;
	case LWS_CALLBACK_CLIENT_RECEIVE:
		handle_server_command(wsi, in, len);
		break;
	case LWS_CALLBACK_CLIENT_WRITEABLE: {
		const char *json = NULL;
		if (client.on_boot) {
			printf("\tsending boot notification...\n");
			json = json_make_message(m_boot);
		} else {
			json = json_make_message(m_status);
		}
		if (json == NULL) {
			printf("\tjson_make_message returned NULL, skipping send\n");
			break;
		}
		client.on_boot = false;
		printf("%s\n", json);
		int json_len = strlen(json);
		unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + json_len + LWS_SEND_BUFFER_POST_PADDING/*this last one is fucking 0*/];
		memcpy(buf + LWS_SEND_BUFFER_PRE_PADDING, json, json_len);
		lws_write(wsi, buf + LWS_SEND_BUFFER_PRE_PADDING, json_len, LWS_WRITE_TEXT);
		break;
	}
	case LWS_CALLBACK_CLIENT_CLOSED:
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		client.web_socket = NULL;
		client.on_boot = true;
		break;
	default:
		break;
	}
	return 0;
}

static struct lws_protocols protocols[] = {
	{
		.name                  = "wallbox",          /* Protocol name*/
		.callback              = protocol_callback,  /* Protocol callback */
		.per_session_data_size = 0,                  /* Protocol callback 'userdata' size */
		.rx_buffer_size        = 0,                  /* Receve buffer size (0 = no restriction) */
		.id                    = 0,                  /* Protocol Id (version) (optional) */
		.user                  = NULL,               /* 'User data' ptr, to access in 'protocol callback */
		.tx_packet_size        = 0                   /* Transmission buffer size restriction (0 = no restriction) */
	},
	LWS_PROTOCOL_LIST_TERM                               /* terminator */
};

void ws_client_init()
{
	/* lws_set_log_level(LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO, NULL); */
	/* first we need to recover the serial-number, we need to include it in the url */
	client.on_boot = true;
	// general ws connection info
	client.info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	client.info.protocols = protocols;
	client.info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	client.info.gid = -1;
	client.info.uid = -1;

	client.context = lws_create_context(&(client.info));
	client.ccinfo.context = client.context;
	client.ccinfo.address = "emotion-projects.eu";
	client.ccinfo.port = 443;
	client.ccinfo.path = "/wallbox";
	client.ccinfo.host = "emotion-projects.eu";
	client.ccinfo.origin = "emotion-projects.eu";
	client.ccinfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
	client.ccinfo.protocol = protocols[PROTOCOL_WALLBOX].name;
}

int ws_connect()
{
	client.web_socket = lws_client_connect_via_info(&(client.ccinfo));
	/* return error if web_socket is NULL */
	return client.web_socket ? NO_ERR : ERR;
}

void ws_on_writable()
{
	if (client.web_socket == NULL)
		return;
	lws_callback_on_writable(client.web_socket);
}

int ws_is_connected()
{
	return client.web_socket ? NO_ERR : ERR;
}

struct lws_context *ws_get_context()
{
	return client.context;
}
