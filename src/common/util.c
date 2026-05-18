
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

/* Print a printf-style error message (appending perror if fmt ends in ':') and exit(1). */
void die(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}

	exit(1);
}

/* Allocate zero-initialized memory and die() on failure, sparing callers null-checks. */
void *ecalloc(size_t nmemb, size_t size) {
	void *p;

	if (!(p = calloc(nmemb, size)))
		die("calloc:");
	return p;
}

/* Mark fd as non-blocking via F_SETFL; logs and returns -1 on fcntl error. */
int32_t fd_set_nonblock(int32_t fd) {
	int32_t flags = fcntl(fd, F_GETFL);
	if (flags < 0) {
		perror("fcntl(F_GETFL):");
		return -1;
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		perror("fcntl(F_SETFL):");
		return -1;
	}

	return 0;
}

#define REGEX_CACHE_SIZE 32

struct regex_cache_entry {
	char *pattern;
	pcre2_code *re;
	pcre2_match_data *match_data;
	uint64_t last_used;
};

static struct regex_cache_entry regex_cache[REGEX_CACHE_SIZE];
static uint64_t regex_cache_clock = 0;

/* Look up a compiled regex by pattern in the LRU cache, returning its match_data via out_md. */
static pcre2_code *regex_cache_lookup(const char *pattern,
                                      pcre2_match_data **out_md) {
	for (int32_t i = 0; i < REGEX_CACHE_SIZE; i++) {
		if (regex_cache[i].pattern &&
		    strcmp(regex_cache[i].pattern, pattern) == 0) {
			regex_cache[i].last_used = ++regex_cache_clock;
			*out_md = regex_cache[i].match_data;
			return regex_cache[i].re;
		}
	}
	return NULL;
}

/* Insert a compiled regex into the cache, evicting the least-recently-used slot if full. */
LEMON_COLD static void regex_cache_insert(const char *pattern, pcre2_code *re,
                               pcre2_match_data *md) {
	int32_t lru_idx = 0;
	uint64_t lru_age = UINT64_MAX;
	for (int32_t i = 0; i < REGEX_CACHE_SIZE; i++) {
		if (!regex_cache[i].pattern) {
			lru_idx = i;
			break;
		}
		if (regex_cache[i].last_used < lru_age) {
			lru_age = regex_cache[i].last_used;
			lru_idx = i;
		}
	}
	if (regex_cache[lru_idx].pattern) {
		free(regex_cache[lru_idx].pattern);
		pcre2_match_data_free(regex_cache[lru_idx].match_data);
		pcre2_code_free(regex_cache[lru_idx].re);
	}
	regex_cache[lru_idx].pattern = strdup(pattern);
	regex_cache[lru_idx].re = re;
	regex_cache[lru_idx].match_data = md;
	regex_cache[lru_idx].last_used = ++regex_cache_clock;
}

/* Return non-zero if str matches the PCRE2 pattern, compiling+caching it on first use. */
int32_t regex_match(const char *pattern, const char *str) {
	int32_t errnum;
	PCRE2_SIZE erroffset;

	if (!pattern || !str) {
		return 0;
	}

	pcre2_match_data *match_data = NULL;
	pcre2_code *re = regex_cache_lookup(pattern, &match_data);

	if (LEMON_UNLIKELY(!re)) {
		re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
		                   PCRE2_UTF, &errnum, &erroffset, NULL);
		if (!re) {
			PCRE2_UCHAR errbuf[256];
			pcre2_get_error_message(errnum, errbuf, sizeof(errbuf));
			fprintf(stderr, "PCRE2 error: %s at offset %zu\n", errbuf, erroffset);
			return 0;
		}
		pcre2_jit_compile(re, PCRE2_JIT_COMPLETE);
		match_data = pcre2_match_data_create_from_pattern(re, NULL);
		regex_cache_insert(pattern, re, match_data);
	}

	int32_t ret =
		pcre2_match(re, (PCRE2_SPTR)str, strlen(str), 0,
		            PCRE2_NO_UTF_CHECK, match_data, NULL);
	return ret >= 0;
}

/* Append node to the tail of a wl_list. */
void wl_list_append(struct wl_list *list, struct wl_list *object) {
	wl_list_insert(list->prev, object);
}

/* Return the current CLOCK_MONOTONIC time as milliseconds (truncated to uint32_t). */
uint32_t get_now_in_ms(void) {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	return timespec_to_ms(&now);
}

struct timespec frame_clock_cached_ts;
uint32_t frame_clock_cached_ms;
bool frame_clock_cached_valid = false;

/* Snapshot the monotonic clock once per frame so subsequent queries are O(1) and consistent. */
void frame_clock_begin(void) {
	clock_gettime(CLOCK_MONOTONIC, &frame_clock_cached_ts);
	frame_clock_cached_ms = timespec_to_ms(&frame_clock_cached_ts);
	frame_clock_cached_valid = true;
}

/* Invalidate the per-frame cached clock so the next query re-reads the OS clock. */
void frame_clock_end(void) {
	frame_clock_cached_valid = false;
}

/* Concatenate a NULL-terminated array of strings into a newly malloc'd string joined by sep. */
char *join_strings(char *arr[], const char *sep) {
	if (!arr || !arr[0]) {
		char *empty = malloc(1);
		if (empty)
			empty[0] = '\0';
		return empty;
	}

	size_t total_len = 0;
	int count = 0;
	for (int i = 0; arr[i] != NULL; i++) {
		total_len += strlen(arr[i]);
		count++;
	}
	if (count > 0) {
		total_len += strlen(sep) * (count - 1);
	}

	char *result = malloc(total_len + 1);
	if (!result)
		return NULL;

	result[0] = '\0';
	for (int i = 0; arr[i] != NULL; i++) {
		if (i > 0)
			strcat(result, sep);
		strcat(result, arr[i]);
	}
	return result;
}

/* Like join_strings but appends suffix after every element, useful for building path lists. */
char *join_strings_with_suffix(char *arr[], const char *suffix,
							   const char *sep) {
	if (!arr || !arr[0]) {
		char *empty = malloc(1);
		if (empty)
			empty[0] = '\0';
		return empty;
	}

	size_t total_len = 0;
	int count = 0;
	for (int i = 0; arr[i] != NULL; i++) {
		total_len += strlen(arr[i]) + strlen(suffix);
		count++;
	}
	if (count > 0) {
		total_len += strlen(sep) * (count - 1);
	}

	char *result = malloc(total_len + 1);
	if (!result)
		return NULL;

	result[0] = '\0';
	for (int i = 0; arr[i] != NULL; i++) {
		if (i > 0)
			strcat(result, sep);
		strcat(result, arr[i]);
		strcat(result, suffix);
	}
	return result;
}

/* printf-style formatter that returns a newly malloc'd string sized exactly to the result. */
char *string_printf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);
	if (len < 0)
		return NULL;

	char *str = malloc(len + 1);
	if (!str)
		return NULL;

	va_start(args, fmt);
	vsnprintf(str, len + 1, fmt, args);
	va_end(args);
	return str;
}
