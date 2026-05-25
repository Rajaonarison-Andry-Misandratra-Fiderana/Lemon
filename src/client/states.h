#pragma once

/* Client maximize / fullscreen / fake-fullscreen state mutators.
   setmaximizescreen and setfullscreen both: clear the opposite state,
   exit scroller stacks, reparent the scene tree to LyrTop/LyrTile/
   LyrOverlay, save/restore size-per, and re-arrange the monitor.
   Single-TU header included once. */

void setmaximizescreen(Client *c, int32_t maximizescreen) {
	struct wlr_box maximizescreen_box;
	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	if (c->mon->isoverview)
		return;

	int32_t old_maximizescreen_state = c->ismaximizescreen;
	client_pending_maximized_state(c, maximizescreen);

	if (maximizescreen) {

		if (c->isfullscreen) {
			client_pending_fullscreen_state(c, 0);
			client_set_fullscreen(c, 0);
		}

		exit_scroller_stack(c);

		maximizescreen_box.x = c->mon->w.x + config.gappoh;
		maximizescreen_box.y = c->mon->w.y + config.gappov;
		maximizescreen_box.width = c->mon->w.width - 2 * config.gappoh;
		maximizescreen_box.height = c->mon->w.height - 2 * config.gappov;
		wlr_scene_node_raise_to_top(&c->scene->node);
		if (!is_scroller_layout(c->mon) || c->isfloating)
			resize(c, maximizescreen_box, 0);
	} else {
		c->bw = c->isnoborder ? 0 : config.borderpx;
		if (c->isfloating)
			setfloating(c, 1);
	}

	wlr_scene_node_reparent(&c->scene->node,
							layers[c->isfloating ? LyrTop : LyrTile]);
	if (!c->ismaximizescreen && old_maximizescreen_state) {
		restore_size_per(c->mon, c);
	}

	if (c->ismaximizescreen && !old_maximizescreen_state) {
		save_old_size_per(c->mon);
	}

	if (!c->force_fakemaximize && !c->ismaximizescreen) {
		client_set_maximized(c, false);
	} else if (!c->force_fakemaximize && c->ismaximizescreen) {
		client_set_maximized(c, true);
	}

	arrange(c->mon, false, false);
}

void setfakefullscreen(Client *c, int32_t fakefullscreen) {
	c->isfakefullscreen = fakefullscreen;
	if (!c->mon)
		return;

	if (c->isfullscreen)
		setfullscreen(c, 0);

	client_set_fullscreen(c, fakefullscreen);
}

void setfullscreen(Client *c, int32_t fullscreen)
{

	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	if (c->mon->isoverview)
		return;

	int32_t old_fullscreen_state = c->isfullscreen;
	c->isfullscreen = fullscreen;
	if (old_fullscreen_state != fullscreen) {
		c->focus_opacity_dirty = true;
		if (c->scene_surface)
			wlr_scene_node_for_each_buffer(&c->scene_surface->node,
										   iter_xdg_scene_buffers, c);
	}

	client_set_fullscreen(c, fullscreen);
	client_pending_fullscreen_state(c, fullscreen);

	if (fullscreen) {

		if (c->ismaximizescreen && !c->force_fakemaximize) {
			client_set_maximized(c, false);
		}

		client_pending_maximized_state(c, 0);

		exit_scroller_stack(c);
		c->isfakefullscreen = 0;

		c->bw = 0;
		wlr_scene_node_raise_to_top(&c->scene->node);
		if (!is_scroller_layout(c->mon) || c->isfloating)
			resize(c, c->mon->m, 1);
	} else {
		c->bw = c->isnoborder ? 0 : config.borderpx;
		if (c->isfloating)
			setfloating(c, 1);
	}

	if (c->isoverlay) {
		wlr_scene_node_reparent(&c->scene->node, layers[LyrOverlay]);
	} else if (client_should_overtop(c) && c->isfloating) {
		wlr_scene_node_reparent(&c->scene->node, layers[LyrTop]);
	} else {
		wlr_scene_node_reparent(
			&c->scene->node,
			layers[fullscreen || c->isfloating ? LyrTop : LyrTile]);
	}

	if (!c->isfullscreen && old_fullscreen_state) {
		restore_size_per(c->mon, c);
	}

	if (c->isfullscreen && !old_fullscreen_state) {
		save_old_size_per(c->mon);
	}

	arrange(c->mon, false, false);
}
