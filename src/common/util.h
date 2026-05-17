/* See LICENSE.dwm file for copyright and license details. */
#include <wayland-util.h>

void die(const char *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);
int32_t fd_set_nonblock(int32_t fd);
int32_t regex_match(const char *pattern_mb, const char *str_mb);
void wl_list_append(struct wl_list *list, struct wl_list *object);
uint32_t get_now_in_ms(void);
uint32_t timespec_to_ms(struct timespec *ts);

/* Frame-cached monotonic clock. Call frame_clock_begin() at the top of a
 * render frame; subsequent frame_now_ms() calls return the cached timestamp
 * (no syscall). frame_clock_end() invalidates the cache so out-of-frame
 * callers fall back to a fresh clock_gettime(). */
void frame_clock_begin(void);
void frame_clock_end(void);
uint32_t frame_now_ms(void);
void frame_clock_now_timespec(struct timespec *ts);
char *join_strings(char *arr[], const char *sep);
char *join_strings_with_suffix(char *arr[], const char *suffix,
							   const char *sep);
char *string_printf(const char *fmt, ...);