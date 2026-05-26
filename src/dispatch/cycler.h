/* Alt+Tab window cycler.
   Instead of building scaled-down thumbnails, the cycler resizes the
   real client windows into a uniform grid (same algorithm as the
   overview layout), then overlays a dim background + selection /
   hover borders + numbered badges anchored to each client's grid
   cell. The currently-selected entry is highlighted by a bright
   border. Selection commits on the configured modifier release
   (driven from input/keypress.h). Dragging a tile with BTN_LEFT
   relocates it in the grid; releasing over another cell swaps the
   two clients in the global wl_list so the tiling order is updated
   after restore. */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct CyclerBackup {
	struct wlr_box geom;
	struct wlr_box float_geom;
	int32_t bw;
	bool isfloating;
	bool isfullscreen;
	bool ismaximizescreen;
	bool iscustomsize;
} CyclerBackup;

static struct WindowCycler {
	bool active;
	int32_t index;
	int32_t hover;
	int32_t count;
	Client **clients;
	CyclerBackup *backup;
	struct wlr_box *cells;
	/* root sits on LyrFadeOut so borders/badges draw above every
	   client. bg_tree sits on LyrBottom so the dim rect only covers
	   the wallpaper, not the real windows we resized into the
	   grid. */
	struct wlr_scene_tree *root;
	struct wlr_scene_tree *bg_tree;
	struct wlr_scene_rect *bg;
	/* Single shared white outline frame (4 thin strips: top, right,
	   bottom, left) that follows the *active* cell. The active cell
	   is the one under the cursor when there is a hover; otherwise
	   the keyboard-selected index. Only one cell carries a border
	   at any time so there is no "two highlighted cells" confusion
	   between hover and arrow-selection. */
	struct wlr_scene_rect *active_frame[4];
	struct wlr_scene_buffer **badges;
	struct wlr_buffer **badge_buffers;
	Monitor *mon;
	/* Drag state. drag_idx >= 0 while a tile follows the cursor. */
	int32_t drag_idx;
	bool drag_moved;
	double drag_grab_dx, drag_grab_dy;
	int32_t cell_pad;
	/* Grid shape, recorded by compute_cells so arrow-key navigation
	   knows the column width without re-deriving it from cell
	   positions. */
	int32_t grid_cols;
	int32_t grid_rows;
	/* Sticky mode: any non-cycler keybind that fires while the
	   cycler is open (typically a launcher like Alt+E) flips this
	   on. While sticky, releasing the cycler modifier no longer
	   auto-commits -- the cycler stays open like an overview so the
	   freshly spawned window has time to map and join the grid via
	   add_client. The user dismisses with Escape, a click on a cell,
	   or a digit jump. */
	bool sticky;
} window_cycler;

typedef struct CyclerBadgeBuffer {
	struct wlr_buffer base;
	uint8_t *data;
	size_t stride;
} CyclerBadgeBuffer;

/* Hand the renderer a pointer to the cached pixel data. */
static bool cycler_badge_begin(struct wlr_buffer *base, uint32_t flags,
							   void **data, uint32_t *format, size_t *stride) {
	CyclerBadgeBuffer *b = wl_container_of(base, b, base);
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE)
		return false;
	*data = b->data;
	*format = DRM_FORMAT_ARGB8888;
	*stride = b->stride;
	return true;
}

static void cycler_badge_end(struct wlr_buffer *base) { (void)base; }

static void cycler_badge_destroy(struct wlr_buffer *base) {
	CyclerBadgeBuffer *b = wl_container_of(base, b, base);
	free(b->data);
	free(b);
}

static const struct wlr_buffer_impl cycler_badge_buffer_impl = {
	.destroy = cycler_badge_destroy,
	.begin_data_ptr_access = cycler_badge_begin,
	.end_data_ptr_access = cycler_badge_end,
};

/* Render a single digit (or short string) onto a fresh ARGB8888
   wlr_buffer. Background is a rounded dark pill; the text is drawn
   centered in white. Returns NULL on allocation failure -- caller
   tolerates missing badges. */
static struct wlr_buffer *cycler_render_badge(const char *text, int32_t size) {
	if (size < 16)
		size = 16;
	int32_t w = size;
	int32_t h = size;
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

	double r = h * 0.5;
	double pad = 1.0;
	cairo_set_source_rgba(cr, 0.05, 0.05, 0.07, 0.92);
	cairo_new_sub_path(cr);
	cairo_arc(cr, r + pad, r, r - pad, M_PI / 2, 3 * M_PI / 2);
	cairo_arc(cr, w - r - pad, r, r - pad, 3 * M_PI / 2, M_PI / 2);
	cairo_close_path(cr);
	cairo_fill(cr);

