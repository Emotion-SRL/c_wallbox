/* this source was made by cluade since i dont feel like doing it by hand */

#include "server_cmd.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <libwebsockets.h>

#include "serial.h"
#include "utils.h"
#include "ws.h"

/* write_serial() is defined in serial.c but not declared in serial.h */
int write_serial(char *write_buffer);

#define CMD_BUF_SIZE 64

/*
  server protocol (see ocpp_manager/src/server/routers/wallbox_router.rs:71-77):
      "<command>"           or
      "<command> <value>"
  examples documented in server_api_handler.rs:
      "setCurrent 16"
      "stop"
*/

int handle_server_command(struct lws *wsi, const char *payload, size_t len)
{
	if (payload == NULL || len == 0)
		return ERR;

	char buf[CMD_BUF_SIZE];
	if (len >= CMD_BUF_SIZE)
		len = CMD_BUF_SIZE - 1;
	memcpy(buf, payload, len);
	buf[len] = '\0';

	/* trim trailing whitespace (newlines, CR, spaces) */
	while (len > 0 && isspace((unsigned char)buf[len - 1]))
		buf[--len] = '\0';
	if (len == 0)
		return ERR;

	/* production server (Python) sends the whole string "set max amps <N>";
	   match it before the generic split, since cmd would otherwise be just "set" */
	{
		static const char *SET_AMPS = "set max amps ";
		size_t plen = strlen(SET_AMPS);
		if (strncmp(buf, SET_AMPS, plen) == 0) {
			const char *amps = buf + plen;
			while (*amps == ' ')
				amps++;
			if (*amps == '\0') {
				LOG_ERR("\tset max amps without value, dropping\n");
				return ERR;
			}
			char serial_buf[CMD_BUF_SIZE];
			LOG_DBG("Server command received: 'set max amps' value: '%s'\n", amps);
			snprintf(serial_buf, sizeof(serial_buf), "max amp %s", amps);
			int rc = write_serial(serial_buf);
			if (rc == NO_ERR) {
				/* max_ampere changed: resend a boot notification so the DB persists it */
				client.on_boot = true;
				lws_callback_on_writable(wsi);
			}
			return rc;
		}
	}

	/* split on first space: cmd + optional value */
	char *value = NULL;
	char *sp = strchr(buf, ' ');
	if (sp != NULL) {
		*sp = '\0';
		value = sp + 1;
		while (*value == ' ')
			value++;
		if (*value == '\0')
			value = NULL;
	}
	const char *cmd = buf;
	LOG_DBG("Server command received: '%s'", cmd);
	if (value != NULL)
		LOG_DBG(" value: '%s'", value);
	LOG_DBG("\n");

	if (strcmp(cmd, "status") == 0) {
		LOG_DBG("\tstatus request: scheduling writable to reply\n");
		lws_callback_on_writable(wsi);
		return NO_ERR;
	}
	char serial_buf[CMD_BUF_SIZE];
	if (strcmp(cmd, "start") == 0) {
		snprintf(serial_buf, sizeof(serial_buf), "start");
	} else if (strcmp(cmd, "stop") == 0) {
		snprintf(serial_buf, sizeof(serial_buf), "stop");
	} else if (strcmp(cmd, "setCurrent") == 0) {
		if (value == NULL) {
			LOG_ERR("\tsetCurrent without value, dropping\n");
			return ERR;
		}
		snprintf(serial_buf, sizeof(serial_buf), "max amp %s", value);
	} else {
		LOG_ERR("\tunknown server command, dropping\n");
		return ERR;
	}

	return write_serial(serial_buf);
}
