#pragma once

/* XWayland-specific listeners and helpers. The entire body is wrapped
   in #ifdef XWAYLAND so a build without xcb / xcb-icccm leaves no
   dangling references. Single-TU header included exactly once. */

#ifdef XWAYLAND
void fix_xwayland_unmanaged_coordinate(Client *c) {
	if (!selmon)
		return;

	if (c->geom.x >= selmon->m.x && c->geom.x < selmon->m.x + selmon->m.width &&
		c->geom.y >= selmon->m.y && c->geom.y < selmon->m.y + selmon->m.height)
		return;

	c->geom = setclient_coordinate_center(c, selmon, c->geom, 0, 0);
}

int32_t synckeymap(void *data) {
	reset_keyboard_layout();

	wlr_log(WLR_INFO, "timer to synckeymap done");
	wl_event_source_timer_update(sync_keymap, 0);
	return 0;
}

void activatex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, activate);
	bool need_arrange = false;

	if (!c || c->iskilling || !c->foreign_toplevel || client_is_unmanaged(c))
		return;

	if (c && c->swallowing)
		return;

	if (c->isminimized) {
		client_pending_minimized_state(c, 0);
		c->tags = c->mini_restore_tag;
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		setborder_color(c);
		if (VISIBLEON(c, c->mon)) {
			need_arrange = true;
		}
	}

	if (config.focus_on_activate && !c->istagsilent && c != selmon->sel) {
		if (!(c->mon == selmon && c->tags & c->mon->tagset[c->mon->seltags]))
			view_in_mon(&(Arg){.ui = c->tags}, true, c->mon, true);
		wlr_xwayland_surface_activate(c->surface.xwayland, 1);
		focusclient(c, 1);
		need_arrange = true;
	} else if (c != focustop(selmon)) {
		c->isurgent = 1;
		if (client_surface(c)->mapped)
			setborder_color(c);
	}

	if (need_arrange) {
		arrange(c->mon, false, false);
	}

	printstatus();
}

void configurex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, configure);
	struct wlr_xwayland_surface_configure_event *event = data;
	if (!client_surface(c) || !client_surface(c)->mapped) {
		wlr_xwayland_surface_configure(c->surface.xwayland, event->x, event->y,
									   event->width, event->height);
		return;
	}
	if (client_is_unmanaged(c)) {
		wlr_scene_node_set_position(&c->scene->node, event->x, event->y);
		wlr_xwayland_surface_configure(c->surface.xwayland, event->x, event->y,
									   event->width, event->height);
		return;
	}
	if ((c->isfloating && c != grabc) ||
		!c->mon->pertag->ltidxs[c->mon->pertag->curtag]->arrange) {
		resize(c,
			   (struct wlr_box){.x = event->x - c->bw,
								.y = event->y - c->bw,
								.width = event->width + c->bw * 2,
								.height = event->height + c->bw * 2},
			   0);
	} else {
		arrange(c->mon, false, false);
	}
}

void createnotifyx11(struct wl_listener *listener, void *data) {
	struct wlr_xwayland_surface *xsurface = data;
	Client *c = NULL;

	c = xsurface->data = ecalloc(1, sizeof(*c));
	c->surface.xwayland = xsurface;
	c->type = X11;

	LISTEN(&xsurface->events.associate, &c->associate, associatex11);
	LISTEN(&xsurface->events.destroy, &c->destroy, destroynotify);
	LISTEN(&xsurface->events.dissociate, &c->dissociate, dissociatex11);
	LISTEN(&xsurface->events.request_activate, &c->activate, activatex11);
	LISTEN(&xsurface->events.request_configure, &c->configure, configurex11);
	LISTEN(&xsurface->events.request_fullscreen, &c->fullscreen,
		   fullscreennotify);
	LISTEN(&xsurface->events.set_hints, &c->set_hints, sethints);
	LISTEN(&xsurface->events.set_title, &c->set_title, updatetitle);
	LISTEN(&xsurface->events.request_maximize, &c->maximize, maximizenotify);
	LISTEN(&xsurface->events.request_minimize, &c->minimize, minimizenotify);
}

void commitx11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, commmitx11);
	struct wlr_surface_state *state = &c->surface.xwayland->surface->current;

	if ((int32_t)c->geom.width - 2 * (int32_t)c->bw == (int32_t)state->width &&
		(int32_t)c->geom.height - 2 * (int32_t)c->bw ==
			(int32_t)state->height &&
		(int32_t)c->surface.xwayland->x ==
			(int32_t)c->geom.x + (int32_t)c->bw &&
		(int32_t)c->surface.xwayland->y ==
			(int32_t)c->geom.y + (int32_t)c->bw) {
		c->configure_serial = 0;
	}
}

void associatex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, associate);

	LISTEN(&client_surface(c)->events.map, &c->map, mapnotify);
	LISTEN(&client_surface(c)->events.unmap, &c->unmap, unmapnotify);
	LISTEN(&client_surface(c)->events.commit, &c->commmitx11, commitx11);
}

void dissociatex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, dissociate);
	wl_list_remove(&c->map.link);
	wl_list_remove(&c->unmap.link);
	wl_list_remove(&c->commmitx11.link);
}

void sethints(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_hints);
	struct wlr_surface *surface = client_surface(c);
	if (c == focustop(selmon) || !c || !c->surface.xwayland->hints)
		return;

	c->isurgent = xcb_icccm_wm_hints_get_urgency(c->surface.xwayland->hints);
	printstatus();

	if (c->isurgent && surface && surface->mapped)
		setborder_color(c);
}

void xwaylandready(struct wl_listener *listener, void *data) {
	struct wlr_xcursor *xcursor;

	wlr_xwayland_set_seat(xwayland, seat);

	if ((xcursor = wlr_xcursor_manager_get_xcursor(cursor_mgr, "default", 1)))
		wlr_xwayland_set_cursor(
			xwayland, xcursor->images[0]->buffer, xcursor->images[0]->width * 4,
			xcursor->images[0]->width, xcursor->images[0]->height,
			xcursor->images[0]->hotspot_x, xcursor->images[0]->hotspot_y);

	wl_event_source_timer_update(sync_keymap, 500);
}

static void setgeometrynotify(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_geometry);

	wlr_scene_node_set_position(&c->scene->node, c->surface.xwayland->x,
								c->surface.xwayland->y);
	motionnotify(0, NULL, 0, 0, 0, 0);
}
#endif