	PangoLayout *layout = pango_cairo_create_layout(cr);
	char font_spec[64];
	snprintf(font_spec, sizeof(font_spec), "Sans Bold %d",
			 (int32_t)(size * 0.5));
	PangoFontDescription *desc = pango_font_description_from_string(font_spec);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);
	pango_layout_set_text(layout, text, -1);

	int32_t tw, th;
	pango_layout_get_pixel_size(layout, &tw, &th);
	cairo_move_to(cr, (w - tw) / 2.0, (h - th) / 2.0);
	cairo_set_source_rgba(cr, 1, 1, 1, 1);
	pango_cairo_show_layout(cr, layout);

	g_object_unref(layout);
	cairo_destroy(cr);
	cairo_surface_flush(surf);

	int32_t stride = cairo_image_surface_get_stride(surf);
	size_t total = (size_t)stride * (size_t)h;
	uint8_t *pixels = malloc(total);
	if (!pixels) {
		cairo_surface_destroy(surf);
		return NULL;
	}
	memcpy(pixels, cairo_image_surface_get_data(surf), total);
	cairo_surface_destroy(surf);

	CyclerBadgeBuffer *b = calloc(1, sizeof(*b));
	if (!b) {
		free(pixels);
		return NULL;
	}
	wlr_buffer_init(&b->base, &cycler_badge_buffer_impl, w, h);
	b->data = pixels;
	b->stride = (size_t)stride;
	return &b->base;
}

/* Last tiled client the cycler promoted to LyrTop so it draws above
   floating siblings. Reset back to LyrTile by focusclient as soon as
   the user focuses something else. */
static Client *cycler_raised_tile = NULL;

/* Drop the cycler's promoted client back into its native layer. */
static void cycler_drop_raised_tile(void) {
	if (!cycler_raised_tile)
		return;
	Client *t = cycler_raised_tile;
	cycler_raised_tile = NULL;
	if (t->iskilling || !t->scene)
		return;
	int32_t target_layer = LyrTile;
	if (t->isoverlay)
		target_layer = LyrOverlay;
	else if (t->isfloating || t->isfullscreen)
		target_layer = LyrTop;
	wlr_scene_node_reparent(&t->scene->node, layers[target_layer]);
}

/* Predicate matching the clients eligible for the cycler list.
   Strict: current monitor, current tagset, not minimized, mapped. The
   cycler is intentionally per-workspace; cross-tag jumps go through
   overview / the tag-switch binds instead. */
static inline bool window_cycler_eligible(Client *c, Monitor *m) {
	return c && c->mon == m && !c->iskilling && !c->isunglobal &&
		   !client_is_unmanaged(c) && !client_is_x11_popup(c) &&
		   !c->isminimized && VISIBLEON(c, m);
}

/* Compute grid layout for n cells inside the monitor's usable area.
   Cells are clamped so a small client count does not stretch the
   thumbnails to fullscreen: max cell width = 45% of monitor work
   area, max cell height = 65%. Grid is centered after clamping. */
static void window_cycler_compute_cells(Monitor *m) {
	const int32_t n = window_cycler.count;
	if (n <= 0)
		return;
	const int32_t gap = 28;
	const int32_t pad = 48;
	int32_t cols, rows;

	if (n == 2) {
		cols = 2;
		rows = 1;
	} else {
		for (cols = 0; cols <= n / 2; cols++) {
			if (cols * cols >= n)
				break;
		}
		if (cols < 1)
			cols = 1;
		rows = (cols && (cols - 1) * cols >= n) ? cols - 1 : cols;
		if (rows < 1)
			rows = 1;
	}
	int32_t overcols = n % cols;

	int32_t avail_w = m->w.width - 2 * pad - (cols - 1) * gap;
	int32_t avail_h = m->w.height - 2 * pad - (rows - 1) * gap;
	if (avail_w < cols)
		avail_w = cols;
	if (avail_h < rows)
		avail_h = rows;
	int32_t cw = avail_w / cols;
	int32_t ch = avail_h / rows;

	/* Clamp to a maximum cell size so n=2 (or any low count) does
	   not render each thumbnail at half-screen. */
	int32_t max_cw = (int32_t)(m->w.width * 0.45);
	int32_t max_ch = (int32_t)(m->w.height * 0.65);
	if (cw > max_cw)
		cw = max_cw;
	if (ch > max_ch)
		ch = max_ch;

	/* Center grid horizontally + vertically after clamp. */
	int32_t total_w = cols * cw + (cols - 1) * gap;
	int32_t total_h = rows * ch + (rows - 1) * gap;
	int32_t origin_x = m->w.x + (m->w.width - total_w) / 2;
	int32_t origin_y = m->w.y + (m->w.height - total_h) / 2;

	int32_t dx = 0;
	if (overcols) {
		int32_t partial_w = overcols * cw + (overcols - 1) * gap;
		dx = (total_w - partial_w) / 2;
	}

	for (int32_t i = 0; i < n; i++) {
		int32_t c_idx = i % cols;
		int32_t r_idx = i / cols;
		int32_t x = origin_x + c_idx * (cw + gap);
		int32_t y = origin_y + r_idx * (ch + gap);
		if (overcols && i >= n - overcols)
			x += dx;
		window_cycler.cells[i] = (struct wlr_box){
			.x = x, .y = y, .width = cw, .height = ch};
	}
	window_cycler.cell_pad = pad;
	window_cycler.grid_cols = cols;
	window_cycler.grid_rows = rows;
}

