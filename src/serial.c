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
	.set_amp = "max amp",
};

#ifdef DEBUG
#define DEBUG_SERIAL_SIZE (255)
#endif

static struct {
	int fd;
	struct termios tty;
} serial_com;

int serial_init()
{
	printf("Microcontroller setup\n");
	serial_com.fd = open("/dev/ttyS1", O_RDWR | O_NOCTTY);
	if (serial_com.fd == -1) {
		printf("\tfailed to open serial comunication\n");
		return ERR;
	}
	tcgetattr(serial_com.fd, &serial_com.tty);
	cfsetispeed(&serial_com.tty, B9600);
	cfsetispeed(&serial_com.tty, B9600); /* Set Read  Speed as 9600                       */
	cfsetospeed(&serial_com.tty, B9600); /* Set Write Speed as 9600                       */

	serial_com.tty.c_cflag &= ~PARENB;   /* Disables the Parity Enable bit(PARENB),So No Parity   */
	serial_com.tty.c_cflag &= ~CSTOPB;   /* CSTOPB = 2 Stop bits,here it is cleared so 1 Stop bit */
	serial_com.tty.c_cflag &= ~CSIZE;    /* Clears the mask for setting the data size             */
	serial_com.tty.c_cflag |=  CS8;      /* Set the data bits = 8                                 */
	serial_com.tty.c_cflag &= ~CRTSCTS;       /* No Hardware flow Control                         */
	serial_com.tty.c_cflag |= CREAD | CLOCAL; /* Enable receiver,Ignore Modem Control lines       */
	serial_com.tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR | IGNCR); /* Disable XON/XOFF flow control and CR/NL translations */
	serial_com.tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  /* Non Cannonical mode                            */
	serial_com.tty.c_oflag &= ~OPOST;/*No Output Processing*/

	/* Setting Time outs */
	serial_com.tty.c_cc[VMIN] = 0;
	serial_com.tty.c_cc[VTIME] = 50;

	if((tcsetattr(serial_com.fd, TCSANOW, &serial_com.tty)) != 0) { /* Set the attributes to the termios structure */
		printf("\tERROR! in Setting attributes");
		return ERR;
	} else {
		PRINTF_DEBUG("\tBaudRate = 9600\n\tStopBits = 1\n\tParity = none\n");
		return NO_ERR;
	}
}

// command should be validated outside of the serial?
/*
  yes! Because the serial api only expose specific commands function
 */
int write_serial(char *write_buffer)
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
	bytes_written = write(serial_com.fd, write_buffer, strlen(write_buffer));
	if (bytes_written == -1) {
		printf("\tError on write on serial port\n");
		close(serial_com.fd);
		return ERR;
	} else {
		printf("\tBuffer written on serial, bytes written: %d\n", bytes_written);
	}
	printf("+----------------------------------+\n\n");
	return NO_ERR;
}

int read_serial(char **out_string)
{
	printf("Reading from serial\n");
	// this specific malloc is just for testing
	// BYTE *read_buffer = malloc(DEBUG_SERIAL_SIZE);
	if (out_string == NULL) {
		printf("\tFATAL ERROR on reading serial, out_string is NULL");
		return ERR;
	}
	char ch = 0;
	dyn_array *buffer = da_alloc(sizeof(char));
	int bytes_read = 0;
	while (1) {
		if ((bytes_read = read(serial_com.fd, &ch, 1)) == -1) {
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
	DA_FREE_AND_NULL(buffer);
	if (bytes_read != 0)
		printf("read string: %s\n", *out_string);
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

int serial_status(char **status)
{
	if (write_serial((char *)cmds.status) == ERR)
		return ERR;
	if (status == NULL) {
		char *tmp = NULL;
		if (read_serial(&tmp) == ERR)
			return ERR;
		free(tmp);
	} else {
		if (read_serial(status) == ERR)
			return ERR;
	}
	return NO_ERR;
}

const char *json_make_message(msg_type type)
{
	char *status = NULL;
	unsigned char *mac = get_mac_address();
	char mac_str[MAC_ADDR_SIZE * 2 + 1] = {0};
	json_object *json_status = NULL;
	if (serial_status(&status) == ERR)
		return NULL;
	if (json_deserialize(status, &json_status) == ERR)
		return NULL;
	for (int i = 0; i < MAC_ADDR_SIZE; i++)
		sprintf(mac_str + (i * 2), "%.2X", mac[i]);
	const char *raw = json_object_get_string(json_object_object_get(json_status, "State"));
	char state[64] = {0};
	if (raw != NULL) {
		if (STR_ARE_EQUAL(raw, "STOPPED-NOT_CONNECTED")) {
			/* no STOPPED_NOT_CONNECTED in the server enum: fall back to NOT_CONNECTED */
			snprintf(state, sizeof(state), "NOT_CONNECTED");
		} else {
			/* firmware emits STOPPED-* with hyphens, the server enum uses underscores */
			size_t i;
			for (i = 0; raw[i] != '\0' && i < sizeof(state) - 1; i++)
				state[i] = (raw[i] == '-') ? '_' : raw[i];
			state[i] = '\0';
		}
	}

	/* ampere = IRMS_L1 * 100; like onion_G.py it picks identification vs realtime */
	int max_amps = json_object_get_int(json_object_object_get(json_status, "Max_amps"));
	int ampere = (int)(json_object_get_double(json_object_object_get(json_status, "IRMS_L1")) * 100);

	json_object *jobj = json_object_new_object();
	json_object_object_add(jobj, "serial_number", json_object_new_string(get_serial_number()));
	json_object_object_add(jobj, "password", json_object_new_string(mac_str));
	json_object_object_add(jobj, "status", json_object_new_string(state));
	if (type == m_boot || ampere < 500) {
		json_object_object_add(jobj, "ip_address", json_object_new_string("127.0.0.1"));
		json_object_object_add(jobj, "max_ampere", json_object_new_int(max_amps));
	} else {
		json_object_object_add(jobj, "ampere", json_object_new_int(ampere));
	}
	return json_object_to_json_string_ext(jobj, 0);
}
