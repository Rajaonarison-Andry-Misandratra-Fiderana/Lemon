/* Alt+Tab window cycler.
   Builds a scene-tree overlay listing the focusable visible clients on
   the active monitor: each entry is a real snapshot of the client's
   scene subtree, recursively scaled down so the strip stays roughly
   the size of the overview panel. The currently-selected entry is
   wrapped in a bright border. Selection commits on Alt release
   (driven from the keypress modifier path in lemon.c). */

static struct WindowCycler {
	bool active;
	int32_t index;
	int32_t count;
	Client **clients;
	struct wlr_scene_tree *root;
	struct wlr_scene_rect *bg;
	struct wlr_scene_rect **borders;
	struct wlr_scene_rect **tiles;
	struct wlr_scene_tree **thumbs;
	Monitor *mon;
} window_cycler;

/* Last tiled client the cycler promoted to LyrTop so it draws above
   floating siblings. Reset back to LyrTile by focusclient as soon as
   the user focuses something else. */
static Client *cycler_raised_tile = NULL;

/* Drop the cycler's promoted client back into its native layer. Mirror
   the layer-routing used by setmaximizescreen / setfullscreen /
   setfloating: overlay -> LyrOverlay, floating or fullscreen -> LyrTop,
   plain tiled / maximized -> LyrTile. Safe to call when nothing is
   raised. */
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

/* Read the "logical" display width/height of a snapshot scene buffer.
   Prefer the explicit dst_width/dst_height (already in logical
   coords, set by scenefx for scaled surfaces). Fall back to the
   underlying wlr_buffer's natural dimensions divided by output_scale
   so HiDPI buffers (whose natural size is in pixels, e.g. 1920px for
   a 960-wide logical client) don't end up doubled. */
static void window_cycler_buffer_size(struct wlr_scene_buffer *buf,
									  double output_scale, int32_t *out_w,
									  int32_t *out_h) {
	int32_t w = buf->dst_width;
	int32_t h = buf->dst_height;
	if ((w == 0 || h == 0) && buf->buffer) {
		double s = output_scale > 0 ? output_scale : 1.0;
		if (w == 0)
			w = (int32_t)(buf->buffer->width / s);
		if (h == 0)
			h = (int32_t)(buf->buffer->height / s);
	}
	*out_w = w;
	*out_h = h;
}

/* Recursively scale a snapshot subtree by (sx, sy): each node's
   position and any sized payload (buffer dst, rect/shadow size) is
   multiplied by the scale factor so the rendered subtree shrinks in
   place. Scale is applied once at every level so the tree's natural
   relative layout is preserved. */
static void window_cycler_scale_node(struct wlr_scene_node *node, double sx,
									 double sy, double output_scale) {
	if (!node)
		return;

	int32_t nx = (int32_t)(node->x * sx);
	int32_t ny = (int32_t)(node->y * sy);
	wlr_scene_node_set_position(node, nx, ny);

	switch (node->type) {
	case WLR_SCENE_NODE_BUFFER: {
		struct wlr_scene_buffer *buf = wlr_scene_buffer_from_node(node);
		int32_t w = 0, h = 0;
		window_cycler_buffer_size(buf, output_scale, &w, &h);
		if (w > 0 && h > 0)
			wlr_scene_buffer_set_dest_size(buf, (int32_t)(w * sx),
										   (int32_t)(h * sy));
		break;
	}
	case WLR_SCENE_NODE_RECT: {
		struct wlr_scene_rect *rect =
			wl_container_of(node, rect, node);
		wlr_scene_rect_set_size(rect, (int32_t)(rect->width * sx),
								(int32_t)(rect->height * sy));
		break;
	}
	case WLR_SCENE_NODE_SHADOW: {
		struct wlr_scene_shadow *shadow =
			wl_container_of(node, shadow, node);
		wlr_scene_shadow_set_size(shadow, (int32_t)(shadow->width * sx),
								  (int32_t)(shadow->height * sy));
		break;
	}
	case WLR_SCENE_NODE_TREE: {
		struct wlr_scene_tree *tree =
			wl_container_of(node, tree, node);
		struct wlr_scene_node *child = NULL;
		wl_list_for_each(child, &tree->children, link) {
			window_cycler_scale_node(child, sx, sy, output_scale);
		}
		break;
	}
	default:
		break;
	}
}

