#pragma once

/* In-memory clipboard history with a built-in popup picker.
 *
 * Lives entirely in RAM: the ring resets every compositor start, so a
 * reboot flushes everything. Captures text/plain selections via the
 * normal wlr_seat set_selection signal, stores up to N entries (each
 * capped in size), and exposes a small scenefx popup at the bottom
 * centre of the focused monitor to pick one back. Picking a row
 * synthesises a new selection via our own wlr_data_source, so the next
 * paste in any client reads the historical bytes. */

#include <cairo.h>
#include <drm_fourcc.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pango/pangocairo.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#define CLIP_MIME_UTF8 "text/plain;charset=utf-8"
#define CLIP_MIME_TEXT "text/plain"
#define CLIP_MIME_UTF8_ALT "UTF8_STRING"
#define CLIP_MIME_STRING "STRING"

#define CLIP_PREVIEW_CHARS 80
#define CLIP_PREVIEW_BYTES 320
#define CLIP_ROW_H 36
#define CLIP_ROW_PAD_X 14
#define CLIP_POPUP_W 520
#define CLIP_POPUP_PAD 10
#define CLIP_POPUP_RADIUS 14
#define CLIP_POPUP_MARGIN_BOTTOM 60

typedef struct {
	uint8_t *data;
	size_t len;
	uint32_t ts_ms;
	char preview[CLIP_PREVIEW_BYTES];
} ClipEntry;

typedef struct ClipboardCapture {
	int fd;
	struct wl_event_source *src;
	uint8_t *buf;
	size_t cap;
	size_t len;
	size_t cap_limit;
	uint32_t ts_ms;
} ClipboardCapture;

typedef struct {
	struct wlr_scene_rect *bg;
	struct wlr_scene_buffer *text;
	struct wlr_buffer *buf_ref;
} ClipRow;

typedef struct ClipTextBuffer {
	struct wlr_buffer base;
	uint8_t *data;
	size_t stride;
} ClipTextBuffer;

typedef struct {
	bool enabled;

	ClipEntry *entries;
	size_t count;
	size_t capacity;
	size_t max_entries;
	size_t max_bytes_per_entry;
	bool ignore_next;

	bool popup_open;
	Monitor *popup_mon;
	struct wlr_scene_tree *popup_tree;
	struct wlr_scene_rect *popup_bg;
	ClipRow *rows;
	size_t rows_count;
	int selected;
	int popup_w;
	int popup_h;
} Clipboard;

static Clipboard clipboard;

/* ---- wlr_buffer backed by a raw ARGB8888 pixel block ---- */

/* Hand the renderer a pointer to the cached pixel data. */
static bool clip_text_buffer_begin(struct wlr_buffer *base, uint32_t flags,
								   void **data, uint32_t *format,
								   size_t *stride) {
	ClipTextBuffer *b = wl_container_of(base, b, base);
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false;
	}
	*data = b->data;
	*format = DRM_FORMAT_ARGB8888;
	*stride = b->stride;
	return true;
}

/* No-op end since the data pointer stays valid for the buffer lifetime. */
static void clip_text_buffer_end(struct wlr_buffer *base) {
	(void)base;
}

/* Free the cached pixel block when the last reference drops. */
static void clip_text_buffer_destroy(struct wlr_buffer *base) {
	ClipTextBuffer *b = wl_container_of(base, b, base);
	free(b->data);
	free(b);
}

static const struct wlr_buffer_impl clip_text_buffer_impl = {
	.destroy = clip_text_buffer_destroy,
	.begin_data_ptr_access = clip_text_buffer_begin,
	.end_data_ptr_access = clip_text_buffer_end,
};

