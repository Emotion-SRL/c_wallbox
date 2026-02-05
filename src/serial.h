#pragma once

#include <termios.h> /* POSIX Terminal Control Definitions */
/*-------------------------------------------------------------*/
/* termios structure -  /usr/include/asm-generic/termbits.h    */
/* use "man termios" to get more info about  termios structure */
/*-------------------------------------------------------------*/

#include "utils.h"

#define AMP_VALUE_OFFSET (sizeof("set max amp"))

typedef enum {
	cmd_start,
	cmd_stop,
	cmd_status,
	cmd_set_amp,
	CMD_COUNT,
	CMD_NOTHING = -1
} type_cmd;

typedef union {
	const char *list[4];
	struct {
		const char *start;
		const char *stop;
		const char *status;
		const char *set_amp;
	};
} serial_cmds;

extern serial_cmds cmds;

/*
  this function allocate size memory on the heap, the programmer
  is responsable for deallocating the buffer when is not needed anymore
 n*/
int serial_init(struct termios *tty, int *fd);
int serial_set_amp(int amp);
int serial_start();
int serial_stop();
int serial_status();

/*this function allocates memory for the pointer <out_string>, the programmer
  is responsable for deallocating the buffer when is not needed anymore*/
int read_serial(int fd, char **out_string, int *out_len);
int serial_extract_amp_from_str(const char *command);
type_cmd serial_command_is_valid(const char *command);
