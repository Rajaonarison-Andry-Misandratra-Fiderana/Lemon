#pragma once

/* xdg-shell + xwayland-shared toplevel lifecycle: createnotify wires
   the LISTEN entries on a new xdg_toplevel; mapnotify builds the
   client's scene tree, border, shadow, applies rules and kicks the
   open animation; destroynotify tears the listener wires down and
   frees the Client; fullscreen / maximize / minimize notify route
   client requests through the public set* helpers. unminimize and
   set_minimized are the helpers minimizenotify calls. Single-TU
   header included once from lemon.c. */

void unminimize(Client *c) {
	if (c && c->is_in_scratchpad && c->is_scratchpad_show) {
		client_pending_minimized_state(c, 0);
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		setborder_color(c);
		return;
	}

	if (c && c->isminimized) {
		show_hide_client(c);
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		setborder_color(c);
		arrange(c->mon, false, false);
		return;
	}
}

void set_minimized(Client *c) {

	if (!c || !c->mon)
		return;

	c->isglobal = 0;
	c->oldtags = c->mon->tagset[c->mon->seltags];
	c->mini_restore_tag = c->tags;
	c->tags = 0;
	client_pending_minimized_state(c, 1);
	c->is_in_scratchpad = 1;
	c->is_scratchpad_show = 0;
	focusclient(focustop(selmon), 1);
	arrange(c->mon, false, false);

	CLIENT_SET_FOREIGN_ACTIVATED(c, false);

	wl_list_remove(&c->link);
	wl_list_insert(clients.prev, &c->link);
}

void
createnotify(struct wl_listener *listener, void *data) {

	struct wlr_xdg_toplevel *toplevel = data;
	Client *c = NULL;

	c = toplevel->base->data = ecalloc(1, sizeof(*c));
	c->surface.xdg = toplevel->base;
	c->bw = config.borderpx;

	LISTEN(&toplevel->base->surface->events.commit, &c->commit, commitnotify);
	LISTEN(&toplevel->base->surface->events.map, &c->map, mapnotify);
	LISTEN(&toplevel->base->surface->events.unmap, &c->unmap, unmapnotify);
	LISTEN(&toplevel->events.destroy, &c->destroy, destroynotify);
	LISTEN(&toplevel->events.request_fullscreen, &c->fullscreen,
		   fullscreennotify);
	LISTEN(&toplevel->events.request_maximize, &c->maximize, maximizenotify);
	LISTEN(&toplevel->events.request_minimize, &c->minimize, minimizenotify);
	LISTEN(&toplevel->events.set_title, &c->set_title, updatetitle);
}

void
destroynotify(struct wl_listener *listener, void *data) {

	Client *c = wl_container_of(listener, c, destroy);
	wl_list_remove(&c->destroy.link);
	wl_list_remove(&c->set_title.link);
	wl_list_remove(&c->fullscreen.link);
	wl_list_remove(&c->maximize.link);
	wl_list_remove(&c->minimize.link);
#ifdef XWAYLAND
	if (c->type != XDGShell) {
		wl_list_remove(&c->activate.link);
		wl_list_remove(&c->associate.link);
		wl_list_remove(&c->configure.link);
		wl_list_remove(&c->dissociate.link);
		wl_list_remove(&c->set_hints.link);
	} else
#endif
	{
		wl_list_remove(&c->commit.link);
		wl_list_remove(&c->map.link);
		wl_list_remove(&c->unmap.link);
	}
	if (c->mon_link_active) {
		wl_list_remove(&c->mon_link);
		c->mon_link_active = false;
	}
	free(c);
}

void
fullscreennotify(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, fullscreen);

	if (!c || c->iskilling)
		return;

	setfullscreen(c, client_wants_fullscreen(c));
}

void maximizenotify(struct wl_listener *listener, void *data) {

	Client *c = wl_container_of(listener, c, maximize);

	if (!c || !c->mon || c->iskilling || c->ignore_maximize)
		return;

	if (!client_is_x11(c) && !c->surface.xdg->initialized) {
		return;
	}

	if (client_request_maximize(c, data)) {
		setmaximizescreen(c, 1);
	} else {
		setmaximizescreen(c, 0);
	}
}

void minimizenotify(struct wl_listener *listener, void *data) {

	Client *c = wl_container_of(listener, c, minimize);

	if (!c || !c->mon || c->iskilling || c->isminimized)
		return;

	if (client_request_minimize(c, data) && !c->ignore_minimize) {
		if (!c->isminimized)
			set_minimized(c);
		client_set_minimized(c, true);
	} else {
		if (c->isminimized)
			unminimize(c);
		client_set_minimized(c, false);
	}
}

