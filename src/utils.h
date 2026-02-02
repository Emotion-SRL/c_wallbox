#pragma once

#include <stddef.h>
#define MAC_ADDR_SIZE 6
#define BYTE unsigned char
#define DYN_ARRAY_SIZE_MULTIPLIER 2

#define DEBUG

#define DA_FREE_AND_NULL(da)					\
	da_free(da);						\
	da = NULL

#define STR_ARE_EQUAL(s1, s2) (((strcmp(s1, s2) == 0) ? 1 : 0))

#ifdef DEBUG
#define PRINTF_DEBUG(...) do { printf(__VA_ARGS__); fflush(stdout); } while (0)
#elif
#define PRINTF_DEBUG()
#endif

int get_mac_addr(unsigned char mac_addr[MAC_ADDR_SIZE]);

typedef struct {
	BYTE *data;
	size_t count;
	size_t capacity;
	size_t el_size;
} dyn_array;

// return NULL on fail
dyn_array *da_alloc(size_t el_size);

// return -1 on fail, 0 on success
int da_append(dyn_array *da, BYTE *new_el);

// return -1 on fail, 0 on success
int da_free(dyn_array *da);
