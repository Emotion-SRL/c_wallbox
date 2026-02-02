#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

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

//3409328527 Beatrice

int main(int argc, char *argv[])
{
	/* the following stuff is just for the development process,
	   it is usefull to have a way to sand a message to the micro
	*/
	if (STR_ARE_EQUAL("sono gay", "sono gay"))
		printf("str are equal!\n");
	else
		printf("str are NOT equal!\n");
	return 0;

	BYTE *command = NULL;
	int cmdlen = 0;
	if (argc == 2) {
		cmdlen = strlen(argv[1]);
		command = malloc(cmdlen + 1);
		memcpy(command, argv[1], cmdlen);
		command[cmdlen] = '\0';
		printf("command %s will be send\n", command);
	} else {
		printf("no command provided, opt to \"status\"\n");
		command = "status";
	}
	//first thing first, let's get the mac address
	printf("Starting client init...\n");
	printf("\tRetriving MAC address\n");
	unsigned char mac[6];
	if (!get_mac_addr(mac)) {
		printf("\tThere was an error on the MAC ADDRESS recover\n");
		// just crash the program, there is no way to recover from this error
		return -1;
	}
	printf("Setting timezone\n");
	struct tm *mt;
	time_t mtt;
	char ftime[sizeof("%d-%m-%Y, %H:%M:%S")];
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
	if (!serial_init(&tty, &fd))
		return -1;
	while (1) {
		write_serial(fd, command, strlen((char *)command));
		char *out_string;
		int out_len;
		read_serial(fd, &out_string, &out_len);
		free(out_string);
		sleep(2);
	}
	return 0;
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
			/* Send a random number to the server every second. */
			lws_callback_on_writable( web_socket );
			old = tv.tv_sec;
		}

		lws_service( context, /* timeout_ms = */ 250 ); /* NOTE: since v3.2, timeout_ms may be set to '0', since it internally ignored */
	}

	lws_context_destroy( context );

	return 0;
}