/* Position the 4 strips that make up a single frame around box b
   with outer thickness `t`. strips[0]=top, [1]=right, [2]=bottom,
   [3]=left. */
static void window_cycler_position_frame(struct wlr_scene_rect *strips[4],
										 struct wlr_box b, int32_t t) {
	if (strips[0]) {
		wlr_scene_rect_set_size(strips[0], b.width + 2 * t, t);
		wlr_scene_node_set_position(&strips[0]->node, b.x - t, b.y - t);
	}
	if (strips[1]) {
		wlr_scene_rect_set_size(strips[1], t, b.height);
		wlr_scene_node_set_position(&strips[1]->node, b.x + b.width, b.y);
	}
	if (strips[2]) {
		wlr_scene_rect_set_size(strips[2], b.width + 2 * t, t);
		wlr_scene_node_set_position(&strips[2]->node, b.x - t, b.y + b.height);
	}
	if (strips[3]) {
		wlr_scene_rect_set_size(strips[3], t, b.height);
		wlr_scene_node_set_position(&strips[3]->node, b.x - t, b.y);
	}
}

/* Enable/disable all 4 strips of a frame at once. */
static void window_cycler_set_frame_enabled(struct wlr_scene_rect *strips[4],
											bool enabled) {
	for (int32_t j = 0; j < 4; j++) {
		if (strips[j])
			wlr_scene_node_set_enabled(&strips[j]->node, enabled);
	}
}

/* Move overlay nodes for client i's current cell (right now: just
   the badge, since the active outline frame is shared and gets
   repositioned in refresh_highlight). */
static void window_cycler_layout_overlay(int32_t i) {
	if (i < 0 || i >= window_cycler.count)
		return;
	struct wlr_box b = window_cycler.cells[i];
	if (window_cycler.badges[i]) {
		int32_t bw = window_cycler.badges[i]->dst_width;
		int32_t inset = bw / 4;
		wlr_scene_node_set_position(&window_cycler.badges[i]->node,
									b.x + inset, b.y + inset);
	}
}

/* Apply cell i to client i: resize the real window into the cell.
   Flips c->ov_anim so the spring uses the smoother
   animation_spring_overview_{tension,friction} pair instead of the
   stiffer default move spring. */
static void window_cycler_apply_cell(int32_t i) {
	if (i < 0 || i >= window_cycler.count)
		return;
	Client *c = window_cycler.clients[i];
	if (!c)
		return;
	c->ov_anim = true;
	resize(c, window_cycler.cells[i], 0);
}

/* Restore one client from its backup and animate it back. ov_anim
   stays on for the return trip so the restore matches the build
   animation curve. */
static void window_cycler_restore_client(int32_t i) {
	if (i < 0 || i >= window_cycler.count)
		return;
	Client *c = window_cycler.clients[i];
	if (!c || c->iskilling)
		return;
	CyclerBackup *bk = &window_cycler.backup[i];
	c->isfloating = bk->isfloating;
	c->isfullscreen = bk->isfullscreen;
	c->ismaximizescreen = bk->ismaximizescreen;
	c->iscustomsize = bk->iscustomsize;
	c->bw = bk->bw;
	c->float_geom = bk->float_geom;
	c->ov_anim = true;
	if (bk->isfullscreen || bk->ismaximizescreen) {
		client_pending_fullscreen_state(c, bk->isfullscreen ? 1 : 0);
		client_pending_maximized_state(c, bk->ismaximizescreen ? 1 : 0);
	}
	resize(c, bk->geom, 0);
}

/* Move the single shared outline frame onto whichever cell is
   "active" (cursor-hover wins; keyboard-selected index otherwise).
   Only one cell ever shows a frame, so there is no confusion
   between hover and arrow position. */
static void window_cycler_refresh_highlight(void) {
	if (window_cycler.count <= 0)
		return;
	int32_t active = (window_cycler.hover >= 0 &&
					  window_cycler.hover < window_cycler.count)
						 ? window_cycler.hover
						 : window_cycler.index;
	if (active < 0 || active >= window_cycler.count) {
		window_cycler_set_frame_enabled(window_cycler.active_frame, false);
		return;
	}
	const int32_t border = 6;
	window_cycler_position_frame(window_cycler.active_frame,
								 window_cycler.cells[active], border);
	window_cycler_set_frame_enabled(window_cycler.active_frame, true);
}

/* Hit-test grid cells against (x, y). Returns the cell index under
   the cursor, or -1 when none. */