/* Render preview text into a fresh ARGB8888 wlr_buffer (single row). */
static struct wlr_buffer *clip_render_row(int w, int h, const char *text,
										  bool selected) {
	cairo_surface_t *surf =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surf);
		return NULL;
	}
	cairo_t *cr = cairo_create(surf);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	if (selected) {
		cairo_set_source_rgba(cr, 0.79, 0.72, 0.56, 0.28);
		cairo_rectangle(cr, 0, 0, w, h);
		cairo_fill(cr);
	}

	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoFontDescription *desc =
		pango_font_description_from_string("Sans 11");
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);
	pango_layout_set_text(layout, text, -1);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	pango_layout_set_width(layout, (w - 2 * CLIP_ROW_PAD_X) * PANGO_SCALE);
	pango_layout_set_single_paragraph_mode(layout, TRUE);

	int tw, th;
	pango_layout_get_pixel_size(layout, &tw, &th);

	cairo_move_to(cr, CLIP_ROW_PAD_X, (h - th) / 2);
	if (selected) {
		cairo_set_source_rgba(cr, 1.0, 0.95, 0.85, 1.0);
	} else {
		cairo_set_source_rgba(cr, 0.92, 0.88, 0.78, 1.0);
	}
	pango_cairo_show_layout(cr, layout);

	g_object_unref(layout);
	cairo_destroy(cr);
	cairo_surface_flush(surf);

	int stride = cairo_image_surface_get_stride(surf);
	size_t total = (size_t)stride * (size_t)h;
	uint8_t *pixels = malloc(total);
	if (!pixels) {
		cairo_surface_destroy(surf);
		return NULL;
	}
	memcpy(pixels, cairo_image_surface_get_data(surf), total);
	cairo_surface_destroy(surf);

	ClipTextBuffer *b = calloc(1, sizeof(*b));
	if (!b) {
		free(pixels);
		return NULL;
	}
	wlr_buffer_init(&b->base, &clip_text_buffer_impl, w, h);
	b->data = pixels;
	b->stride = (size_t)stride;
	return &b->base;
}

/* ---- entry storage helpers ---- */

/* Build a single-line, length-capped UTF-8 preview from a raw byte buffer. */
static void clip_make_preview(char *out, const uint8_t *data, size_t len) {
	size_t n = len < CLIP_PREVIEW_BYTES - 1 ? len : CLIP_PREVIEW_BYTES - 1;
	for (size_t i = 0; i < n; i++) {
		unsigned char c = data[i];
		if (c == '\n' || c == '\r' || c == '\t') {
			out[i] = ' ';
		} else if (c < 0x20 || c == 0x7f) {
			out[i] = ' ';
		} else {
			out[i] = (char)c;
		}
	}
	out[n] = '\0';
}

/* Release the bytes owned by one ClipEntry. */
static void clip_entry_free(ClipEntry *e) {
	free(e->data);
	e->data = NULL;
	e->len = 0;
}

/* Drop every stored entry. */
static void clip_history_clear(void) {
	for (size_t i = 0; i < clipboard.count; i++) {
		clip_entry_free(&clipboard.entries[i]);
	}
	clipboard.count = 0;
}

/* Insert a freshly captured byte buffer at the front, dedup vs head,
 * drop the tail beyond max_entries. Takes ownership of data. */
static void clip_history_push(uint8_t *data, size_t len) {
	if (len == 0) {
		free(data);
		return;
	}
	if (clipboard.count > 0) {
		ClipEntry *head = &clipboard.entries[0];
		if (head->len == len && memcmp(head->data, data, len) == 0) {
			free(data);
			return;
		}
	}
	if (clipboard.count == clipboard.max_entries) {
		clip_entry_free(&clipboard.entries[clipboard.count - 1]);
		clipboard.count--;
	}
	memmove(&clipboard.entries[1], &clipboard.entries[0],
			clipboard.count * sizeof(ClipEntry));
	clipboard.entries[0].data = data;
	clipboard.entries[0].len = len;
	clipboard.entries[0].ts_ms = get_now_in_ms();
	clip_make_preview(clipboard.entries[0].preview, data, len);
	clipboard.count++;
}

/* ---- async capture from a selection source ---- */

static void clip_capture_destroy(ClipboardCapture *cap) {
	if (cap->src) {
		wl_event_source_remove(cap->src);
	}
	if (cap->fd >= 0) {
		close(cap->fd);
	}
	free(cap->buf);
	free(cap);
}

static void clip_capture_finish(ClipboardCapture *cap, bool aborted) {
	if (!aborted && cap->len > 0 && cap->len <= cap->cap_limit) {
		wlr_log(WLR_DEBUG, "clipboard: captured %zu bytes (history=%zu)",
				cap->len, clipboard.count + 1);
		uint8_t *take = cap->buf;
		size_t len = cap->len;
		cap->buf = NULL;
		clip_history_push(take, len);
	} else {
		wlr_log(WLR_DEBUG,
				"clipboard: capture aborted=%d len=%zu cap_limit=%zu", aborted,
				cap->len, cap->cap_limit);
	}
	clip_capture_destroy(cap);
}

