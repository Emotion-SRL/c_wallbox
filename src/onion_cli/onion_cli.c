#include "../serial.h"

#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../utils.h"

#define CLI_PREFIX "[ONION CLI] "
#define OK       0
#define NO_INPUT 1
#define TOO_LONG 2

int write_serial(int fd, BYTE *write_buffer, int buffer_size);

static int getLine (char *prmpt, char *buff, size_t sz)
{
	int ch, extra;

	// Get line with buffer overrun protection.
	if (prmpt != NULL) {
		printf("%s", prmpt);
		fflush(stdout);
	}
	if (fgets (buff, sz, stdin) == NULL)
		return NO_INPUT;

	// If it was too long, there'll be no newline. In that case, we flush
	// to end of line so that excess doesn't affect the next call.
	if (buff[strlen(buff)-1] != '\n') {
		extra = 0;
		while (((ch = getchar()) != '\n') && (ch != EOF))
			extra = 1;
		return (extra == 1) ? TOO_LONG : OK;
	}

	// Otherwise remove newline and give string back to caller.
	buff[strlen(buff)-1] = '\0';
	return OK;
}

/* this simple cli will serve to send command to the micro controller.
   it is compiled separatelly, you will find it under "onion_cli" name
 */
int main(void)
{
	struct termios tty;
	int fd;
	if (serial_init(&tty, &fd) != 0) {
		printf(CLI_PREFIX"Error on serial initialization\n");
		return -1;
	}
	int rc;
	char buff[10];
	while (1) {
		rc = getLine(CLI_PREFIX"> ", buff, sizeof(buff));
		if (STR_ARE_EQUAL(buff, "exit"))
			break;
		if (rc == NO_INPUT) {
			// Extra NL since my system doesn't output that on EOF.
			printf (CLI_PREFIX"No input\n");
			return 1;
		}
		if (rc == TOO_LONG) {
			printf (CLI_PREFIX"Input too long [%s]\n", buff);
			return 1;
		}
		// else input is ok
		if (write_serial(fd, buff, ARRAY_SIZE(buff)) != 0) {
			printf(CLI_PREFIX"There was an error writing on serial, exiting...\n");
			sleep(3);
			return -1;
		}
		char *response = NULL;
		int outlen = 0;
		if (read_serial(fd, &response, &outlen) != 0) {
			printf(CLI_PREFIX"There was an error reading from serial, exiting...\n");
			sleep(3);
			return -1;
		}
		if (outlen == 0)
			printf(CLI_PREFIX"Nothing to read from serial\n");
	}
	return 0;
}
