#include "utils.h"

#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <time.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <json.h>

#include "serial.h"

typedef struct {
	struct tm *mt;
	time_t mtt;
	char ftime[128];
} local_time;

typedef struct {
	unsigned char mac[6];
	char *serial_number;
} wb_id;

static local_time t = {0};
static wb_id id = {0};

FILE *g_log_file = NULL;

int log_file_init(const char *path)
{
	g_log_file = fopen(path, "w");   /* "w" truncates -> fresh log on every start */
	if (g_log_file == NULL) {
		LOG_ERR("ERROR on log_file_init, could not open %s\n", path);
		return ERR;
	}
	return NO_ERR;
}

char *get_serial_number()
{
	return id.serial_number;
}

unsigned char *get_mac_address()
{
	return id.mac;
}

int wallbox_identity_init(void)
{
	if (retrive_mac_addr(id.mac) == ERR)
		return ERR;
	/* let's just read the file */
	if (load_file_nul_str("serial_number.txt", &id.serial_number) == ERR)
		return ERR;
	char *tmp = NULL;
	if ((tmp = strchr(id.serial_number, '\n')) != NULL) {
		/* then there is a new line to be removed */
		*tmp = '\0';
	}
	LOG_DBG("serial number: %s\n", id.serial_number);
	return NO_ERR;
}

int local_time_init(void)
{
	setenv("TZ", "Europe/Rome", 1);
	tzset();
	t.mtt = time(NULL);
	t.mt = localtime(&t.mtt);
	strftime(t.ftime, sizeof(t.ftime), "%d-%m-%Y, %H:%M:%S", t.mt);
	return NO_ERR;
}

char *local_time_get_time(void)
{
	strftime(t.ftime, sizeof(t.ftime), "%d-%m-%Y, %H:%M:%S", t.mt);
	return t.ftime;
}

int load_file_nul_str(const char *path, char **out)
{
	FILE *fs;
	if (!(fs = fopen(path, "r"))) {
		LOG_ERR("ERROR on load file, fopen failed\n");
		return ERR;
	}
	if (fseek(fs, 0, SEEK_END) != NO_ERR) {
		LOG_ERR("ERROR on load file, fseek failed\n");
		return ERR;
	}
	int len = 0;
	if ((len = ftell(fs)) == ERR) {
		LOG_ERR("ERROR on load file, ftell failed\n");
		return ERR;
	}
	LOG_DBG("len: %d\n", len);
	rewind(fs); /*from the manual: "The rewind() function returns no value."
		      so there is no fucking way on doing error checking Dio Insetto
		      i mean, i could check if fs position is NOT at start, but non ne ho voglia*/
	(*out) = malloc(len + 1); // +1 for the '\0'[NULL] character
	fread((*out), 1, len, fs);
	(*out)[len] = '\0';
	//LOG_DBG("file read: %s\n", (*out));
	return NO_ERR;
}

/*
  dynamic arrays functions
*/
dyn_array *da_alloc(size_t el_size)
{
	if (el_size == 0) {
		LOG_ERR("FATAL ERROR on dynamic array allocation, element size is 0\n");
		return NULL;
	}
	dyn_array *da = calloc(1, sizeof(dyn_array));
	da->data = calloc(DYN_ARRAY_SIZE_MULTIPLIER, el_size);
	da->count = 0;
	da->capacity = DYN_ARRAY_SIZE_MULTIPLIER;
	da->el_size = el_size;
	return da;
}