static int32_t window_cycler_pick_at(double x, double y) {
	for (int32_t k = 0; k < window_cycler.count; k++) {
		struct wlr_box *b = &window_cycler.cells[k];
		if (x >= b->x && x < b->x + b->width &&
			y >= b->y && y < b->y + b->height) {
			return k;
		}
	}
	return -1;
}

/* Update the hover index from cursor coords. */
static void window_cycler_hover_at(double x, double y) {
	if (!window_cycler.active || !window_cycler.cells)
		return;
	int32_t hit = window_cycler_pick_at(x, y);
	if (hit != window_cycler.hover) {
		window_cycler.hover = hit;
		window_cycler_refresh_highlight();
	}
}

/* Attach a "1".."9" badge at the upper-left corner of cell k. */
static void cycler_attach_badge(int32_t k) {
	if (!config.cycler_show_badges || k >= 9)
		return;
	struct wlr_box b = window_cycler.cells[k];
	int32_t badge_size = b.height / 8;
	if (badge_size < 22)
		badge_size = 22;
	if (badge_size > 56)
		badge_size = 56;
	char label[4];
	snprintf(label, sizeof(label), "%d", k + 1);
	struct wlr_buffer *bb = cycler_render_badge(label, badge_size);
	if (!bb)
		return;
	struct wlr_scene_buffer *bnode =
		wlr_scene_buffer_create(window_cycler.root, bb);
	if (!bnode) {
		wlr_buffer_drop(bb);
		return;
	}
	wlr_scene_buffer_set_dest_size(bnode, badge_size, badge_size);
	int32_t inset = badge_size / 4;
	wlr_scene_node_set_position(&bnode->node, b.x + inset, b.y + inset);
	window_cycler.badges[k] = bnode;
	window_cycler.badge_buffers[k] = bb;
}

/* Tear down the cycler overlay and restore every client to its
   pre-cycler geometry/state. After this returns, window_cycler is
   zeroed. */
static void window_cycler_destroy(void) {
	if (!window_cycler.active)
		return;
	for (int32_t i = 0; i < window_cycler.count; i++) {
		window_cycler_restore_client(i);
	}
	if (window_cycler.root)
		wlr_scene_node_destroy(&window_cycler.root->node);
	if (window_cycler.bg_tree)
		wlr_scene_node_destroy(&window_cycler.bg_tree->node);
	if (window_cycler.badge_buffers) {
		for (int32_t i = 0; i < window_cycler.count; i++) {
			if (window_cycler.badge_buffers[i])
				wlr_buffer_drop(window_cycler.badge_buffers[i]);
		}
	}
	free(window_cycler.clients);
	free(window_cycler.backup);
	free(window_cycler.cells);
	free(window_cycler.badges);
	free(window_cycler.badge_buffers);
	memset(&window_cycler, 0, sizeof(window_cycler));
	window_cycler.hover = -1;
	window_cycler.drag_idx = -1;
	/* If a tile reorder happened, request a fresh arrange so tile
	   layouts pick up the new client order. */
	if (selmon)
		arrange(selmon, false, false);
}

/* Focus the client at window_cycler.index, restore everyone, then
   destroy the overlay. The picked client is hoisted into LyrTop so
   it draws over any floating sibling; the next focusclient() call
   clears the promotion. */
static void window_cycler_commit(void) {
	if (!window_cycler.active)
		return;
	Client *target = NULL;
	if (window_cycler.index >= 0 && window_cycler.index < window_cycler.count)
		target = window_cycler.clients[window_cycler.index];
	window_cycler_destroy();
	if (target && client_surface(target)->mapped) {
		cycler_drop_raised_tile();
		focusclient(target, 1);
		if (target->scene && !target->isoverlay) {
			wlr_scene_node_reparent(&target->scene->node, layers[LyrTop]);
			wlr_scene_node_raise_to_top(&target->scene->node);
			cycler_raised_tile = target;
		}
		if (config.warpcursor)
			warp_cursor(target);
	}
}

/* Swap clients at indices a and b: swap their entries in
   window_cycler.clients[] AND in the global `clients` wl_list so
   the tile layout picks up the new order after restore. Backup
   entries swap too so each tile restores to its own original geom.
   Cells stay anchored to grid positions, so after swap cell a still
   shows whatever client now sits at index a. */
