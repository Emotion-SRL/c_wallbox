#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <json.h>

#include "utils.h"
#include "serial.h"

static struct lws *web_socket = NULL;

#define EXAMPLE_TX_BUFFER_BYTES 10

static int callback_example( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len )
{
	switch( reason )
	{
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		lws_callback_on_writable( wsi );
		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
		/* Handle incomming messages here. */
		break;

	case LWS_CALLBACK_CLIENT_WRITEABLE:
	{
		unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + EXAMPLE_TX_BUFFER_BYTES + LWS_SEND_BUFFER_POST_PADDING];
		unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];
		size_t n = sprintf( (char *)p, "%u", rand() );
		lws_write( wsi, p, n, LWS_WRITE_TEXT );
		break;
	}

	case LWS_CALLBACK_CLIENT_CLOSED:
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		web_socket = NULL;
		break;

	default:
		break;
	}

	return 0;
}

enum protocols
{
	PROTOCOL_EXAMPLE = 0,
	PROTOCOL_COUNT
};

static struct lws_protocols protocols[] =
{
	{
		.name                  = "example-protocol", /* Protocol name*/
		.callback              = callback_example,   /* Protocol callback */
		.per_session_data_size = 0,                  /* Protocol callback 'userdata' size */
		.rx_buffer_size        = 0,                  /* Receve buffer size (0 = no restriction) */
		.id                    = 0,                  /* Protocol Id (version) (optional) */
		.user                  = NULL,               /* 'User data' ptr, to access in 'protocol callback */
		.tx_packet_size        = 0                   /* Transmission buffer size restriction (0 = no restriction) */
	},
	LWS_PROTOCOL_LIST_TERM /* terminator */
};

int main(int argc, char *argv[])
{
	struct json_object *jobj;
	char *json_str;
	load_file_nul_str("test.json", &json_str);
	json_deserialize(json_str, &jobj);
	return 0;
	/* the following stuff is just for the development process,
	   it is usefull to have a way to sand a message to the micro
	*/
	//first thing first, let's get the mac address
	printf("Starting client init...\n");
	printf("\tRetriving MAC address\n");
	unsigned char mac[6];
	if (get_mac_addr(mac) == ERR) {
		printf("\tThere was an error on the MAC ADDRESS recover\n");
		// just crash the program, there is no way to recover from this error
		return ERR;
	}

	// SET TIME, move this to utils and a dedicated struct
	printf("Setting timezone\n");
	struct tm *mt;
	time_t mtt;
	char ftime[128];
	setenv("TZ", "Europe/Rome", 1);
	tzset();
	mtt = time(NULL);
	mt = localtime(&mtt);
	strftime(ftime, sizeof(ftime), "%d-%m-%Y, %H:%M:%S", mt);
	printf("\ttimezone set\n");
	printf("\twe are at %s\n", ftime);

	// micro setup
	struct termios tty;
	int fd;
	const char *command = "set max amp 1600";
	type_cmd cmdt = CMD_NOTHING;
	if (serial_init(&tty, &fd) != NO_ERR)
		return ERR;
	// all of this will move to the dedicated ws api
	if ((cmdt = serial_command_is_valid(command)) == CMD_NOTHING) {
		printf("ERROR, serial command is invalid\n");
		return ERR;
	}
	if (cmdt == cmd_set_amp) {
		int amp = 0;
		if ((amp = serial_extract_amp_from_str(command)) == ERR) {
			printf("ERROR, serial command \"set max amp\" is invalid\n");
			return ERR;
		}
		serial_set_amp(amp);
	} else if (cmdt == cmd_start) {
		serial_start();
	} else if (cmdt == cmd_stop) {
		serial_stop();
	} else if (cmdt == cmd_status) {
		serial_status();
	}
	return NO_ERR;
	//write_serial(fd, (BYTE *)command, strlen((char *)command));
	char *out_string;
	int out_len;
	read_serial(fd, &out_string, &out_len);
	free(out_string);
	sleep(2);
	// ========================================
	// ws client
	// ========================================
	struct lws_context_creation_info info;
	memset( &info, 0, sizeof(info) );

	info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	info.protocols = protocols;
	info.gid = -1;
	info.uid = -1;

	struct lws_context *context = lws_create_context(&info);

	time_t old = 0;
	while(1)
	{
		struct timeval tv;
		gettimeofday( &tv, NULL );

		/* Connect if we are not connected to the server. */
		if( !web_socket && tv.tv_sec != old )
		{
			struct lws_client_connect_info ccinfo;
			memset(&ccinfo, 0, sizeof(ccinfo));
			ccinfo.context = context;
			ccinfo.address = "192.168.1.89";
			ccinfo.port = 8000;
			ccinfo.path = "/";
			ccinfo.host = lws_canonical_hostname(context);
			ccinfo.origin = "origin";
			ccinfo.protocol = protocols[PROTOCOL_EXAMPLE].name;

			web_socket = lws_client_connect_via_info(&ccinfo);
		}

		if( tv.tv_sec != old )
		{
			/* Send a random numbrer to the server every second. */
			lws_callback_on_writable( web_socket );
			old = tv.tv_sec;
		}

		lws_service( context, /* timeout_ms = */ 250 ); /* NOTE: since v3.2, timeout_ms may be set to '0', since it internally ignored */
	}

	lws_context_destroy( context );

	return NO_ERR;
}
