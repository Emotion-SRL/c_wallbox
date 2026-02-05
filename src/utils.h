#pragma once

#include <stddef.h>
#include <json.h>
#define MAC_ADDR_SIZE 6
#define BYTE unsigned char
#define DYN_ARRAY_SIZE_MULTIPLIER 2
#define BUFF_MAX_SIZE 64 /* this is just a standrt guideline for how
			      much allocate on the stuck for a buffer
			      you don't have to follow this
			   */

#define DEBUG

#define DA_FREE_AND_NULL(da)					\
	da_free(da);						\
	da = NULL

#define STR_ARE_EQUAL(s1, s2) (((strcmp(s1, s2) == 0) ? 1 : 0))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#ifdef DEBUG
#define PRINTF_DEBUG(format, ...) do { printf(format __VA_ARGS__); fflush(stdout); } while (0)
#else
#define PRINTF_DEBUG()
#endif

#define NO_ERR 0
#define ERR 1

typedef struct {
	BYTE *data;
	size_t count;
	size_t capacity;
	size_t el_size;
} dyn_array;

int load_file_nul_str(const char *path, char **out);

int get_mac_addr(unsigned char mac_addr[MAC_ADDR_SIZE]);

// return NULL on fail
dyn_array *da_alloc(size_t el_size);

// return ERR on fail, NO_ERR on success
int da_append(dyn_array *da, BYTE *new_el);

// return ERR on fail, NO_ERR on success
int da_free(dyn_array *da);

// return ERR if fails, NO_ERR otherwise
int ascii_to_int(const char *s, int *out);
int micro_command_is_valid(const char *command, int command_size);
int json_deserialize(const char *json_str, struct json_object **jobj);