static void window_cycler_swap(int32_t a, int32_t b) {
	if (a == b || a < 0 || b < 0 || a >= window_cycler.count ||
		b >= window_cycler.count)
		return;
	Client *ca = window_cycler.clients[a];
	Client *cb = window_cycler.clients[b];
	if (!ca || !cb)
		return;
	/* Swap in the global linked list: pluck both, splice in the
	   opposite order at their respective former neighbors. */
	struct wl_list *prev_a = ca->link.prev;
	struct wl_list *prev_b = cb->link.prev;
	if (prev_a == &cb->link) {
		/* Adjacent: a sits right after b. Move b after a. */
		wl_list_remove(&cb->link);
		wl_list_insert(&ca->link, &cb->link);
	} else if (prev_b == &ca->link) {
		/* Adjacent the other way. */
		wl_list_remove(&ca->link);
		wl_list_insert(&cb->link, &ca->link);
	} else {
		wl_list_remove(&ca->link);
		wl_list_remove(&cb->link);
		wl_list_insert(prev_b, &ca->link);
		wl_list_insert(prev_a, &cb->link);
	}
	/* Swap our parallel arrays. */
	window_cycler.clients[a] = cb;
	window_cycler.clients[b] = ca;
	CyclerBackup tmp = window_cycler.backup[a];
	window_cycler.backup[a] = window_cycler.backup[b];
	window_cycler.backup[b] = tmp;
	/* Re-apply cells: each client snaps into its (possibly new)
	   cell. */
	window_cycler_apply_cell(a);
	window_cycler_apply_cell(b);
	/* index follows the originally-selected client. */
	if (window_cycler.index == a)
		window_cycler.index = b;
	else if (window_cycler.index == b)
		window_cycler.index = a;
	window_cycler_refresh_highlight();
}

/* Begin dragging the tile under (x, y). Called from buttonpress
   when the cycler is open. Returns true on a hit (caller swallows
   the click). */
static bool window_cycler_drag_begin(double x, double y) {
	int32_t hit = window_cycler_pick_at(x, y);
	if (hit < 0)
		return false;
	window_cycler.drag_idx = hit;
	window_cycler.drag_moved = false;
	struct wlr_box b = window_cycler.cells[hit];
	window_cycler.drag_grab_dx = x - b.x;
	window_cycler.drag_grab_dy = y - b.y;
	window_cycler.index = hit;
	window_cycler_refresh_highlight();
	return true;
}

/* Update drag position: move the dragged client geom to follow the
   cursor. */
static void window_cycler_drag_motion(double x, double y) {
	if (window_cycler.drag_idx < 0)
		return;
	Client *c = window_cycler.clients[window_cycler.drag_idx];
	if (!c)
		return;
	struct wlr_box b = window_cycler.cells[window_cycler.drag_idx];
	int32_t nx = (int32_t)(x - window_cycler.drag_grab_dx);
	int32_t ny = (int32_t)(y - window_cycler.drag_grab_dy);
	if (!window_cycler.drag_moved) {
		/* Threshold so a normal pick (left-click without dragging)
		   doesn't degrade into a no-op swap with itself. */
		int32_t ddx = nx - b.x;
		int32_t ddy = ny - b.y;
		if (ddx * ddx + ddy * ddy < 64)
			return;
		window_cycler.drag_moved = true;
		/* Hoist scene tree so the dragged window draws above the
		   overlay borders. */
		if (c->scene && !c->isoverlay) {
			wlr_scene_node_reparent(&c->scene->node, layers[LyrTop]);
			wlr_scene_node_raise_to_top(&c->scene->node);
		}
	}
	struct wlr_box live = {.x = nx, .y = ny,
						   .width = b.width, .height = b.height};
	resize(c, live, 0);
}

/* End drag. If released over a different cell, swap; otherwise snap
   back. Returns true if it consumed a release. */
static bool window_cycler_drag_end(double x, double y) {
	if (window_cycler.drag_idx < 0)
		return false;
	int32_t src = window_cycler.drag_idx;
	window_cycler.drag_idx = -1;
	Client *c = window_cycler.clients[src];
	/* Always drop the dragged client back into LyrTile / its natural
	   layer; commit() will hoist it again if picked. */
	if (c && c->scene && !c->isoverlay) {
		int32_t target_layer = LyrTile;
		if (c->isfloating || c->isfullscreen)
			target_layer = LyrTop;
		wlr_scene_node_reparent(&c->scene->node, layers[target_layer]);
	}
	if (!window_cycler.drag_moved) {
		/* Treat as a plain click pick. */
		window_cycler.index = src;
		window_cycler_commit();
		return true;
	}
	int32_t dst = window_cycler_pick_at(x, y);
	if (dst < 0 || dst == src) {
		/* Snap back into the source cell. */
		window_cycler_apply_cell(src);
	} else {
		window_cycler_swap(src, dst);
	}
	window_cycler.drag_moved = false;
	return true;
}

/* Collect the focusable visible clients on m, save their state,
   resize each into a grid cell, and build the overlay (dim
   backdrop, per-cell selection / hover borders, numbered
   badges). Returns the number of clients picked. */
