#pragma once

#include <stddef.h>
#include <stdio.h>
#include <json.h>
#define MAC_ADDR_SIZE 6
#define BYTE unsigned char
#define DYN_ARRAY_SIZE_MULTIPLIER 2
#define BUFF_MAX_SIZE 64 /* this is just a standrt guideline for how
			      much allocate on the stuck for a buffer
			      you don't have to follow this
			   */

#define DA_FREE_AND_NULL(da)					\
	da_free(da);						\
	da = NULL

#define STR_ARE_EQUAL(s1, s2) (((strcmp(s1, s2) == 0) ? 1 : 0))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/*
  logging macros, both drop-in replacements for printf (same format + args):
    LOG_DBG - verbose traces, compiled out unless built with -DDEBUG (dev targets)
    LOG_ERR - errors, always printed, on stderr
  DEBUG is set by the Makefile (-DDEBUG) on dev builds and left undefined on release.

  Both also mirror to g_log_file when it's open (see log_file_init): this lets us
  read what the client did even when it's started detached at boot. Note: the args
  are evaluated twice (console + file), so don't pass side-effecting expressions.
*/
extern FILE *g_log_file;   /* runtime log file, opened in main(); NULL until then */

#ifdef DEBUG
#define LOG_DBG(...) do {                                                       \
		printf(__VA_ARGS__); fflush(stdout);                           \
		if (g_log_file) { fprintf(g_log_file, __VA_ARGS__); fflush(g_log_file); } \
	} while (0)
#else
#define LOG_DBG(...) ((void)0)
#endif

#define LOG_ERR(...) do {                                                       \
		fprintf(stderr, __VA_ARGS__);                                  \
		if (g_log_file) { fprintf(g_log_file, __VA_ARGS__); fflush(g_log_file); } \
	} while (0)

/* open the runtime log file in "w" (truncates: each boot starts fresh -> our
   poor-man's rotation). best-effort: on failure logging just stays console-only */
int log_file_init(const char *path);

#define NO_ERR 0
#define ERR 1

typedef struct {
	BYTE *data;
	size_t count;
	size_t capacity;
	size_t el_size;
} dyn_array;

int wallbox_identity_init(void);
char *get_serial_number();
unsigned char *get_mac_address();

int load_file_nul_str(const char *path, char **out);

int retrive_mac_addr(unsigned char mac_addr[MAC_ADDR_SIZE]);
int read_serial_number(char **out);

// return NULL on fail
dyn_array *da_alloc(size_t el_size);

// return ERR on fail, NO_ERR on success
int da_append(dyn_array *da, BYTE *new_el);

// return ERR on fail, NO_ERR on success
int da_free(dyn_array *da);

// return ERR if fails, NO_ERR otherwise
int ascii_to_int(const char *s, int *out);
int micro_command_is_valid(const char *command, int command_size);

/*json api docs https://json-c.github.io/json-c/json-c-0.10/doc/html/json__object_8h.html */
int json_deserialize(const char *json_str, struct json_object **jobj);

/* time related functions
   time is an opaque struct for clear reasons
 */
int local_time_init(void);
char *local_time_get_time(void);
