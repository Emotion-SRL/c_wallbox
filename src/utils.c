#include "utils.h"

#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "serial.h"

/*
  dynamic arrays functions
*/

dyn_array *da_alloc(size_t el_size)
{
	if (el_size == 0) {
		printf("FATAL ERROR on dynamic array allocation, element size is 0\n");
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
		printf("FATAL ERROR on dynamic array append, dynamic array pointer is NULL\n");
		return ERR;
	}
	if (new_el == NULL) {
		printf("FATAL ERROR on dynamic array append, new element pointer is NULL\n");
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
	memcpy (b, new_el, da->el_size);
	da->count++;
	return NO_ERR;
}

int da_free(dyn_array *da)
{
	if (da == NULL) {
		printf("FATAL ERROR on dynamic array free, dynamic array pointer is NULL\n");
		return ERR;
	}
	if (da->data == NULL) {
		printf("FATAL ERROR on dynamic array free, da data is NULL\n");
		return ERR;
	}
	free(da->data);
	da->data = NULL;
	free(da);
	return NO_ERR;
}

int get_mac_addr(unsigned char mac_addr[MAC_ADDR_SIZE])
{
	struct ifreq ifr;
	struct ifconf ifc;
	char buf[1024];
	int success = ERR;

	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock == -1) { /* handle error*/ };

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) { /* handle error */ }

	struct ifreq* it = ifc.ifc_req;
	const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

	for (; it != end; ++it) {
		strcpy(ifr.ifr_name, it->ifr_name);
		if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
			if (! (ifr.ifr_flags & IFF_LOOPBACK)) { // don't count loopback
				if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
					success = NO_ERR;
					break;
				}
			}
		}
		else { /* handle error */ }

	}
	if (success == NO_ERR) {
		printf("\tMAC address recovered: ");
		memcpy(mac_addr, ifr.ifr_hwaddr.sa_data, 6);
		for (int i = 0; i < MAC_ADDR_SIZE; i++) {
			printf("%.2X", mac_addr[i]);
			if (i != MAC_ADDR_SIZE - 1)
				printf(":");
		}
		printf("\n");
	}
	return success;
}

// return ERR if fails, NO_ERR otherwise
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
	printf("jobj from str:\n---\n%s\n---\n", json_object_to_json_string_ext(*jobj, JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY));
	return NO_ERR;
}
