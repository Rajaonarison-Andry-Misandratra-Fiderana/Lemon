#pragma once

/* Layer-shell positioning + floating-window snap + exclusive layer
   focus reset. arrangelayer / arrangelayers run the exclusive-zone
   shrink pass and the non-exclusive positioning pass; apply_window_snap
   pulls a floating client's edges to neighbours and monitor bounds;
   focuslayer / reset_exclusive_layers_focus manage keyboard focus
   exchange between clients and overlay layer surfaces. Single-TU
   header included once. */

/* Configure each layer surface in list and shrink usable_area when exclusive zones apply. */
void arrangelayer(Monitor *m, struct wl_list *list, struct wlr_box *usable_area,
				  int32_t exclusive) {
	LayerSurface *l = NULL;
	struct wlr_box full_area = m->m;

	wl_list_for_each(l, list, link) {
		struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;

		if (exclusive != (layer_surface->current.exclusive_zone > 0) ||
			!layer_surface->initialized)
			continue;

		if (l->being_unmapped)
			continue;

		wlr_scene_layer_surface_v1_configure(l->scene_layer, &full_area,
											 usable_area);
		wlr_scene_node_set_position(&l->popups->node, l->scene->node.x,
									l->scene->node.y);
	}
}

/* Snap a floating client's edges to nearby windows, monitor edges, or usable-area boundaries. */
void apply_window_snap(Client *c) {
	int32_t snap_up = 99999, snap_down = 99999, snap_left = 99999,
			snap_right = 99999;
	int32_t snap_up_temp = 0, snap_down_temp = 0, snap_left_temp = 0,
			snap_right_temp = 0;
	int32_t snap_up_screen = 0, snap_down_screen = 0, snap_left_screen = 0,
			snap_right_screen = 0;
	int32_t snap_up_mon = 0, snap_down_mon = 0, snap_left_mon = 0,
			snap_right_mon = 0;

	uint32_t cbw = !render_border || c->fake_no_border ? config.borderpx : 0;
	uint32_t tcbw;
	uint32_t cx, cy, cw, ch, tcx, tcy, tcw, tch;
	cx = c->geom.x + cbw;
	cy = c->geom.y + cbw;
	cw = c->geom.width - 2 * cbw;
	ch = c->geom.height - 2 * cbw;

	Client *tc = NULL;
	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	if (!c->isfloating || !config.enable_floating_snap)
		return;

	wl_list_for_each(tc, &clients, link) {
		if (tc && tc->isfloating && !tc->iskilling &&
			client_surface(tc)->mapped && VISIBLEON(tc, c->mon)) {

			tcbw = !render_border || tc->fake_no_border ? config.borderpx : 0;
			tcx = tc->geom.x + tcbw;
			tcy = tc->geom.y + tcbw;
			tcw = tc->geom.width - 2 * tcbw;
			tch = tc->geom.height - 2 * tcbw;

			snap_left_temp = cx - tcx - tcw;
			snap_right_temp = tcx - cx - cw;
			snap_up_temp = cy - tcy - tch;
			snap_down_temp = tcy - cy - ch;

			if (snap_left_temp < snap_left && snap_left_temp >= 0) {
				snap_left = snap_left_temp;
			}
			if (snap_right_temp < snap_right && snap_right_temp >= 0) {
				snap_right = snap_right_temp;
			}
			if (snap_up_temp < snap_up && snap_up_temp >= 0) {
				snap_up = snap_up_temp;
			}
			if (snap_down_temp < snap_down && snap_down_temp >= 0) {
				snap_down = snap_down_temp;
			}
		}
	}

	snap_left_mon = cx - c->mon->m.x;
	snap_right_mon = c->mon->m.x + c->mon->m.width - cx - cw;
	snap_up_mon = cy - c->mon->m.y;
	snap_down_mon = c->mon->m.y + c->mon->m.height - cy - ch;

	if (snap_up_mon >= 0 && snap_up_mon < snap_up)
		snap_up = snap_up_mon;
	if (snap_down_mon >= 0 && snap_down_mon < snap_down)
		snap_down = snap_down_mon;
	if (snap_left_mon >= 0 && snap_left_mon < snap_left)
		snap_left = snap_left_mon;
	if (snap_right_mon >= 0 && snap_right_mon < snap_right)
		snap_right = snap_right_mon;

	snap_left_screen = cx - c->mon->w.x;
	snap_right_screen = c->mon->w.x + c->mon->w.width - cx - cw;
	snap_up_screen = cy - c->mon->w.y;
	snap_down_screen = c->mon->w.y + c->mon->w.height - cy - ch;

	if (snap_up_screen >= 0 && snap_up_screen < snap_up)
		snap_up = snap_up_screen;
	if (snap_down_screen >= 0 && snap_down_screen < snap_down)
		snap_down = snap_down_screen;
	if (snap_left_screen >= 0 && snap_left_screen < snap_left)
		snap_left = snap_left_screen;
	if (snap_right_screen >= 0 && snap_right_screen < snap_right)
		snap_right = snap_right_screen;

	if (snap_left < snap_right && snap_left < config.snap_distance) {
		c->geom.x = c->geom.x - snap_left;
	}

	if (snap_right <= snap_left && snap_right < config.snap_distance) {
		c->geom.x = c->geom.x + snap_right;
	}

	if (snap_up < snap_down && snap_up < config.snap_distance) {
		c->geom.y = c->geom.y - snap_up;
	}

	if (snap_down <= snap_up && snap_down < config.snap_distance) {
		c->geom.y = c->geom.y + snap_down;
	}

	c->float_geom = c->geom;
	resize(c, c->geom, 0);
}