int da_append(dyn_array *da, BYTE *new_el)
{
	if (da == NULL) {
		LOG_ERR("FATAL ERROR on dynamic array append, dynamic array pointer is NULL\n");
		return ERR;
	}
	if (new_el == NULL) {
		LOG_ERR("FATAL ERROR on dynamic array append, new element pointer is NULL\n");
		return ERR;
	}
	if (da->count == da->capacity) {
		// time to reallocate
		da->capacity *= DYN_ARRAY_SIZE_MULTIPLIER;
		da->data = realloc((void *)da->data, da->el_size * da->capacity);
	}
	/* ok, here is the tricky part. we need to find the first empty byte
	 kinda easy, just move the pointer after the last element
	 this offset is given by multipling the size of the element type and the number of elements
	 for example:
	 let's say count is 10 and the element size (el_size) is 4 (sizeof int), then the next free byte is
	 the 11th element given by count * el_size => 10 * 4 = 40 (a.k.a. element [10])
	*/
	BYTE *b = (BYTE *)(da->data + (da->el_size * da->count));
	// then, raw copy data from new element to the byte pointer
	// the size given to memcpy is, ofcourse, the size of the element type
	memcpy(b, new_el, da->el_size);
	da->count++;
	return NO_ERR;
}

int da_free(dyn_array *da)
{
	if (da == NULL) {
		LOG_ERR("FATAL ERROR on dynamic array free, dynamic array pointer is NULL\n");
		return ERR;
	}
	if (da->data == NULL) {
		LOG_ERR("FATAL ERROR on dynamic array free, da data is NULL\n");
		return ERR;
	}
	free(da->data);
	da->data = NULL;
	free(da);
	return NO_ERR;
}

#define MAC_IFACE "br-wlan"

int retrive_mac_addr(unsigned char mac_addr[MAC_ADDR_SIZE])
{
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock == -1)
		return ERR;
	struct ifreq ifr = {0};
	strncpy(ifr.ifr_name, MAC_IFACE, IFNAMSIZ - 1);
	if (ioctl(sock, SIOCGIFHWADDR, &ifr) != 0) {
		close(sock);
		return ERR;
	}
	close(sock);
	memcpy(mac_addr, ifr.ifr_hwaddr.sa_data, MAC_ADDR_SIZE);
	LOG_DBG("\tMAC address recovered from " MAC_IFACE ": ");
	for (int i = 0; i < MAC_ADDR_SIZE; i++) {
		LOG_DBG("%.2X", mac_addr[i]);
		if (i != MAC_ADDR_SIZE - 1)
			LOG_DBG(":");
	}
	LOG_DBG("\n");
	return NO_ERR;
}

/* return ERR if fails, NO_ERR otherwise */
int ascii_to_int(const char *s, int *out)
{
	const int MAX_SIZE = 5; // i will never need more then 4 digits
	if (s == NULL)
		return ERR;
	if (strlen(s) > MAX_SIZE)
		return ERR;
	for (int i = 0; i < strlen(s); i++)
		if (!isdigit(s[i]))
			return ERR;
	int cursor = 0;
	int mul = 1;
	for (int i = 0; i < strlen(s) - 1; i++)
		mul *= 10;
	int val = 0;
	while (1) {
		if (s[cursor] == '\0')
			break;
		if (cursor > MAX_SIZE)
			return ERR;
		val = val + ((s[cursor] - '0') * mul);
		mul /= 10;
		cursor++;
		/*e.g.
		  let's parse "123" -> mul = 10 * 10 = 100
		  first char is 1 so
		      val = 0 + (('1' - '0') * 100)
		      val = 0 + (1 * 100) = 100 -> mul = 100 / 10 = 10
		  second char is 2 so
		      val = 100 + (('2' - '0') * 10)
		      val = 100 + (2 * 10)
		      val = 100 + 20 = 120 -> mul = 10 / 10 = 1
		  last char is 3 so
		      val = 120 + (('3' - '0') * 1)
		      val = 120 + (3 * 1)
		      val = 120 + 3 = 123
		      [done!]
		 */
	}
	*out = val;
	return NO_ERR;
}

int json_deserialize(const char *json_str, struct json_object **jobj)
{
	*jobj = json_tokener_parse(json_str);
	if (*jobj == NULL)
		return ERR;
	return NO_ERR;
}
