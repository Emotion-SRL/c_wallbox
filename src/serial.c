#include "serial.h"

#include <stdio.h>
#include <fcntl.h>   /* File Control Definitions           */
#include <termios.h> /* POSIX Terminal Control Definitions */
#include <unistd.h>  /* UNIX Standard Definitions          */
#include <errno.h>   /* ERROR Number Definitions           */
#include <stdlib.h>
#include <string.h>

#include "utils.h"

serial_cmds cmds = {
	.start   = "start",
	.stop    = "stop",
	.status  = "status",
	.set_amp = "set max amp",
};

#ifdef DEBUG
#define DEBUG_SERIAL_SIZE (255)
#endif

int serial_init(struct termios *tty, int *fd)
{
	if (tty == NULL)
		return ERR;
	printf("Microcontroller setup\n");
	*fd = open("/dev/ttyS1", O_RDWR | O_NOCTTY);
	if (*fd == -1) {
		printf("\tfailed to open serial comunication\n");
		return ERR;
	}
	tcgetattr(*fd, tty);
	cfsetispeed(tty, B9600);
	cfsetispeed(tty, B9600); /* Set Read  Speed as 9600                       */
	cfsetospeed(tty, B9600); /* Set Write Speed as 9600                       */

	tty->c_cflag &= ~PARENB;   /* Disables the Parity Enable bit(PARENB),So No Parity   */
	tty->c_cflag &= ~CSTOPB;   /* CSTOPB = 2 Stop bits,here it is cleared so 1 Stop bit */
	tty->c_cflag &= ~CSIZE;    /* Clears the mask for setting the data size             */
	tty->c_cflag |=  CS8;      /* Set the data bits = 8                                 */
	tty->c_cflag &= ~CRTSCTS;       /* No Hardware flow Control                         */
	tty->c_cflag |= CREAD | CLOCAL; /* Enable receiver,Ignore Modem Control lines       */
	tty->c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR | IGNCR); /* Disable XON/XOFF flow control and CR/NL translations */
	tty->c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  /* Non Cannonical mode                            */

	tty->c_oflag &= ~OPOST;/*No Output Processing*/

	/* Setting Time outs */
	tty->c_cc[VMIN] = 0;
	tty->c_cc[VTIME] = 50;

	if((tcsetattr(*fd, TCSANOW, tty)) != 0) { /* Set the attributes to the termios structure */
		printf("\tERROR! in Setting attributes");
		return ERR;
	} else {
		PRINTF_DEBUG("\tBaudRate = 9600\n\tStopBits = 1\n\tParity = none\n");
		return NO_ERR;
	}
}

int serial_set_amp(int amp)
{
	const int MIN_AMP = 6;
	const int MAX_AMP = 32;
	const int DIVISOR = 100;
	amp /= DIVISOR;
	if (amp < MIN_AMP || amp > MAX_AMP) {
		printf("set amp error, ampere[%d] is invalid, opt to min[%d]\n", amp, MIN_AMP);
		amp = MIN_AMP;
	}

	char command[BUFF_MAX_SIZE] = {0};
	sprintf(command, "set amp %d%c", amp, '\0');
	printf("command set amp generated is: %s\n", command);
	return NO_ERR;
}

int serial_start()
{
	printf("sending START\n");
	return NO_ERR;
}

int serial_stop()
{
	printf("sending STOP\n");
	return NO_ERR;
}

int serial_status()
{
	printf("sending STATUS\n");
	return NO_ERR;
}

// command should be validated outside of the serial?
/*
  yes! Because the serial api only expose specific commands function
 */
int write_serial(int fd, BYTE *write_buffer, int buffer_size)
{
	printf("Writing on serial\n");
	if (write_buffer == NULL) {
		printf("\tFATAL ERROR on write serial, write buffer is NULL\n");
		return ERR;
	}
	if (serial_command_is_valid((char *)write_buffer) == CMD_NOTHING) {
		printf("\tFATAL ERROR on write serial, write buffer is invalid\n");
		return ERR;
	}
	int bytes_written = 0;	/* Value for storing the number of bytes written to the port */
	bytes_written = write(fd, write_buffer, buffer_size);
	if (bytes_written == -1) {
		printf("\tError on write on serial port\n");
		close(fd);
		return ERR;
	} else {
		printf("\tBuffer written on serial, bytes written: %d\n", bytes_written);
		printf("\tsize of to write buffer: %d\n", buffer_size);
	}
	printf("+----------------------------------+\n\n");
	return NO_ERR;
}

int read_serial(int fd, char **out_string, int *out_len)
{
	printf("Reading from serial\n");
	// this specific malloc is just for testing
	// BYTE *read_buffer = malloc(DEBUG_SERIAL_SIZE);
	if (out_string == NULL) {
		printf("\tFATAL ERROR on reading serial, out_string is NULL");
		return ERR;
	}
	if (out_len == NULL) {
		printf("\tFATAL ERROR on reading serial, out_len ptr is NULL");
		return ERR;
	}
	char ch = 0;
	dyn_array *buffer = da_alloc(sizeof(char));
	int bytes_read = 0;
	while (1) {
		if ((bytes_read = read(fd, &ch, 1)) == -1) {
			printf("\tERROR in read_serial, read failed\n");
			return ERR;
		}
		if (bytes_read == 0) // this is fine, nothing to read
			break;
		if (ch == '\n') {
			printf("\tDone reading, last byte whas %.2X, bytes_read is %d\n", ch, bytes_read);
			fflush(stdout);
			break;
		}
		da_append(buffer, (BYTE *)&ch);
	}
	*out_string = calloc(buffer->count + 1, sizeof(char));
	memcpy((void *)(*out_string), (void *)buffer->data, buffer->count); // avoiding buffer->el_size multiplication, since sizeof char is 1
	(*out_string)[buffer->count] = '\0';
	*out_len = buffer->count;
	DA_FREE_AND_NULL(buffer);
	if (bytes_read != 0)
		printf("read string: %s\n", *out_string);
	//tcflush(fd, TCIFLUSH);
	printf("+----------------------------------+\n\n");
	return NO_ERR;
}

int serial_extract_amp_from_str(const char *command)
{
	if (command == NULL)
		return ERR;
	int amp = 0;
	if (ascii_to_int(command + AMP_VALUE_OFFSET, &amp) == NO_ERR)
		return amp;
	return ERR;
}

//set max amp 16
type_cmd serial_command_is_valid(const char *command)
{
	if (STR_ARE_EQUAL(command, cmds.status))     return cmd_status;
	else if (STR_ARE_EQUAL(command, cmds.start)) return cmd_start;
	else if (STR_ARE_EQUAL(command, cmds.stop))  return cmd_stop;
	else if (strncmp(command, cmds.set_amp, strlen(cmds.set_amp)) == 0) return cmd_set_amp;
	return CMD_NOTHING;
	/* the last control of the above if check if the command starts with
	   the string "set max amp"*/
}