static int32_t window_cycler_build(Monitor *m) {
	if (!m)
		return 0;

	Client *c = NULL;
	int32_t n = 0;
	wl_list_for_each(c, &clients, link) {
		if (window_cycler_eligible(c, m))
			n++;
	}
	if (n <= 1)
		return n;

	window_cycler.clients = ecalloc(n, sizeof(*window_cycler.clients));
	window_cycler.backup = ecalloc(n, sizeof(*window_cycler.backup));
	window_cycler.cells = ecalloc(n, sizeof(*window_cycler.cells));
	window_cycler.badges = ecalloc(n, sizeof(*window_cycler.badges));
	window_cycler.badge_buffers =
		ecalloc(n, sizeof(*window_cycler.badge_buffers));
	window_cycler.count = n;
	window_cycler.mon = m;
	window_cycler.hover = -1;
	window_cycler.drag_idx = -1;

	int32_t i = 0;
	int32_t current_idx = 0;
	wl_list_for_each(c, &clients, link) {
		if (!window_cycler_eligible(c, m))
			continue;
		window_cycler.clients[i] = c;
		window_cycler.backup[i] = (CyclerBackup){
			.geom = c->geom,
			.float_geom = c->float_geom,
			.bw = c->bw,
			.isfloating = c->isfloating,
			.isfullscreen = c->isfullscreen,
			.ismaximizescreen = c->ismaximizescreen,
			.iscustomsize = c->iscustomsize,
		};
		if (c == m->sel)
			current_idx = i;
		i++;
	}
	window_cycler.index = current_idx;

	window_cycler_compute_cells(m);

	/* LyrFadeOut: xytonode (src/fetch/common.h) explicitly skips
	   this layer, so pointer hit-tests can't deref our overlay
	   nodes. */
	window_cycler.root = wlr_scene_tree_create(layers[LyrFadeOut]);
	if (!window_cycler.root) {
		free(window_cycler.clients);
		free(window_cycler.backup);
		free(window_cycler.cells);
		free(window_cycler.badges);
		free(window_cycler.badge_buffers);
		memset(&window_cycler, 0, sizeof(window_cycler));
		return 0;
	}

	/* Full-monitor dim backdrop. Lives on LyrBottom so the real
	   windows draw on top of it; only the wallpaper / bottom layers
	   are dimmed. */
	window_cycler.bg_tree = wlr_scene_tree_create(layers[LyrBottom]);
	if (window_cycler.bg_tree) {
		float bg_color[4] = {0.02f, 0.02f, 0.04f, 0.88f};
		window_cycler.bg = wlr_scene_rect_create(
			window_cycler.bg_tree, m->m.width, m->m.height, bg_color);
		if (window_cycler.bg) {
			wlr_scene_node_set_position(&window_cycler.bg->node, m->m.x,
										m->m.y);
		}
		wlr_scene_node_raise_to_top(&window_cycler.bg_tree->node);
	}

	/* Single shared white outline frame, parked on the currently
	   selected cell. Filled rects covering the whole cell would
	   hide the real client window; thin outline strips draw the
	   frame and leave the window pixels visible. Hover and arrow
	   selection share this one frame, so only one cell ever has a
	   border at a time. */
	float white[4] = {1.0f, 1.0f, 1.0f, 0.95f};
	for (int32_t j = 0; j < 4; j++) {
		window_cycler.active_frame[j] =
			wlr_scene_rect_create(window_cycler.root, 1, 1, white);
	}
	for (int32_t k = 0; k < n; k++) {
		cycler_attach_badge(k);
	}
	window_cycler_refresh_highlight();

	wlr_scene_node_raise_to_top(&window_cycler.root->node);

	/* Resize the actual windows into their cells. */
	for (int32_t k = 0; k < n; k++) {
		window_cycler_apply_cell(k);
	}

	window_cycler.active = true;
	return n;
}

/* Move the selection one cell in the grid: (dx, dy) is the discrete
   step in column / row units. Clamps to the grid edges so an arrow
   key never wraps unexpectedly (Tab is the wrap path). */
static int32_t window_cycler_step_grid(int32_t dx, int32_t dy) {
	if (!window_cycler.active || window_cycler.count <= 0)
		return 0;
	int32_t cols = window_cycler.grid_cols > 0 ? window_cycler.grid_cols : 1;
	int32_t cur = window_cycler.index;
	int32_t cur_col = cur % cols;
	int32_t cur_row = cur / cols;
	int32_t new_col = cur_col + dx;
	int32_t new_row = cur_row + dy;
	if (new_col < 0)
		new_col = 0;
	if (new_row < 0)
		new_row = 0;
	int32_t cand = new_row * cols + new_col;
	if (cand >= window_cycler.count) {
		/* Last row may be partial: snap to the last valid index. */
		cand = window_cycler.count - 1;
	}
	if (cand == window_cycler.index)
		return 0;
	window_cycler.index = cand;
	window_cycler_refresh_highlight();
	return 1;
}

/* Pick the active target for keyboard-modifier shortcuts. Hovered
   cell wins when the cursor sits over the grid; otherwise the
   keyboard-selected index. */
