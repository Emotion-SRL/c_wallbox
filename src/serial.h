#pragma once

#include <termios.h> /* POSIX Terminal Control Definitions */
/*-------------------------------------------------------------*/
/* termios structure -  /usr/include/asm-generic/termbits.h    */
/* use "man termios" to get more info about  termios structure */
/*-------------------------------------------------------------*/

#include "utils.h"

/*
  this function allocate size memory on the heap, the programmer
  is responsable for deallocating the buffer when is not needed anymore
 */
int serial_init(struct termios *tty, int *fd);
int write_serial(int fd, BYTE *buffer, int buffer_size);
/*this function allocates memory for the pointer <out_string>, the programmer
  is responsable for deallocating the buffer when is not needed anymore*/
int read_serial(int fd, char **out_string, int *out_len);
