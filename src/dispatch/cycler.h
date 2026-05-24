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

/* Recursively scale a snapshot subtree by (sx, sy): each node's
   position and any sized payload (buffer dst, rect/shadow size) is
   multiplied by the scale factor so the rendered subtree shrinks in
   place. Scale is applied once at every level so the tree's natural
   relative layout is preserved. */
static void window_cycler_scale_node(struct wlr_scene_node *node, double sx,
									 double sy) {
	if (!node)
		return;

	int32_t nx = (int32_t)(node->x * sx);
	int32_t ny = (int32_t)(node->y * sy);
	wlr_scene_node_set_position(node, nx, ny);

	switch (node->type) {
	case WLR_SCENE_NODE_BUFFER: {
		struct wlr_scene_buffer *buf = wlr_scene_buffer_from_node(node);
		int32_t w = buf->dst_width;
		int32_t h = buf->dst_height;
		if ((w == 0 || h == 0) && buf->buffer) {
			if (w == 0)
				w = buf->buffer->width;
			if (h == 0)
				h = buf->buffer->height;
		}
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
			window_cycler_scale_node(child, sx, sy);
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

/* Collect the focusable visible clients on m into the cycler array and
   build a strip overlay (background panel + one snapshot thumbnail per
   client, with an outer rectangle drawn as the highlight border).
   Returns the number of clients picked. */
static int32_t window_cycler_build(Monitor *m) {
	if (!m)
		return 0;

	Client *c = NULL;
	int32_t n = 0;
	wl_list_for_each(c, &clients, link) {
		if (!c || c->mon != m || c->iskilling || c->isunglobal ||
			client_is_unmanaged(c) || client_is_x11_popup(c) ||
			c->isminimized || !VISIBLEON(c, m))
			continue;
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
		if (!c || c->mon != m || c->iskilling || c->isunglobal ||
			client_is_unmanaged(c) || client_is_x11_popup(c) ||
			c->isminimized || !VISIBLEON(c, m))
			continue;
		window_cycler.clients[i] = c;
		if (c == m->sel)
			current_idx = i;
		i++;
	}
	window_cycler.index = current_idx;

	/* Aim for overview-sized thumbs: ~28% of the monitor's shortest
	   dimension per tile, capped so a very wide monitor doesn't blow
	   the strip past the screen edges. Keep a 16:10 aspect ratio. */
	int32_t short_side = m->m.width < m->m.height ? m->m.width : m->m.height;
	int32_t thumb_h = (int32_t)(short_side * 0.32);
	int32_t thumb_w = (int32_t)(thumb_h * 16.0 / 10.0);
	const int32_t gap = 24;
	const int32_t pad = 32;
	const int32_t border = 6;

	/* Shrink uniformly if the row would overflow the monitor width. */
	int32_t total_w = n * thumb_w + (n - 1) * gap + 2 * pad;
	int32_t max_w = m->m.width - 80;
	if (total_w > max_w) {
		double shrink = (double)(max_w - (n - 1) * gap - 2 * pad) /
						(n * thumb_w);
		if (shrink < 0.2)
			shrink = 0.2;
		thumb_w = (int32_t)(thumb_w * shrink);
		thumb_h = (int32_t)(thumb_h * shrink);
		total_w = n * thumb_w + (n - 1) * gap + 2 * pad;
	}
	int32_t total_h = thumb_h + 2 * pad;
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
		int32_t tx = origin_x + pad + k * (thumb_w + gap);
		int32_t ty = origin_y + pad;

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

		/* Snapshot the live client subtree. scene_node_snapshot() flattens
		   the source into a single layer of buffer children whose
		   positions are the source's accumulated monitor-space
		   coordinates (starts at lx=0 then adds c->scene->node.x, which
		   may be the *animated* position rather than c->geom.x).
		   Compute a bounding box over the snapshot's children instead
		   of trusting cl->geom -- handles mid-animation, scroller
		   off-screen positions, maximized, fullscreen, etc. */
		struct wlr_scene_tree *snap =
			wlr_scene_tree_snapshot(&cl->scene->node, window_cycler.root);
		if (!snap)
			continue;
		window_cycler.thumbs[k] = snap;

		int32_t min_x = INT32_MAX, min_y = INT32_MAX;
		int32_t max_x = INT32_MIN, max_y = INT32_MIN;
		struct wlr_scene_node *child = NULL;
		wl_list_for_each(child, &snap->children, link) {
			if (child->type != WLR_SCENE_NODE_BUFFER)
				continue;
			struct wlr_scene_buffer *buf = wlr_scene_buffer_from_node(child);
			int32_t bw = buf->dst_width;
			int32_t bh = buf->dst_height;
			if ((bw == 0 || bh == 0) && buf->buffer) {
				if (bw == 0)
					bw = buf->buffer->width;
				if (bh == 0)
					bh = buf->buffer->height;
			}
			if (bw <= 0 || bh <= 0)
				continue;
			if (child->x < min_x)
				min_x = child->x;
			if (child->y < min_y)
				min_y = child->y;
			if (child->x + bw > max_x)
				max_x = child->x + bw;
			if (child->y + bh > max_y)
				max_y = child->y + bh;
		}

		int32_t bbox_w = max_x - min_x;
		int32_t bbox_h = max_y - min_y;
		if (bbox_w <= 0 || bbox_h <= 0) {
			/* No usable buffer in this snapshot -- happens for clients
			   that just unmapped or have not produced a frame yet.
			   Leave the dark backdrop tile in place and move on. */
			wlr_scene_node_destroy(&snap->node);
			window_cycler.thumbs[k] = NULL;
			continue;
		}

		double sx = (double)thumb_w / bbox_w;
		double sy = (double)thumb_h / bbox_h;
		double s = sx < sy ? sx : sy;

		wl_list_for_each(child, &snap->children, link) {
			wlr_scene_node_set_position(child, child->x - min_x,
										child->y - min_y);
			window_cycler_scale_node(child, s, s);
		}

		/* Center the scaled snapshot inside its tile. The preserved
		   aspect ratio leaves slack along the shorter axis. */
		int32_t draw_w = (int32_t)(bbox_w * s);
		int32_t draw_h = (int32_t)(bbox_h * s);
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
