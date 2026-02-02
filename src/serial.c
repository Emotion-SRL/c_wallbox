#include "serial.h"

#include <stdio.h>
#include <fcntl.h>   /* File Control Definitions           */
#include <termios.h> /* POSIX Terminal Control Definitions */
#include <unistd.h>  /* UNIX Standard Definitions 	   */
#include <errno.h>   /* ERROR Number Definitions           */
#include <stdlib.h>
#include <string.h>

#include "utils.h"

#ifdef DEBUG
#define DEBUG_SERIAL_SIZE (255)
#endif

static struct {
	int fd;
	struct termios *tty;
} serial;

int serial_init(struct termios *tty, int *fd)
{
	if (tty == NULL)
		return -1;
	printf("Microcontroller setup\n");
	*fd = open("/dev/ttyS1", O_RDWR | O_NOCTTY);
	if (*fd == -1) {
		printf("\tfailed to open serial comunication\n");
		return -1;
	}
	tcgetattr(*fd, tty);
	cfsetispeed(tty, B9600);
	cfsetispeed(tty, B9600); /* Set Read  Speed as 9600                       */
	cfsetospeed(tty, B9600); /* Set Write Speed as 9600                       */

	tty->c_cflag &= ~PARENB;   /* Disables the Parity Enable bit(PARENB),So No Parity   */
	tty->c_cflag &= ~CSTOPB;   /* CSTOPB = 2 Stop bits,here it is cleared so 1 Stop bit */
	tty->c_cflag &= ~CSIZE;	 /* Clears the mask for setting the data size             */
	tty->c_cflag |=  CS8;      /* Set the data bits = 8                                 */
	tty->c_cflag &= ~CRTSCTS;       /* No Hardware flow Control                         */
	tty->c_cflag |= CREAD | CLOCAL; /* Enable receiver,Ignore Modem Control lines       */
	tty->c_iflag &= ~(IXON | IXOFF | IXANY);          /* Disable XON/XOFF flow control both i/p and o/p */
	tty->c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  /* Non Cannonical mode                            */

	tty->c_oflag &= ~OPOST;/*No Output Processing*/

	/* Setting Time outs */
	tty->c_cc[VMIN] = 1;
	tty->c_cc[VTIME] = 10; /* Wait indefinetly   */

	if((tcsetattr(*fd, TCSANOW, tty)) != 0) { /* Set the attributes to the termios structure */
		printf("\tERROR! in Setting attributes");
		return -1;
	} else {
		printf("\tBaudRate = 9600\n\tStopBits = 1\n\tParity = none\n");
		return 1;
	}
}

int write_serial(int fd, BYTE *write_buffer, int buffer_size)
{
	printf("Writing on serial\n");
	int bytes_written = 0;	/* Value for storing the number of bytes written to the port */
	bytes_written = write(fd, write_buffer, buffer_size);
	if (bytes_written == -1) {
		printf("\tError on write on serial port\n");
		close(fd);
		return 0;
	} else {
		printf("\tBuffer written on serial, bytes written: %d\n", bytes_written);
		printf("\tsize of to write buffer: %d\n", buffer_size);
	}
	printf("+----------------------------------+\n\n");
	return 0;
}

int read_serial(int fd, char **out_string, int *out_len)
{
	printf("Reading from serial\n");
	// this specific malloc is just for testing
	// BYTE *read_buffer = malloc(DEBUG_SERIAL_SIZE);
	if (out_string == NULL) {
		printf("\tFATAL ERROR on reading serial, out_string is NULL, addr: %ld", out_string);
		return -1;
	}
	if (out_len == NULL) {
		printf("\tFATAL ERROR on reading serial, out_len ptr is NULL");
		return -1;
	}
	char ch = 0;
	dyn_array *buffer = da_alloc(sizeof(char));
	int bytes_read = 0;
	while (1) {
		if ((bytes_read = read(fd, &ch, 1)) == -1 || bytes_read == 0) {
			printf("\tERROR in read_serial, read failed\n");
			return -1;
		}
		if (bytes_read == 0 || ch == '\n') {
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
	printf("\read string: %s\n", *out_string);
	//tcflush(fd, TCIFLUSH);
	printf("+----------------------------------+\n\n");
	return 0;
}
