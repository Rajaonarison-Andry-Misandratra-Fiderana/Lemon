#ifndef LEMON_SURFACE_CACHE_H
#define LEMON_SURFACE_CACHE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wlr/util/box.h>

/* Persistent (app_id -> last known geometry) cache. Used to send a sensible
   initial xdg_toplevel size before the client commits its first buffer,
   eliminating one configure round-trip on app launch. */

#define SURFACE_CACHE_CAP 256

typedef struct {
	char app_id[128];
	int32_t width;
	int32_t height;
	uint64_t last_used;
} SurfaceCacheEntry;

static SurfaceCacheEntry surface_cache[SURFACE_CACHE_CAP];
static uint64_t surface_cache_clock = 0;
static char surface_cache_path[512];
static bool surface_cache_dirty = false;

/* Resolve $XDG_CACHE_HOME/lemon/surfaces.db (fallback to $HOME/.cache/lemon). */
static void surface_cache_resolve_path(void) {
	const char *cache = getenv("XDG_CACHE_HOME");
	if (cache && cache[0]) {
		snprintf(surface_cache_path, sizeof(surface_cache_path),
		         "%s/lemon/surfaces.db", cache);
	} else {
		const char *home = getenv("HOME");
		if (!home || !home[0]) {
			surface_cache_path[0] = '\0';
			return;
		}
		snprintf(surface_cache_path, sizeof(surface_cache_path),
		         "%s/.cache/lemon/surfaces.db", home);
	}
	char dir[512];
	snprintf(dir, sizeof(dir), "%s", surface_cache_path);
	char *slash = strrchr(dir, '/');
	if (slash) {
		*slash = '\0';
		mkdir(dir, 0700);
	}
}

/* Read the on-disk DB once at startup. Silently tolerates a missing file. */
static void surface_cache_load(void) {
	surface_cache_resolve_path();
	if (!surface_cache_path[0])
		return;
	FILE *fp = fopen(surface_cache_path, "re");
	if (!fp)
		return;
	char buf[256];
	while (fgets(buf, sizeof(buf), fp)) {
		char id[128];
		int w, h;
		if (sscanf(buf, "%127[^\t]\t%d\t%d", id, &w, &h) != 3)
			continue;
		if (w <= 0 || h <= 0)
			continue;
		for (int i = 0; i < SURFACE_CACHE_CAP; i++) {
			if (!surface_cache[i].app_id[0]) {
				strncpy(surface_cache[i].app_id, id,
				        sizeof(surface_cache[i].app_id) - 1);
				surface_cache[i].width = w;
				surface_cache[i].height = h;
				surface_cache[i].last_used = ++surface_cache_clock;
				break;
			}
		}
	}
	fclose(fp);
}

/* Flush in-memory cache to disk. Atomic via rename. */
static void surface_cache_save(void) {
	if (!surface_cache_dirty || !surface_cache_path[0])
		return;
	char tmp[600];
	snprintf(tmp, sizeof(tmp), "%s.tmp", surface_cache_path);
	FILE *fp = fopen(tmp, "we");
	if (!fp)
		return;
	for (int i = 0; i < SURFACE_CACHE_CAP; i++) {
		if (!surface_cache[i].app_id[0])
			continue;
		fprintf(fp, "%s\t%d\t%d\n", surface_cache[i].app_id,
		        surface_cache[i].width, surface_cache[i].height);
	}
	fclose(fp);
	rename(tmp, surface_cache_path);
	surface_cache_dirty = false;
}

/* Look up the last known size for app_id; returns true if found. */
static bool surface_cache_lookup(const char *app_id, int32_t *width,
                                 int32_t *height) {
	if (!app_id || !app_id[0])
		return false;
	for (int i = 0; i < SURFACE_CACHE_CAP; i++) {
		if (surface_cache[i].app_id[0] &&
		    strcmp(surface_cache[i].app_id, app_id) == 0) {
			surface_cache[i].last_used = ++surface_cache_clock;
			*width = surface_cache[i].width;
			*height = surface_cache[i].height;
			return true;
		}
	}
	return false;
}

/* Update (or insert with LRU eviction) the cached size for app_id. */
static void surface_cache_store(const char *app_id, int32_t width,
                                int32_t height) {
	if (!app_id || !app_id[0] || width <= 0 || height <= 0)
		return;
	int slot = -1;
	uint64_t oldest = UINT64_MAX;
	for (int i = 0; i < SURFACE_CACHE_CAP; i++) {
		if (surface_cache[i].app_id[0] &&
		    strcmp(surface_cache[i].app_id, app_id) == 0) {
			if (surface_cache[i].width == width &&
			    surface_cache[i].height == height) {
				surface_cache[i].last_used = ++surface_cache_clock;
				return;
			}
			surface_cache[i].width = width;
			surface_cache[i].height = height;
			surface_cache[i].last_used = ++surface_cache_clock;
			surface_cache_dirty = true;
			return;
		}
		if (!surface_cache[i].app_id[0]) {
			slot = i;
			oldest = 0;
		} else if (surface_cache[i].last_used < oldest) {
			oldest = surface_cache[i].last_used;
			slot = i;
		}
	}
	if (slot < 0)
		return;
	strncpy(surface_cache[slot].app_id, app_id,
	        sizeof(surface_cache[slot].app_id) - 1);
	surface_cache[slot].app_id[sizeof(surface_cache[slot].app_id) - 1] = '\0';
	surface_cache[slot].width = width;
	surface_cache[slot].height = height;
	surface_cache[slot].last_used = ++surface_cache_clock;
	surface_cache_dirty = true;
}

#endif