/* Read end of the pipe is ready: drain into the growable buffer. */
static int clip_capture_readable(int fd, uint32_t mask, void *data) {
	ClipboardCapture *cap = data;
	if (mask & (WL_EVENT_ERROR | WL_EVENT_HANGUP)) {
		uint8_t tmp[4096];
		while (1) {
			ssize_t n = read(fd, tmp, sizeof(tmp));
			if (n <= 0) {
				break;
			}
			if (cap->len + (size_t)n > cap->cap_limit) {
				clip_capture_finish(cap, true);
				return 0;
			}
			if (cap->len + (size_t)n > cap->cap) {
				size_t nc = cap->cap ? cap->cap * 2 : 4096;
				while (nc < cap->len + (size_t)n) {
					nc *= 2;
				}
				uint8_t *nb = realloc(cap->buf, nc);
				if (!nb) {
					clip_capture_finish(cap, true);
					return 0;
				}
				cap->buf = nb;
				cap->cap = nc;
			}
			memcpy(cap->buf + cap->len, tmp, (size_t)n);
			cap->len += (size_t)n;
		}
		clip_capture_finish(cap, false);
		return 0;
	}

	uint8_t tmp[4096];
	ssize_t n = read(fd, tmp, sizeof(tmp));
	if (n < 0) {
		if (errno == EAGAIN || errno == EINTR) {
			return 0;
		}
		clip_capture_finish(cap, true);
		return 0;
	}
	if (n == 0) {
		clip_capture_finish(cap, false);
		return 0;
	}
	if (cap->len + (size_t)n > cap->cap_limit) {
		clip_capture_finish(cap, true);
		return 0;
	}
	if (cap->len + (size_t)n > cap->cap) {
		size_t nc = cap->cap ? cap->cap * 2 : 4096;
		while (nc < cap->len + (size_t)n) {
			nc *= 2;
		}
		uint8_t *nb = realloc(cap->buf, nc);
		if (!nb) {
			clip_capture_finish(cap, true);
			return 0;
		}
		cap->buf = nb;
		cap->cap = nc;
	}
	memcpy(cap->buf + cap->len, tmp, (size_t)n);
	cap->len += (size_t)n;
	return 0;
}

/* Pick the best text-ish MIME exposed by a selection source. */
static const char *clip_pick_text_mime(struct wlr_data_source *src) {
	const char *best = NULL;
	const char **m;
	wl_array_for_each(m, &src->mime_types) {
		if (strcmp(*m, CLIP_MIME_UTF8) == 0) {
			return *m;
		}
		if (!best && strcmp(*m, CLIP_MIME_TEXT) == 0) {
			best = *m;
		}
		if (!best && strcmp(*m, CLIP_MIME_UTF8_ALT) == 0) {
			best = *m;
		}
		if (!best && strcmp(*m, CLIP_MIME_STRING) == 0) {
			best = *m;
		}
	}
	return best;
}

/* Begin an async pipe-read of the selection's text bytes. */
static void clip_capture_from_source(struct wlr_data_source *src) {
	if (!clipboard.enabled || !src) {
		return;
	}
	const char *mime = clip_pick_text_mime(src);
	if (!mime) {
		wlr_log(WLR_DEBUG, "clipboard: no text mime in selection source");
		return;
	}
	wlr_log(WLR_DEBUG, "clipboard: capturing selection mime=%s", mime);
	int p[2];
	if (pipe(p) < 0) {
		return;
	}
	for (int i = 0; i < 2; i++) {
		int fl = fcntl(p[i], F_GETFL, 0);
		fcntl(p[i], F_SETFL, fl | O_NONBLOCK);
		int fd = fcntl(p[i], F_GETFD, 0);
		fcntl(p[i], F_SETFD, fd | FD_CLOEXEC);
	}
	ClipboardCapture *cap = calloc(1, sizeof(*cap));
	if (!cap) {
		close(p[0]);
		close(p[1]);
		return;
	}
	cap->fd = p[0];
	cap->cap_limit = clipboard.max_bytes_per_entry;
	cap->src = wl_event_loop_add_fd(event_loop, p[0],
									WL_EVENT_READABLE | WL_EVENT_HANGUP,
									clip_capture_readable, cap);
	if (!cap->src) {
		close(p[0]);
		close(p[1]);
		free(cap);
		return;
	}
	wlr_data_source_send(src, mime, p[1]);
	close(p[1]);
}

/* ---- our own data_source for paste-from-history ---- */

typedef struct {
	struct wlr_data_source base;
	uint8_t *data;
	size_t len;
	char *mime;
} ClipPasteSource;