/* Highlight the entry at window_cycler.index by enabling its border
   node and disabling the others. */
static void window_cycler_refresh_highlight(void) {
	int32_t i;
	for (i = 0; i < window_cycler.count; i++) {
		if (window_cycler.borders[i]) {
			wlr_scene_node_set_enabled(&window_cycler.borders[i]->node,
									   i == window_cycler.index);
		}
	}
}

/* Tear down the cycler overlay and reset the global state. */
static void window_cycler_destroy(void) {
	if (!window_cycler.active)
		return;
	if (window_cycler.root)
		wlr_scene_node_destroy(&window_cycler.root->node);
	free(window_cycler.clients);
	free(window_cycler.borders);
	free(window_cycler.tiles);
	free(window_cycler.thumbs);
	memset(&window_cycler, 0, sizeof(window_cycler));
}

/* Focus the client at window_cycler.index, then destroy the overlay.
   If the picked client is tiled, hoist its scene subtree into LyrTop
   so it visibly draws on top of any floating sibling -- selected
   window always lands in the foreground regardless of tile/float
   state. The next focusclient() call clears the promotion. */
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
		/* Always hoist into LyrTop so the picked window draws over any
		   floating / maximized / fullscreen sibling. The next focus
		   change snaps it back via cycler_drop_raised_tile(). Overlay
		   clients keep their LyrOverlay parent (they already draw on
		   top of everything). */
		if (target->scene && !target->isoverlay) {
			wlr_scene_node_reparent(&target->scene->node, layers[LyrTop]);
			wlr_scene_node_raise_to_top(&target->scene->node);
			cycler_raised_tile = target;
		}
		if (config.warpcursor)
			warp_cursor(target);
	}
}

/* Predicate matching the clients eligible for the cycler list. */
static inline bool window_cycler_eligible(Client *c, Monitor *m) {
	return c && c->mon == m && !c->iskilling && !c->isunglobal &&
		   !client_is_unmanaged(c) && !client_is_x11_popup(c) &&
		   !c->isminimized && VISIBLEON(c, m);
}