static int32_t window_cycler_active_target(void) {
	if (window_cycler.hover >= 0 && window_cycler.hover < window_cycler.count)
		return window_cycler.hover;
	return window_cycler.index;
}

/* Move the client at cycler index `idx` to the workspace mask
   `tagmask`. The moved client is reverted to its pre-cycler state,
   retagged, then hidden on the scene tree so it disappears
   immediately from the current workspace -- without a full
   arrange() that would retile the remaining clients into the
   normal master/stack layout. The cycler arrays are compacted,
   the grid is recomputed for the new count, all remaining cells
   re-applied (so the surviving windows spring into the new bigger
   grid) and the badges are re-rendered with correct numbers (a
   compact alone would leave each badge displaying the old
   number). If the destination tag has no other visible clients,
   the moved window is floated + centered (so a lone window does
   not stretch fullscreen); otherwise it lands tiled and is
   retiled together with the existing residents on tag-switch.
   Caller passes the bit mask, not a digit. */
static void window_cycler_move_to_tag(int32_t idx, uint32_t tagmask) {
	if (idx < 0 || idx >= window_cycler.count || !(tagmask & TAGMASK))
		return;
	Client *target = window_cycler.clients[idx];
	if (!target || target->iskilling)
		return;

	/* Revert client state in place (do NOT call restore_client: that
	   would resize() the moved window back to its original geom on
	   the current workspace, animating it across the screen as it is
	   simultaneously retagged -- a visual flicker. Just restore the
	   logical state; the new tagset hides the scene node. */
	CyclerBackup *bk = &window_cycler.backup[idx];
	target->isfloating = bk->isfloating;
	target->isfullscreen = bk->isfullscreen;
	target->ismaximizescreen = bk->ismaximizescreen;
	target->iscustomsize = bk->iscustomsize;
	target->bw = bk->bw;
	target->geom = bk->geom;
	target->float_geom = bk->float_geom;
	target->ov_anim = false;
	if (bk->isfullscreen || bk->ismaximizescreen) {
		client_pending_fullscreen_state(target, bk->isfullscreen ? 1 : 0);
		client_pending_maximized_state(target, bk->ismaximizescreen ? 1 : 0);
	}
	target->tags = tagmask & TAGMASK;
	target->istagswitching = 1;

	/* Count other live clients that will share the destination tag on
	   the same monitor (skip the cycler entries -- they're still on
	   the source tag for one more frame). If the moved window is the
	   only inhabitant, float + center it so it does not stretch
	   fullscreen. Otherwise leave the pre-cycler tiled/floating state
	   alone and let arrange() retile alongside the existing
	   residents when the destination tag is viewed. */
	if (target->mon) {
		Client *other = NULL;
		int32_t roommates = 0;
		wl_list_for_each(other, &clients, link) {
			if (other == target || !other || other->iskilling ||
				other->isunglobal || client_is_unmanaged(other) ||
				client_is_x11_popup(other) || other->isminimized)
				continue;
			if (other->mon != target->mon)
				continue;
			if (!(other->tags & target->tags))
				continue;
			roommates++;
			break;
		}
		if (roommates == 0) {
			struct wlr_box centered = bk->float_geom;
			if (centered.width <= 0 || centered.height <= 0)
				centered = bk->geom;
			if (centered.width <= 0 || centered.height <= 0) {
				centered.width = (int32_t)(target->mon->w.width * 0.6);
				centered.height = (int32_t)(target->mon->w.height * 0.6);
			}
			centered = setclient_coordinate_center(target, target->mon,
												   centered, 0, 0);
			target->isfloating = 1;
			target->isfullscreen = 0;
			target->ismaximizescreen = 0;
			target->geom = centered;
			target->float_geom = centered;
		}
	}

	/* Hide the moved client right now so the empty cell does not
	   visibly remain on screen until the next arrange. arrange() at
	   cycler exit (or when the destination workspace is viewed)
	   will reattach to the correct layer. */
	if (target->scene)
		wlr_scene_node_set_enabled(&target->scene->node, false);

	/* Free badge node + buffer at idx -- the rest are re-rendered
	   below once the array is compacted, so their pixel content
	   matches their new index. */
	for (int32_t i = 0; i < window_cycler.count; i++) {
		if (window_cycler.badges[i]) {
			wlr_scene_node_destroy(&window_cycler.badges[i]->node);
			window_cycler.badges[i] = NULL;
		}
		if (window_cycler.badge_buffers[i]) {
			wlr_buffer_drop(window_cycler.badge_buffers[i]);
			window_cycler.badge_buffers[i] = NULL;
		}
	}

	/* Compact arrays: shift everything past idx one slot left. */
	for (int32_t i = idx; i < window_cycler.count - 1; i++) {
		window_cycler.clients[i] = window_cycler.clients[i + 1];
		window_cycler.backup[i] = window_cycler.backup[i + 1];
		window_cycler.cells[i] = window_cycler.cells[i + 1];
	}
	window_cycler.count--;
	if (window_cycler.index >= window_cycler.count)
		window_cycler.index = window_cycler.count - 1;
	if (window_cycler.index < 0)
		window_cycler.index = 0;
	if (window_cycler.hover >= window_cycler.count)
		window_cycler.hover = -1;

	if (window_cycler.count <= 1 || !window_cycler.mon) {
		/* Below two surviving windows the cycler has no point;
		   close it and let the destroy path arrange()ing the
		   compositor to its rest state. */
		window_cycler_destroy();
		return;
	}

	/* Recompute grid for the new count, re-apply cells (windows
	   spring into the new, larger thumbnails), re-render badges
	   with correct numbers, then refocus the highlight. */
	window_cycler_compute_cells(window_cycler.mon);
	for (int32_t i = 0; i < window_cycler.count; i++) {
		window_cycler_apply_cell(i);
		cycler_attach_badge(i);
	}
	window_cycler_refresh_highlight();
}

