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
#define CLIP_MIME_PNG "image/png"
#define CLIP_MIME_JPEG "image/jpeg"
#define CLIP_MIME_JPG "image/jpg"
#define CLIP_MIME_BMP "image/bmp"
#define CLIP_MIME_WEBP "image/webp"
#define CLIP_MIME_GIF "image/gif"
#define CLIP_MIME_TIFF "image/tiff"

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
	/* Heap-owned MIME string. NULL means "text" (legacy entries). Any
	   non-NULL value starting with "image/" is treated as an image. */
	char *mime;
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
	/* Heap-owned MIME copy attached to the freshly-captured bytes. */
	char *mime;
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
		cairo_set_source_rgba(cr, config.clipboard_selected[0],
							  config.clipboard_selected[1],
							  config.clipboard_selected[2],
							  config.clipboard_selected[3]);
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
		cairo_set_source_rgba(cr, config.clipboard_text_selected[0],
							  config.clipboard_text_selected[1],
							  config.clipboard_text_selected[2],
							  config.clipboard_text_selected[3]);
	} else {
		cairo_set_source_rgba(cr, config.clipboard_text[0],
							  config.clipboard_text[1],
							  config.clipboard_text[2],
							  config.clipboard_text[3]);
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

/* ---- image preview decode ---- */

typedef struct {
	const uint8_t *data;
	size_t len;
	size_t off;
} ClipPngReader;

static cairo_status_t clip_png_read_cb(void *closure, unsigned char *out,
									   unsigned int n) {
	ClipPngReader *r = closure;
	if (r->off + n > r->len)
		return CAIRO_STATUS_READ_ERROR;
	memcpy(out, r->data + r->off, n);
	r->off += n;
	return CAIRO_STATUS_SUCCESS;
}

/* Decode a PNG byte buffer via cairo's streaming PNG loader. Returns
   NULL when bytes aren't a valid PNG -- caller falls back to a text
   placeholder. Guards against malformed inputs by verifying the 8-byte
   PNG magic before invoking cairo (libpng has been observed to abort
   on certain truncated or non-PNG payloads). */
static cairo_surface_t *clip_decode_png(const uint8_t *data, size_t len) {
	if (!data || len < 8)
		return NULL;
	static const uint8_t png_magic[8] = {0x89, 0x50, 0x4E, 0x47,
										 0x0D, 0x0A, 0x1A, 0x0A};
	if (memcmp(data, png_magic, 8) != 0)
		return NULL;
	ClipPngReader r = {data, len, 0};
	cairo_surface_t *s =
		cairo_image_surface_create_from_png_stream(clip_png_read_cb, &r);
	if (!s)
		return NULL;
	if (cairo_surface_status(s) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(s);
		return NULL;
	}
	return s;
}

/* Render an image-entry row. Decoding the PNG into a thumbnail is the
   expensive step (typical PNG payload sits on the multi-MiB side); we
   only spend it when the row is actually selected. Non-selected image
   rows render the placeholder + label, which keeps popup_open cost
   constant in the number of stored entries even when the history is
   dominated by screenshots. */
static struct wlr_buffer *clip_render_image_row(int w, int h,
												const ClipEntry *e,
												bool selected,
												bool decode_image) {
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
		cairo_set_source_rgba(cr, config.clipboard_selected[0],
							  config.clipboard_selected[1],
							  config.clipboard_selected[2],
							  config.clipboard_selected[3]);
		cairo_rectangle(cr, 0, 0, w, h);
		cairo_fill(cr);
	}

	int thumb_w = h - 6;
	int thumb_h = h - 6;
	int thumb_x = 3;
	int thumb_y = 3;
	int img_w = 0, img_h = 0;

	cairo_surface_t *img = NULL;
	if (decode_image && e->mime && strcmp(e->mime, CLIP_MIME_PNG) == 0)
		img = clip_decode_png(e->data, e->len);

	if (img) {
		img_w = cairo_image_surface_get_width(img);
		img_h = cairo_image_surface_get_height(img);
		double sx = (double)thumb_w / img_w;
		double sy = (double)thumb_h / img_h;
		double s = sx < sy ? sx : sy;
		int draw_w = (int)(img_w * s);
		int draw_h = (int)(img_h * s);
		int dx = thumb_x + (thumb_w - draw_w) / 2;
		int dy = thumb_y + (thumb_h - draw_h) / 2;
		cairo_save(cr);
		cairo_translate(cr, dx, dy);
		cairo_scale(cr, s, s);
		cairo_set_source_surface(cr, img, 0, 0);
		cairo_paint(cr);
		cairo_restore(cr);
		cairo_surface_destroy(img);
	} else {
		/* No decoded preview: paint a dark rounded placeholder square. */
		cairo_set_source_rgba(cr, 0.18, 0.18, 0.22, 1.0);
		cairo_rectangle(cr, thumb_x, thumb_y, thumb_w, thumb_h);
		cairo_fill(cr);
	}

	/* Text label to the right of the thumbnail. */
	const char *fmt = e->mime ? e->mime : "image";
	if (strncmp(fmt, "image/", 6) == 0)
		fmt += 6;
	char label[160];
	if (img_w > 0 && img_h > 0) {
		double kib = e->len / 1024.0;
		if (kib >= 1024.0)
			snprintf(label, sizeof(label), "%s %dx%d, %.1f MiB", fmt, img_w,
					 img_h, kib / 1024.0);
		else
			snprintf(label, sizeof(label), "%s %dx%d, %.1f KiB", fmt, img_w,
					 img_h, kib);
	} else {
		double kib = e->len / 1024.0;
		if (kib >= 1024.0)
			snprintf(label, sizeof(label), "%s %.1f MiB", fmt, kib / 1024.0);
		else
			snprintf(label, sizeof(label), "%s %.1f KiB", fmt, kib);
	}

	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoFontDescription *desc =
		pango_font_description_from_string("Sans 11");
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);
	pango_layout_set_text(layout, label, -1);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	int label_x = thumb_x + thumb_w + CLIP_ROW_PAD_X;
	pango_layout_set_width(layout, (w - label_x - CLIP_ROW_PAD_X) * PANGO_SCALE);
	pango_layout_set_single_paragraph_mode(layout, TRUE);

	int tw, th;
	pango_layout_get_pixel_size(layout, &tw, &th);
	cairo_move_to(cr, label_x, (h - th) / 2);
	if (selected)
		cairo_set_source_rgba(cr, config.clipboard_text_selected[0],
							  config.clipboard_text_selected[1],
							  config.clipboard_text_selected[2],
							  config.clipboard_text_selected[3]);
	else
		cairo_set_source_rgba(cr, config.clipboard_text[0],
							  config.clipboard_text[1],
							  config.clipboard_text[2],
							  config.clipboard_text[3]);
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
	free(e->mime);
	e->data = NULL;
	e->mime = NULL;
	e->len = 0;
}

