#ifndef LEMON_UTIL_H
#define LEMON_UTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <wayland-util.h>

#if defined(__GNUC__) || defined(__clang__)
#define LEMON_HOT  __attribute__((hot))
#define LEMON_COLD __attribute__((cold))
#define LEMON_ALWAYS_INLINE __attribute__((always_inline))
#define LEMON_LIKELY(x)   __builtin_expect(!!(x), 1)
#define LEMON_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LEMON_HOT
#define LEMON_COLD
#define LEMON_ALWAYS_INLINE
#define LEMON_LIKELY(x)   (x)
#define LEMON_UNLIKELY(x) (x)
#endif

LEMON_COLD void die(const char *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);
int32_t fd_set_nonblock(int32_t fd);
LEMON_HOT int32_t regex_match(const char *pattern_mb, const char *str_mb);
void wl_list_append(struct wl_list *list, struct wl_list *object);
uint32_t get_now_in_ms(void);

void frame_clock_begin(void);
void frame_clock_end(void);
char *join_strings(char *arr[], const char *sep);
char *join_strings_with_suffix(char *arr[], const char *suffix,
							   const char *sep);
char *string_printf(const char *fmt, ...);

extern struct timespec frame_clock_cached_ts;
extern uint32_t frame_clock_cached_ms;
extern bool frame_clock_cached_valid;

/* Convert a struct timespec into milliseconds as a uint32_t. */
static inline LEMON_ALWAYS_INLINE uint32_t timespec_to_ms(const struct timespec *ts) {
	return (uint32_t)ts->tv_sec * 1000u + (uint32_t)ts->tv_nsec / 1000000u;
}

/* Cached per-frame monotonic time in ms (live read fallback). */
static inline LEMON_ALWAYS_INLINE uint32_t frame_now_ms(void) {
	if (LEMON_LIKELY(frame_clock_cached_valid))
		return frame_clock_cached_ms;
	return get_now_in_ms();
}

/* Fill ts with cached frame timespec if available, else read CLOCK_MONOTONIC live. */
static inline LEMON_ALWAYS_INLINE void frame_clock_now_timespec(struct timespec *ts) {
	if (LEMON_LIKELY(frame_clock_cached_valid)) {
		*ts = frame_clock_cached_ts;
		return;
	}
	clock_gettime(CLOCK_MONOTONIC, ts);
}

#endif