/* Give keyboard focus to a layer surface and notify the input-method relay. */
void focuslayer(LayerSurface *l) {
	focusclient(NULL, 0);
	dwl_im_relay_set_focus(dwl_input_method_relay, l->layer_surface->surface);
	client_notify_enter(l->layer_surface->surface, wlr_seat_get_keyboard(seat));
}

/* Re-evaluate exclusive keyboard focus on layer surfaces and move focus back to a client if needed. */
void reset_exclusive_layers_focus(Monitor *m) {
	LayerSurface *l = NULL;
	int32_t i;
	bool neet_change_focus_to_client = false;
	uint32_t layers_above_shell[] = {
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
		ZWLR_LAYER_SHELL_V1_LAYER_TOP,
		ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM,
	};

	if (!m)
		return;

	for (i = 0; i < (int32_t)LENGTH(layers_above_shell); i++) {
		wl_list_for_each(l, &m->layers[layers_above_shell[i]], link) {
			if (l == exclusive_focus &&
				l->layer_surface->current.keyboard_interactive !=
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE) {

				exclusive_focus = NULL;

				neet_change_focus_to_client = true;
			}

			if (l->layer_surface->surface ==
					seat->keyboard_state.focused_surface &&
				l->being_unmapped) {
				neet_change_focus_to_client = true;
			}

			if (l->layer_surface->current.keyboard_interactive ==
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE &&
				l->layer_surface->surface ==
					seat->keyboard_state.focused_surface) {
				neet_change_focus_to_client = true;
			}

			if (locked ||
				l->layer_surface->current.keyboard_interactive !=
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE ||
				l->being_unmapped)
				continue;

			exclusive_focus = l;
			neet_change_focus_to_client = false;
			if (l->layer_surface->surface !=
				seat->keyboard_state.focused_surface)
				focuslayer(l);
			return;
		}
	}

	if (neet_change_focus_to_client) {
		focusclient(focustop(selmon), 1);
	}
}

/* Position all layer surfaces on monitor m and re-arrange tiles if usable area shrank. */
void arrangelayers(Monitor *m) {
	int32_t i;
	struct wlr_box usable_area = m->m;

	if (!m->wlr_output->enabled)
		return;

	if (m->iscleanuping)
		return;

	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 1);

	if (!wlr_box_equal(&usable_area, &m->w)) {
		m->w = usable_area;
		arrange(m, false, false);
	}

	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 0);
}
