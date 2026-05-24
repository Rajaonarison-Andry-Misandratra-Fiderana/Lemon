/* Alt+Tab window cycler.
   Builds a horizontal scene-tree overlay listing the focusable visible
   clients on the active monitor. Each entry is a colored rectangle with
   a snapshot of the client laid on top; the currently-selected entry
   gets a bright border. Selection commits on Alt release (driven from
   the keypress modifier path in lemon.c). */

static struct WindowCycler {
	bool active;
	int32_t index;
	int32_t count;
	Client **clients;
	struct wlr_scene_tree *root;
	struct wlr_scene_rect *bg;
	struct wlr_scene_rect **borders;
	struct wlr_scene_rect **tiles;
	Monitor *mon;
} window_cycler;

/* Highlight the entry at window_cycler.index by enabling its border node
   and disabling the others. */
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
	memset(&window_cycler, 0, sizeof(window_cycler));
}

/* Focus the client at window_cycler.index, then destroy the overlay. */
static void window_cycler_commit(void) {
	if (!window_cycler.active)
		return;
	Client *target = NULL;
	if (window_cycler.index >= 0 && window_cycler.index < window_cycler.count)
		target = window_cycler.clients[window_cycler.index];
	window_cycler_destroy();
	if (target && client_surface(target)->mapped) {
		focusclient(target, 1);
		if (config.warpcursor)
			warp_cursor(target);
	}
}

/* Collect the focusable visible clients on m into the cycler array and
   build the strip overlay (background panel + one bordered tile per
   client). Returns the number of clients picked. */
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

	const int32_t thumb_w = 180;
	const int32_t thumb_h = 120;
	const int32_t gap = 16;
	const int32_t pad = 20;
	const int32_t border = 4;
	int32_t total_w = n * thumb_w + (n - 1) * gap + 2 * pad;
	int32_t total_h = thumb_h + 2 * pad;
	int32_t origin_x = m->m.x + (m->m.width - total_w) / 2;
	int32_t origin_y = m->m.y + (m->m.height - total_h) / 2;

	window_cycler.root = wlr_scene_tree_create(layers[LyrOverlay]);
	if (!window_cycler.root) {
		free(window_cycler.clients);
		free(window_cycler.borders);
		free(window_cycler.tiles);
		memset(&window_cycler, 0, sizeof(window_cycler));
		return 0;
	}

	float bg_color[4] = {0.08f, 0.08f, 0.10f, 0.92f};
	window_cycler.bg = wlr_scene_rect_create(window_cycler.root, total_w,
											 total_h, bg_color);
	if (window_cycler.bg)
		wlr_scene_node_set_position(&window_cycler.bg->node, origin_x,
									origin_y);

	float tile_color[4] = {0.20f, 0.20f, 0.24f, 0.95f};
	for (int32_t k = 0; k < n; k++) {
		int32_t tx = origin_x + pad + k * (thumb_w + gap);
		int32_t ty = origin_y + pad;

		window_cycler.borders[k] = wlr_scene_rect_create(
			window_cycler.root, thumb_w + 2 * border, thumb_h + 2 * border,
			config.focuscolor);
		if (window_cycler.borders[k]) {
			wlr_scene_node_set_position(&window_cycler.borders[k]->node,
										tx - border, ty - border);
			wlr_scene_node_set_enabled(&window_cycler.borders[k]->node,
									   k == current_idx);
		}

		window_cycler.tiles[k] =
			wlr_scene_rect_create(window_cycler.root, thumb_w, thumb_h,
								  tile_color);
		if (window_cycler.tiles[k])
			wlr_scene_node_set_position(&window_cycler.tiles[k]->node, tx, ty);
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