/* Append a freshly-mapped client to the running cycler. Called from
   mapnotify so that any window spawned while Alt+Tab is held (a
   shortcut launching the file manager, a notification daemon
   popping up, etc.) joins the grid in place rather than appearing
   *under* the dim backdrop. Grid is recomputed for the new count,
   every existing cell springs to its new geometry, and badges are
   re-rendered so their numbers match the new positions. Selection
   moves to the newcomer so releasing the cycler modifier focuses
   the just-spawned app. */
static void window_cycler_add_client(Client *c) {
	if (!window_cycler.active || !c || !window_cycler.mon)
		return;
	if (!window_cycler_eligible(c, window_cycler.mon))
		return;
	for (int32_t k = 0; k < window_cycler.count; k++) {
		if (window_cycler.clients[k] == c)
			return;
	}
	int32_t n = window_cycler.count + 1;
	Client **nc = realloc(window_cycler.clients, n * sizeof(*nc));
	if (!nc)
		return;
	window_cycler.clients = nc;
	CyclerBackup *nb = realloc(window_cycler.backup, n * sizeof(*nb));
	if (!nb)
		return;
	window_cycler.backup = nb;
	struct wlr_box *ncells = realloc(window_cycler.cells, n * sizeof(*ncells));
	if (!ncells)
		return;
	window_cycler.cells = ncells;
	struct wlr_scene_buffer **nbg =
		realloc(window_cycler.badges, n * sizeof(*nbg));
	if (!nbg)
		return;
	window_cycler.badges = nbg;
	struct wlr_buffer **nbb =
		realloc(window_cycler.badge_buffers, n * sizeof(*nbb));
	if (!nbb)
		return;
	window_cycler.badge_buffers = nbb;

	int32_t i = window_cycler.count;
	window_cycler.clients[i] = c;
	window_cycler.backup[i] = (CyclerBackup){
		.geom = c->geom,
		.float_geom = c->float_geom,
		.bw = c->bw,
		.isfloating = c->isfloating,
		.isfullscreen = c->isfullscreen,
		.ismaximizescreen = c->ismaximizescreen,
		.iscustomsize = c->iscustomsize,
	};
	window_cycler.cells[i] = (struct wlr_box){0};
	window_cycler.badges[i] = NULL;
	window_cycler.badge_buffers[i] = NULL;
	window_cycler.count = n;

	/* Tear down all badges (their dimensions are tuned to the
	   previous cell size and their labels still show the previous
	   indices); they're rebuilt immediately below against the new
	   grid. */
	for (int32_t k = 0; k < window_cycler.count; k++) {
		if (window_cycler.badges[k]) {
			wlr_scene_node_destroy(&window_cycler.badges[k]->node);
			window_cycler.badges[k] = NULL;
		}
		if (window_cycler.badge_buffers[k]) {
			wlr_buffer_drop(window_cycler.badge_buffers[k]);
			window_cycler.badge_buffers[k] = NULL;
		}
	}

	window_cycler_compute_cells(window_cycler.mon);
	for (int32_t k = 0; k < window_cycler.count; k++) {
		window_cycler_apply_cell(k);
		cycler_attach_badge(k);
	}
	window_cycler.index = window_cycler.count - 1;
	window_cycler_refresh_highlight();
}

/* Step the cycler selection by delta (positive = next, negative =
   prev), building the overlay on first call. */
static int32_t window_cycler_step(int32_t delta) {
	Monitor *m = selmon;
	if (!m)
		return 0;
	if (!window_cycler.active) {
		int32_t built = window_cycler_build(m);
		if (built <= 1) {
			window_cycler_destroy();
			return 0;
		}
	}
	if (window_cycler.count <= 0)
		return 0;
	int32_t idx = (window_cycler.index + delta) % window_cycler.count;
	if (idx < 0)
		idx += window_cycler.count;
	window_cycler.index = idx;
	window_cycler_refresh_highlight();
	return 1;
}
