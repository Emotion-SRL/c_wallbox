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

/* how often we push a message to the server (seconds):
   - STATUS_PUSH_SECS: normal cadence once we're registered
   - BOOT_RETRY_SECS:  fast retry until the first boot notification goes out
     (right after a cold boot the micro may not answer on serial yet, so the
     boot message is skipped; retry quickly instead of waiting a full cycle) */
#define STATUS_PUSH_SECS 30
#define BOOT_RETRY_SECS   3

int main(void)
{
	/* open the log file first so every trace/error below is mirrored to it.
	   relative path: start_wallbox.sh cd's into the working dir (/root) */
	log_file_init("ws-log.txt");
	LOG_DBG("Starting client init...\n");
	if (wallbox_identity_init() == ERR) {
		LOG_ERR("\tthere was an error on wallbox identity initialization\n");
		return ERR;
	}
	if (local_time_init() == ERR) {
		LOG_ERR("\tThere was an error on the time setup somehow\n");
		return ERR;
	} else {
		LOG_DBG("\ttimezone set\n");
		LOG_DBG("\twe are at %s\n", local_time_get_time());
	}
	ws_client_init();
	struct termios tty;
	int fd;
	if (serial_init(&tty, &fd) != NO_ERR)
		return ERR;
	time_t old = 0;
	time_t last_push = 0;
	struct timeval tv;
	while (1) {
		gettimeofday(&tv, NULL);
		if ((ws_is_connected() == ERR) && tv.tv_sec != old) {
			LOG_DBG("\tconnecting to server...\n");
			ws_connect();
		}
		if (tv.tv_sec != old) {
			old = tv.tv_sec;
		}
		/* poll fast while the first boot is still pending (micro warming up),
		   then relax to the normal cadence once we're registered */
		int push_interval = client.on_boot ? BOOT_RETRY_SECS : STATUS_PUSH_SECS;
		if (tv.tv_sec - last_push >= push_interval) {
			ws_on_writable();
			last_push = tv.tv_sec;
		}
		lws_service(ws_get_context(), 250);
	}
}
