#include "utils.h"

#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
		return -1;
	}
	if (new_el == NULL) {
		printf("FATAL ERROR on dynamic array append, new element pointer is NULL\n");
		return -1;
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
	return 0;
}

int da_free(dyn_array *da)
{
	if (da == NULL) {
		printf("FATAL ERROR on dynamic array free, dynamic array pointer is NULL\n");
		return -1;
	}
	if (da->data == NULL) {
		printf("FATAL ERROR on dynamic array free, da data is NULL\n");
		return -1;
	}
	free(da->data);
	da->data = NULL;
	free(da);
	return 0;
}

int get_mac_addr(unsigned char mac_addr[MAC_ADDR_SIZE])
{
	struct ifreq ifr;
	struct ifconf ifc;
	char buf[1024];
	int success = 0;

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
					success = 1;
					break;
				}
			}
		}
		else { /* handle error */ }

	}
	if (success) {
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
