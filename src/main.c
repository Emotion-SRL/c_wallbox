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
#include "ws.h"

int main(void)
{
	printf("Starting client init...\n");
	if (wallbox_identity_init() == ERR) {
		printf("\tthere was an error on wallbox identity initialization\n");
		return ERR;
	}
	if (local_time_init() == ERR) {
		printf("\tThere was an error on the time setup somehow\n");
		return ERR;
	} else {
		printf("\ttimezone set\n");
		printf("\twe are at %s\n", local_time_get_time());
	}
	ws_client_init();
	struct termios tty;
	int fd;
	if (serial_init(&tty, &fd) != NO_ERR)
		return ERR;
	time_t old = 0;
	struct timeval tv;
	while (1) {
		gettimeofday(&tv, NULL);
		if ((ws_is_connected() == ERR) && tv.tv_sec != old) {
			printf("\tconnecting to server...\n");
			ws_connect();
		}
		if (tv.tv_sec != old) {
			ws_on_writable();
			old = tv.tv_sec;
		}
		lws_service(ws_get_context(), 250);
	}
}