/* Stream our cached bytes to whichever client requested the paste. */
static void clip_paste_send(struct wlr_data_source *base, const char *mime_type,
							int32_t fd) {
	(void)mime_type;
	ClipPasteSource *src = wl_container_of(base, src, base);
	size_t off = 0;
	while (off < src->len) {
		ssize_t n = write(fd, src->data + off, src->len - off);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			break;
		}
		off += (size_t)n;
	}
	close(fd);
}

/* Free the heap-owned bytes when the paste source is destroyed. */
static void clip_paste_destroy(struct wlr_data_source *base) {
	ClipPasteSource *src = wl_container_of(base, src, base);
	free(src->data);
	free(src->mime);
	free(src);
}

static const struct wlr_data_source_impl clip_paste_impl = {
	.send = clip_paste_send,
	.destroy = clip_paste_destroy,
};

/* Build a self-owned wlr_data_source carrying entry's bytes, advertise text MIMEs. */
static ClipPasteSource *clip_paste_source_from_entry(const ClipEntry *e) {
	ClipPasteSource *src = calloc(1, sizeof(*src));
	if (!src) {
		return NULL;
	}
	src->data = malloc(e->len);
	if (!src->data) {
		free(src);
		return NULL;
	}
	memcpy(src->data, e->data, e->len);
	src->len = e->len;
	src->mime = strdup(CLIP_MIME_UTF8);
	wlr_data_source_init(&src->base, &clip_paste_impl);

	const char *mimes[] = {CLIP_MIME_UTF8, CLIP_MIME_TEXT, CLIP_MIME_UTF8_ALT,
						   CLIP_MIME_STRING};
	for (size_t i = 0; i < sizeof(mimes) / sizeof(mimes[0]); i++) {
		char **slot = wl_array_add(&src->base.mime_types, sizeof(char *));
		if (slot) {
			*slot = strdup(mimes[i]);
		}
	}
	return src;
}

/* ---- popup ---- */

/* Tear down popup scene tree + cached row buffers. */
static void clip_popup_close(void) {
	if (!clipboard.popup_open) {
		return;
	}
	for (size_t i = 0; i < clipboard.rows_count; i++) {
		if (clipboard.rows[i].buf_ref) {
			wlr_buffer_drop(clipboard.rows[i].buf_ref);
			clipboard.rows[i].buf_ref = NULL;
		}
	}
	free(clipboard.rows);
	clipboard.rows = NULL;
	clipboard.rows_count = 0;
	if (clipboard.popup_tree) {
		wlr_scene_node_destroy(&clipboard.popup_tree->node);
		clipboard.popup_tree = NULL;
	}
	clipboard.popup_bg = NULL;
	clipboard.popup_mon = NULL;
	clipboard.popup_open = false;
}

/* Rebuild rows after the selection moves. */
static void clip_popup_refresh_rows(void) {
	for (size_t i = 0; i < clipboard.rows_count; i++) {
		if (clipboard.rows[i].text) {
			wlr_scene_node_destroy(&clipboard.rows[i].text->node);
			clipboard.rows[i].text = NULL;
		}
		if (clipboard.rows[i].buf_ref) {
			wlr_buffer_drop(clipboard.rows[i].buf_ref);
			clipboard.rows[i].buf_ref = NULL;
		}
		int inner_w = clipboard.popup_w - 2 * CLIP_POPUP_PAD;
		const char *text =
			clipboard.count > 0
				? clipboard.entries[i].preview
				: "(clipboard history empty -- copy something to fill it)";
		bool selected =
			clipboard.count > 0 && (int)i == clipboard.selected;
		struct wlr_buffer *buf =
			clip_render_row(inner_w, CLIP_ROW_H, text, selected);
		if (!buf) {
			continue;
		}
		clipboard.rows[i].buf_ref = buf;
		clipboard.rows[i].text =
			wlr_scene_buffer_create(clipboard.popup_tree, buf);
		if (clipboard.rows[i].text) {
			wlr_scene_node_set_position(
				&clipboard.rows[i].text->node, CLIP_POPUP_PAD,
				CLIP_POPUP_PAD + (int)i * CLIP_ROW_H);
		}
	}
}

/* Build and show the popup on the focused monitor; shows an empty-state
 * row when the history is empty so the user gets visual feedback. */