/* Collect the focusable visible clients on m into the cycler array and
   build a grid overlay (background panel + one snapshot thumbnail per
   client, with an outer rectangle drawn as the highlight border). Up
   to 3 thumbs per row; extra clients wrap to the next row, and the
   final partial row is centered. Returns the number of clients
   picked. */
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
	window_cycler.borders = ecalloc(n, sizeof(*window_cycler.borders));
	window_cycler.tiles = ecalloc(n, sizeof(*window_cycler.tiles));
	window_cycler.thumbs = ecalloc(n, sizeof(*window_cycler.thumbs));
	window_cycler.count = n;
	window_cycler.mon = m;

	int32_t i = 0;
	int32_t current_idx = 0;
	wl_list_for_each(c, &clients, link) {
		if (!window_cycler_eligible(c, m))
			continue;
		window_cycler.clients[i] = c;
		if (c == m->sel)
			current_idx = i;
		i++;
	}
	window_cycler.index = current_idx;

	/* Grid layout: up to 3 thumbnails per row, wrap to next row when
	   n > 3. Thumb size starts at ~32% of the monitor's shortest
	   dimension (16:10 aspect) and shrinks uniformly if the full grid
	   would overflow either screen axis. */
	const int32_t per_row = 3;
	const int32_t cols = n < per_row ? n : per_row;
	const int32_t rows = (n + per_row - 1) / per_row;
	const int32_t gap = 24;
	const int32_t pad = 32;
	const int32_t border = 6;
	const int32_t screen_pad = 80; /* safety margin off the monitor edges */

	int32_t short_side = m->m.width < m->m.height ? m->m.width : m->m.height;
	int32_t thumb_h = (int32_t)(short_side * 0.32);
	int32_t thumb_w = (int32_t)(thumb_h * 16.0 / 10.0);

	int32_t inner_w_budget = m->m.width - screen_pad - 2 * pad -
							 (cols - 1) * gap;
	int32_t inner_h_budget = m->m.height - screen_pad - 2 * pad -
							 (rows - 1) * gap;
	if (inner_w_budget < cols)
		inner_w_budget = cols;
	if (inner_h_budget < rows)
		inner_h_budget = rows;
	double shrink_w = (double)inner_w_budget / (cols * thumb_w);
	double shrink_h = (double)inner_h_budget / (rows * thumb_h);
	double shrink = shrink_w < shrink_h ? shrink_w : shrink_h;
	if (shrink < 1.0) {
		if (shrink < 0.15)
			shrink = 0.15;
		thumb_w = (int32_t)(thumb_w * shrink);
		thumb_h = (int32_t)(thumb_h * shrink);
	}

	int32_t total_w = cols * thumb_w + (cols - 1) * gap + 2 * pad;
	int32_t total_h = rows * thumb_h + (rows - 1) * gap + 2 * pad;
	int32_t origin_x = m->m.x + (m->m.width - total_w) / 2;
	int32_t origin_y = m->m.y + (m->m.height - total_h) / 2;

	/* LyrFadeOut: xytonode (src/fetch/common.h) explicitly skips this
	   layer, so pointer hit-tests can't deref our snapshot scene_buffer
	   nodes (they are not wlr_scene_surface backed -> NULL->surface
	   crash in wlr_scene_surface_try_from_buffer). */
	window_cycler.root = wlr_scene_tree_create(layers[LyrFadeOut]);
	if (!window_cycler.root) {
		free(window_cycler.clients);
		free(window_cycler.borders);
		free(window_cycler.tiles);
		free(window_cycler.thumbs);
		memset(&window_cycler, 0, sizeof(window_cycler));
		return 0;
	}

	float bg_color[4] = {0.06f, 0.06f, 0.08f, 0.92f};
	window_cycler.bg = wlr_scene_rect_create(window_cycler.root, total_w,
											 total_h, bg_color);
	if (window_cycler.bg)
		wlr_scene_node_set_position(&window_cycler.bg->node, origin_x,
									origin_y);

	float tile_color[4] = {0.14f, 0.14f, 0.18f, 0.95f};
	for (int32_t k = 0; k < n; k++) {
		Client *cl = window_cycler.clients[k];
		if (!cl || !cl->scene)
			continue;
		int32_t row = k / per_row;
		int32_t col = k % per_row;
		/* Center the final (possibly partial) row of thumbs. */
		int32_t row_count = (row == rows - 1 && n % per_row)
								? (n % per_row)
								: cols;
		int32_t row_w = row_count * thumb_w + (row_count - 1) * gap;
		int32_t row_x = origin_x + pad + (cols * thumb_w + (cols - 1) * gap -
										   row_w) / 2;
		int32_t tx = row_x + col * (thumb_w + gap);
		int32_t ty = origin_y + pad + row * (thumb_h + gap);

		/* Outer highlight border (one per tile, only the current one is
		   enabled). Drawn first so the thumbnail layers on top of it. */
		window_cycler.borders[k] = wlr_scene_rect_create(
			window_cycler.root, thumb_w + 2 * border, thumb_h + 2 * border,
			config.focuscolor);
		if (window_cycler.borders[k]) {
			wlr_scene_node_set_position(&window_cycler.borders[k]->node,
										tx - border, ty - border);
			wlr_scene_node_set_enabled(&window_cycler.borders[k]->node,
									   k == current_idx);
		}

		/* Dark backdrop tile under the snapshot so transparent regions of
		   the client still read against the strip. */
		window_cycler.tiles[k] =
			wlr_scene_rect_create(window_cycler.root, thumb_w, thumb_h,
								  tile_color);
		if (window_cycler.tiles[k])
			wlr_scene_node_set_position(&window_cycler.tiles[k]->node, tx,
										ty);

		/* Snapshot the live client subtree. scene_node_snapshot()
		   flattens the source into a single layer of buffer children
		   whose positions start at c->scene->node.x/y (the actual
		   on-screen position, possibly animated). Use the client's
		   intended geom dimensions for the scale factor -- a
		   bounding-box pass over snapshot children was unreliable
		   because:
		     - single-pixel-buffer placeholders report tiny natural
		       sizes (1x1) but live inside a full-size client; their
		       bbox dragged the scale to 1, blowing every other buffer
		       up out of the tile;
		     - popups / sub-surfaces (decorations, tooltips) that
		       briefly stick outside the client extend the bbox the
		       other way, pushing the real surface contents off
		       center;
		     - some clients leave dst_width == 0 with a transformed
		       wlr_buffer whose natural width is the wrong axis.

		   Stable approach: subtract the snapshot's origin
		   (c->scene->node.x/y), scale by min(thumb_w / geom_w,
		   thumb_h / geom_h), then disable every snapshot child that
		   is fully outside [0, geom.w] x [0, geom.h] so stray popup
		   buffers can't render past the tile border. */
		struct wlr_scene_tree *snap =
			wlr_scene_tree_snapshot(&cl->scene->node, window_cycler.root);
		if (!snap)
			continue;
		window_cycler.thumbs[k] = snap;

		int32_t cw = cl->geom.width > 0 ? cl->geom.width : 1;
		int32_t ch = cl->geom.height > 0 ? cl->geom.height : 1;
		double sx = (double)thumb_w / cw;
		double sy = (double)thumb_h / ch;
		double s = sx < sy ? sx : sy;
		int32_t ox = cl->scene->node.x;
		int32_t oy = cl->scene->node.y;
		double output_scale =
			(cl->mon && cl->mon->wlr_output)
				? cl->mon->wlr_output->scale
				: 1.0;

		bool any_visible = false;
		struct wlr_scene_node *child = NULL;
		wl_list_for_each(child, &snap->children, link) {
			int32_t local_x = child->x - ox;
			int32_t local_y = child->y - oy;

			int32_t bw = 0, bh = 0;
			if (child->type == WLR_SCENE_NODE_BUFFER) {
				struct wlr_scene_buffer *buf =
					wlr_scene_buffer_from_node(child);
				window_cycler_buffer_size(buf, output_scale, &bw, &bh);
			}

			/* Tolerant inside check: keep any child whose buffer
			   reaches into the client geom, even if it sticks out a
			   bit. Strict "fully inside" rejected too many real
			   client surfaces -- HiDPI clients with mismatched dst
			   sizes, or sub-surfaces just past the border. */
			bool inside = (bw > 0 && bh > 0 &&
						   local_x + bw > 0 && local_y + bh > 0 &&
						   local_x < 2 * cw && local_y < 2 * ch);
			if (!inside) {
				wlr_scene_node_set_enabled(child, false);
				continue;
			}
			any_visible = true;
			wlr_scene_node_set_position(child, local_x, local_y);
			window_cycler_scale_node(child, s, s, output_scale);
		}

		if (!any_visible) {
			/* Client has no buffer overlapping its own geom -- e.g.
			   it just mapped, or it's a single-pixel placeholder.
			   Drop the snapshot, keep the dark backdrop tile. */
			wlr_scene_node_destroy(&snap->node);
			window_cycler.thumbs[k] = NULL;
			continue;
		}

		/* Center the scaled snapshot inside its tile. Preserving the
		   aspect ratio leaves slack along the shorter axis. */
		int32_t draw_w = (int32_t)(cw * s);
		int32_t draw_h = (int32_t)(ch * s);
		int32_t offx = tx + (thumb_w - draw_w) / 2;
		int32_t offy = ty + (thumb_h - draw_h) / 2;
		wlr_scene_node_set_position(&snap->node, offx, offy);
	}

	wlr_scene_node_raise_to_top(&window_cycler.root->node);
	window_cycler.active = true;
	return n;
}

/* Step the cycler selection by delta (positive = next, negative = prev),
   building the overlay on first call. arg->i not used. */
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