/* True when the entry's MIME indicates an image (image/...). */
static inline bool clip_entry_is_image(const ClipEntry *e) {
	return e && e->mime && strncmp(e->mime, "image/", 6) == 0;
}

/* Build a human-readable preview line for an image entry. */
static void clip_image_preview(char *out, const char *mime, size_t len) {
	const char *fmt = mime ? mime : "image";
	if (strncmp(fmt, "image/", 6) == 0)
		fmt += 6;
	double kib = len / 1024.0;
	if (kib >= 1024.0)
		snprintf(out, CLIP_PREVIEW_BYTES, "[Image %s, %.1f MiB]", fmt,
				 kib / 1024.0);
	else
		snprintf(out, CLIP_PREVIEW_BYTES, "[Image %s, %.1f KiB]", fmt, kib);
}

/* Drop every stored entry. */
static void clip_history_clear(void) {
	for (size_t i = 0; i < clipboard.count; i++) {
		clip_entry_free(&clipboard.entries[i]);
	}
	clipboard.count = 0;
}

/* Insert a freshly captured byte buffer at the front, dedup vs head,
 * drop the tail beyond max_entries. Takes ownership of data and mime.
 * mime may be NULL for legacy text entries. */
static void clip_history_push(uint8_t *data, size_t len, char *mime) {
	if (len == 0) {
		free(data);
		free(mime);
		return;
	}
	if (clipboard.count > 0) {
		ClipEntry *head = &clipboard.entries[0];
		bool same_kind =
			(head->mime == NULL && mime == NULL) ||
			(head->mime && mime && strcmp(head->mime, mime) == 0);
		if (same_kind && head->len == len &&
			memcmp(head->data, data, len) == 0) {
			free(data);
			free(mime);
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
	clipboard.entries[0].mime = mime;
	clipboard.entries[0].ts_ms = get_now_in_ms();
	if (mime && strncmp(mime, "image/", 6) == 0)
		clip_image_preview(clipboard.entries[0].preview, mime, len);
	else
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
	free(cap->mime);
	free(cap);
}

static void clip_capture_finish(ClipboardCapture *cap, bool aborted) {
	if (!aborted && cap->len > 0 && cap->len <= cap->cap_limit) {
		wlr_log(WLR_DEBUG,
				"clipboard: captured %zu bytes mime=%s (history=%zu)", cap->len,
				cap->mime ? cap->mime : "(text)", clipboard.count + 1);
		uint8_t *take = cap->buf;
		size_t len = cap->len;
		char *mime = cap->mime;
		cap->buf = NULL;
		cap->mime = NULL;
		clip_history_push(take, len, mime);
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

/* Pick the best supported MIME (text or image) exposed by a selection
   source. Returns NULL if the source advertises nothing we capture.
   Sets *out_is_image to true when the chosen mime is image/...
   Preference: PNG > other image formats > text/plain;charset=utf-8 >
   text/plain > UTF8_STRING > STRING. Apps usually advertise multiple
   mimes for the same payload; we treat image and text selections as
   distinct entry kinds so paste replays the right format. */
static const char *clip_pick_capture_mime(struct wlr_data_source *src,
										  bool *out_is_image) {
	const char *best_text = NULL;
	const char *best_image = NULL;
	const char **m;
	wl_array_for_each(m, &src->mime_types) {
		if (strcmp(*m, CLIP_MIME_PNG) == 0)
			return (*out_is_image = true), *m;
		if (!best_image && (strcmp(*m, CLIP_MIME_JPEG) == 0 ||
							strcmp(*m, CLIP_MIME_JPG) == 0 ||
							strcmp(*m, CLIP_MIME_BMP) == 0 ||
							strcmp(*m, CLIP_MIME_WEBP) == 0 ||
							strcmp(*m, CLIP_MIME_GIF) == 0 ||
							strcmp(*m, CLIP_MIME_TIFF) == 0))
			best_image = *m;
		if (strcmp(*m, CLIP_MIME_UTF8) == 0) {
			best_text = *m;
			break;
		}
		if (!best_text && strcmp(*m, CLIP_MIME_TEXT) == 0)
			best_text = *m;
		if (!best_text && strcmp(*m, CLIP_MIME_UTF8_ALT) == 0)
			best_text = *m;
		if (!best_text && strcmp(*m, CLIP_MIME_STRING) == 0)
			best_text = *m;
	}
	if (best_image) {
		*out_is_image = true;
		return best_image;
	}
	*out_is_image = false;
	return best_text;
}

/* Begin an async pipe-read of the selection's bytes. Captures text and
   image selections; entry MIME is recorded on the capture struct so
   the eventual history push tags the entry correctly. */
static void clip_capture_from_source(struct wlr_data_source *src) {
	if (!clipboard.enabled || !src) {
		return;
	}
	bool is_image = false;
	const char *mime = clip_pick_capture_mime(src, &is_image);
	if (!mime) {
		wlr_log(WLR_DEBUG, "clipboard: no supported mime in selection source");
		return;
	}
	wlr_log(WLR_DEBUG, "clipboard: capturing selection mime=%s image=%d", mime,
			is_image);
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
	cap->mime = strdup(mime);
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

/* In-flight async paste: keeps writing whatever didn't fit in the
   pipe buffer when clip_paste_send was first called. Decoupled from
   ClipPasteSource because the source may get destroyed before the
   paste drains (the seat replaces our selection while a slow client
   still holds the pipe). */
typedef struct {
	int fd;
	uint8_t *data;
	size_t len;
	size_t off;
	struct wl_event_source *src;
} ClipPasteFlight;

static void clip_paste_flight_destroy(ClipPasteFlight *f) {
	if (f->src)
		wl_event_source_remove(f->src);
	if (f->fd >= 0)
		close(f->fd);
	free(f->data);
	free(f);
}

/* Event loop callback: pipe is writable again, push more bytes. */
static int clip_paste_writable(int fd, uint32_t mask, void *data) {
	ClipPasteFlight *f = data;
	if (mask & (WL_EVENT_ERROR | WL_EVENT_HANGUP)) {
		clip_paste_flight_destroy(f);
		return 0;
	}
	while (f->off < f->len) {
		ssize_t n = write(fd, f->data + f->off, f->len - f->off);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN)
				return 0;
			clip_paste_flight_destroy(f);
			return 0;
		}
		f->off += (size_t)n;
	}
	clip_paste_flight_destroy(f);
	return 0;
}

/* Stream our cached bytes to whichever client requested the paste.
   Switched to non-blocking + async drain so a large image payload
   doesn't deadlock the main event loop waiting for a slow consumer
   to drain its end of the pipe. Text-sized writes still go through
   in one shot via the synchronous prologue. */
static void clip_paste_send(struct wlr_data_source *base, const char *mime_type,
							int32_t fd) {
	(void)mime_type;
	ClipPasteSource *src = wl_container_of(base, src, base);
	int fl = fcntl(fd, F_GETFL, 0);
	if (fl >= 0)
		fcntl(fd, F_SETFL, fl | O_NONBLOCK);

	size_t off = 0;
	while (off < src->len) {
		ssize_t n = write(fd, src->data + off, src->len - off);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN) {
				/* Pipe buffer full -- copy the remainder and hand
				   it to the event loop. The source struct can be
				   destroyed before the paste completes; the flight
				   owns its own data copy so that is safe. */
				ClipPasteFlight *f = calloc(1, sizeof(*f));
				if (!f) {
					close(fd);
					return;
				}
				f->fd = fd;
				f->len = src->len - off;
				f->data = malloc(f->len);
				if (!f->data) {
					free(f);
					close(fd);
					return;
				}
				memcpy(f->data, src->data + off, f->len);
				f->off = 0;
				f->src = wl_event_loop_add_fd(
					event_loop, fd,
					WL_EVENT_WRITABLE | WL_EVENT_HANGUP,
					clip_paste_writable, f);
				if (!f->src) {
					free(f->data);
					free(f);
					close(fd);
				}
				return;
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

/* Build a self-owned wlr_data_source carrying entry's bytes. For text
   entries we advertise all of the historical text MIME aliases so any
   paste target finds a match. For image entries we advertise only the
   captured MIME -- claiming text/plain on raw PNG bytes would let a
   text paste fall through and dump binary into the focused field. */
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
	bool is_image = clip_entry_is_image(e);
	src->mime = strdup(is_image ? e->mime : CLIP_MIME_UTF8);
	wlr_data_source_init(&src->base, &clip_paste_impl);

	if (is_image) {
		char **slot = wl_array_add(&src->base.mime_types, sizeof(char *));
		if (slot)
			*slot = strdup(e->mime);
	} else {
		const char *mimes[] = {CLIP_MIME_UTF8, CLIP_MIME_TEXT,
							   CLIP_MIME_UTF8_ALT, CLIP_MIME_STRING};
		for (size_t i = 0; i < sizeof(mimes) / sizeof(mimes[0]); i++) {
			char **slot =
				wl_array_add(&src->base.mime_types, sizeof(char *));
			if (slot)
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
		bool selected =
			clipboard.count > 0 && (int)i == clipboard.selected;
		struct wlr_buffer *buf = NULL;
		if (clipboard.count > 0 && clip_entry_is_image(&clipboard.entries[i])) {
			buf = clip_render_image_row(inner_w, CLIP_ROW_H,
										&clipboard.entries[i], selected,
										selected);
		}
		if (!buf) {
			const char *text =
				clipboard.count > 0
					? clipboard.entries[i].preview
					: "(clipboard history empty -- copy something to fill it)";
			buf = clip_render_row(inner_w, CLIP_ROW_H, text, selected);
		}
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

	clipboard.popup_bg = wlr_scene_rect_create(
		clipboard.popup_tree, clipboard.popup_w, clipboard.popup_h,
		config.clipboard_bg);
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

/* Inject a Ctrl+V key sequence to the surface that currently has the
   keyboard focus. The popup never grabs focus (LyrFadeOut, not in the
   seat keyboard chain), so focused_surface still points at whatever
   text input the user had active before invoking the popup. We
   advertise Ctrl down via wl_keyboard.modifiers, replay key 47 (V)
   press+release, then restore the prior modifier mask so the real
   keyboard state stays consistent. Linux keycodes are used directly
   (29 = KEY_LEFTCTRL, 47 = KEY_V) -- xkb adds the +8 X11 offset
   inside wlr_seat. */
static void clip_synth_ctrl_v(void) {
	if (!seat || !seat->keyboard_state.focused_surface)
		return;
	struct wlr_keyboard *kb = wlr_seat_get_keyboard(seat);
	if (!kb)
		return;

	struct wlr_keyboard_modifiers saved = kb->modifiers;
	struct wlr_keyboard_modifiers ctrl = {
		.depressed = WLR_MODIFIER_CTRL,
		.latched = 0,
		.locked = saved.locked,
		.group = saved.group,
	};
	uint32_t t = (uint32_t)get_now_in_ms();

	wlr_seat_keyboard_notify_modifiers(seat, &ctrl);
	wlr_seat_keyboard_notify_key(seat, t, 47, WL_KEYBOARD_KEY_STATE_PRESSED);
	wlr_seat_keyboard_notify_key(seat, t, 47, WL_KEYBOARD_KEY_STATE_RELEASED);
	wlr_seat_keyboard_notify_modifiers(seat, &saved);
}

/* Synthesise a paste-from-history selection from the chosen row, close
   popup, then fire Ctrl+V at the focused surface so the app pastes the
   entry directly into wherever the user's text cursor was sitting. */
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
	clip_synth_ctrl_v();
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