static void clip_popup_open(void) {
	if (!clipboard.enabled || clipboard.popup_open || !selmon) {
		wlr_log(WLR_INFO, "clipboard popup open: enabled=%d open=%d selmon=%p",
				clipboard.enabled, clipboard.popup_open, (void *)selmon);
		return;
	}
	clipboard.popup_mon = selmon;
	clipboard.selected = 0;
	/* LyrFadeOut: xytonode skips this layer, so the cairo-backed
	   wlr_buffer scene_buffer nodes (no wlr_scene_surface backing)
	   cannot trip wlr_scene_surface_try_from_buffer()->surface on
	   pointer hit-tests. Identical hazard to the Alt+Tab cycler. */
	clipboard.popup_tree = wlr_scene_tree_create(layers[LyrFadeOut]);
	if (!clipboard.popup_tree) {
		return;
	}

	int rows = (int)clipboard.count;
	if (rows == 0) {
		rows = 1;
	}
	clipboard.popup_w = CLIP_POPUP_W;
	clipboard.popup_h = 2 * CLIP_POPUP_PAD + rows * CLIP_ROW_H;

	float bg_color[4] = {0.09f, 0.08f, 0.06f, 0.92f};
	clipboard.popup_bg = wlr_scene_rect_create(
		clipboard.popup_tree, clipboard.popup_w, clipboard.popup_h, bg_color);
	if (clipboard.popup_bg) {
		wlr_scene_rect_set_corner_radius(clipboard.popup_bg,
										 CLIP_POPUP_RADIUS,
										 CORNER_LOCATION_ALL);
	}

	clipboard.rows = calloc((size_t)rows, sizeof(ClipRow));
	clipboard.rows_count = (size_t)rows;
	clip_popup_refresh_rows();

	int mon_x = selmon->m.x;
	int mon_y = selmon->m.y;
	int mon_w = selmon->m.width;
	int mon_h = selmon->m.height;
	int px = mon_x + (mon_w - clipboard.popup_w) / 2;
	int py = mon_y + mon_h - clipboard.popup_h - CLIP_POPUP_MARGIN_BOTTOM;
	wlr_scene_node_set_position(&clipboard.popup_tree->node, px, py);
	wlr_scene_node_raise_to_top(&clipboard.popup_tree->node);
	clipboard.popup_open = true;
}

/* Move the selected row by delta with clamp. */
static void clip_popup_move(int delta) {
	if (!clipboard.popup_open || clipboard.rows_count == 0) {
		return;
	}
	int next = clipboard.selected + delta;
	if (next < 0) {
		next = 0;
	} else if (next >= (int)clipboard.rows_count) {
		next = (int)clipboard.rows_count - 1;
	}
	if (next == clipboard.selected) {
		return;
	}
	clipboard.selected = next;
	clip_popup_refresh_rows();
}

/* Synthesise a paste-from-history selection from the chosen row, close popup. */
static void clip_popup_pick(void) {
	if (!clipboard.popup_open || clipboard.selected < 0 ||
		clipboard.selected >= (int)clipboard.count) {
		clip_popup_close();
		return;
	}
	ClipPasteSource *src =
		clip_paste_source_from_entry(&clipboard.entries[clipboard.selected]);
	clip_popup_close();
	if (!src) {
		return;
	}
	clipboard.ignore_next = true;
	wlr_seat_set_selection(seat, &src->base,
						   wl_display_next_serial(dpy));
}

/* Toggle visibility — entry point bound to a keybind. */
static void clip_popup_toggle(void) {
	wlr_log(WLR_DEBUG,
			"clipboard: toggle (enabled=%d open=%d count=%zu selmon=%p)",
			clipboard.enabled, clipboard.popup_open, clipboard.count,
			(void *)selmon);
	if (clipboard.popup_open) {
		clip_popup_close();
	} else {
		clip_popup_open();
	}
}

/* ---- lifecycle ---- */

/* Allocate the ring after config has been parsed. */
static void clip_init(void) {
	if (clipboard.entries) {
		return;
	}
	if (clipboard.max_entries == 0) {
		clipboard.max_entries = 100;
	}
	if (clipboard.max_bytes_per_entry == 0) {
		clipboard.max_bytes_per_entry = 1u * 1024u * 1024u;
	}
	clipboard.entries =
		ecalloc(clipboard.max_entries, sizeof(ClipEntry));
	clipboard.capacity = clipboard.max_entries;
}

/* Drop all entries and pop down the popup. */
static void clip_cleanup(void) {
	clip_popup_close();
	if (clipboard.entries) {
		clip_history_clear();
		free(clipboard.entries);
		clipboard.entries = NULL;
	}
	clipboard.capacity = 0;
}