void
mapnotify(struct wl_listener *listener, void *data) {

	Client *at_client = NULL;
	Client *c = wl_container_of(listener, c, map);
	int32_t i = 0;

	c->scene = client_surface(c)->data = wlr_scene_tree_create(layers[LyrTile]);
	wlr_scene_node_set_enabled(&c->scene->node, c->type != XDGShell);
	c->scene_surface =
		c->type == XDGShell
			? wlr_scene_xdg_surface_create(c->scene, c->surface.xdg)
			: wlr_scene_subsurface_tree_create(c->scene, client_surface(c));
	c->scene->node.data = c->scene_surface->node.data = c;

	client_get_geometry(c, &c->geom);

	if (client_is_x11(c))
		init_client_properties(c);

	if (client_is_unmanaged(c) || client_is_x11_popup(c)) {
		c->bw = 0;
		c->isnoborder = 1;
	} else {
		c->bw = config.borderpx;
	}

	if (client_should_global(c)) {
		c->isunglobal = 1;
	}

	c->geom.width += 2 * c->bw;
	c->geom.height += 2 * c->bw;

	if (client_is_unmanaged(c)) {

#ifdef XWAYLAND
		if (client_is_x11(c)) {
			fix_xwayland_unmanaged_coordinate(c);
			LISTEN(&c->surface.xwayland->events.set_geometry, &c->set_geometry,
				   setgeometrynotify);
		}
#endif
		wlr_scene_node_reparent(&c->scene->node, layers[LyrOverlay]);
		wlr_scene_node_set_position(&c->scene->node, c->geom.x, c->geom.y);
		if (client_wants_focus(c)) {
			focusclient(c, 1);
			exclusive_focus = c;
		}
		return;
	}

	/* splitindicator rects are only ever shown under the dwindle layout
	   with dwindle_manual_split=true. Allocate them on first use inside
	   apply_border() instead of paying the cost for every client. */
	for (i = 0; i < 2; i++) {
		c->splitindicator[i] = NULL;
	}

	/* droparea scene rect is allocated lazily on first drag (rare event).
	   Saves one wlr_scene_rect + damage tracking state per client. */
	c->droparea = NULL;

	c->border = wlr_scene_rect_create(
		c->scene, 0, 0, c->isurgent ? config.urgentcolor : config.bordercolor);
	wlr_scene_node_lower_to_bottom(&c->border->node);
	wlr_scene_node_set_position(&c->border->node, 0, 0);
	wlr_scene_rect_set_corner_radius(c->border, config.border_radius,
									 config.border_radius_location_default);
	wlr_scene_node_set_enabled(&c->border->node, true);

	c->shadow =
		wlr_scene_shadow_create(c->scene, 0, 0, config.border_radius,
								config.shadows_blur, config.shadowscolor);

	wlr_scene_node_lower_to_bottom(&c->shadow->node);
	wlr_scene_node_set_enabled(&c->shadow->node, true);

	if (config.new_is_master && selmon && !is_scroller_layout(selmon))

		wl_list_insert(&clients, &c->link);
	else if (selmon && is_scroller_layout(selmon) &&
			 selmon->visible_scroll_tiling_clients > 0) {

		if (selmon->sel && ISSCROLLTILED(selmon->sel) &&
			VISIBLEON(selmon->sel, selmon)) {
			at_client = scroll_get_stack_tail_client(selmon->sel);
		} else {
			at_client = center_tiled_select(selmon);
		}

		if (at_client) {
			wl_list_insert(&at_client->link, &c->link);
		} else {
			wl_list_insert(clients.prev, &c->link);
		}
	} else
		wl_list_insert(clients.prev, &c->link);

	wl_list_insert(&fstack, &c->flink);

	applyrules(c);

	if (!c->isfloating || c->force_tiled_state) {
		client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT |
								WLR_EDGE_RIGHT);
	}

	wlr_scene_node_for_each_buffer(&c->scene_surface->node,
								   iter_xdg_scene_buffers, c);

	setborder_color(c);

	c->is_pending_open_animation = true;
	resize(c, c->geom, 0);
	printstatus();

	/* Under the vertical scroller, a freshly opened window can be
	   auto-maximize-to-screen. Off by default -- it stops the scroller
	   from actually tiling new windows alongside the existing ones,
	   which surprised users (every app launched fullscreen). Opt in
	   via scroller_auto_maximize=1 in the config. */
	if (config.scroller_auto_maximize && !c->isfloating && c->mon &&
		!c->ismaximizescreen &&
		c->mon->pertag->ltidxs[c->mon->pertag->curtag]->id == VERTICAL_SCROLLER)
		setmaximizescreen(c, 1);
}
