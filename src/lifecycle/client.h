#pragma once

/* Client lifecycle. setmon reassigns a client between monitors and
   keeps the per-monitor index (Monitor.clients / Client.mon_link)
   consistent; unmapnotify handles teardown: cycler rebuild on
   in-cycler unmap, surface-cache geometry save, fadeout init,
   scroller/dwindle removal, focus retarget, foreign-toplevel cleanup
   and the swallow / swallowedby chain. Single-TU header included
   once from lemon.c. */

void setmon(Client *c, Monitor *m, uint32_t newtags, bool focus) {
	Monitor *oldmon = c->mon;

	if (oldmon == m)
		return;

	if (oldmon && oldmon->sel == c) {
		oldmon->sel = NULL;
	}

	if (oldmon && oldmon->prevsel == c) {
		oldmon->prevsel = NULL;
	}

	/* Move c between per-monitor lists. mon_link_active is the explicit
	   membership flag (wl_list nodes start uninitialized in ecalloc'd
	   Clients; we set it once linked). */
	if (c->mon_link_active) {
		wl_list_remove(&c->mon_link);
		c->mon_link_active = false;
	}
	if (m) {
		wl_list_insert(&m->clients, &c->mon_link);
		c->mon_link_active = true;
	}

	c->mon = m;

	if (oldmon)
		arrange(oldmon, false, false);
	if (m) {

		reset_foreign_tolevel(c, oldmon, m);
		resize(c, c->geom, 0);
		client_reset_mon_tags(c, m, newtags);
		check_match_tag_floating_rule(c, m);
		setfloating(c, c->isfloating);
		setfullscreen(c, c->isfullscreen);
	}

	if (focus && !client_is_x11_popup(c)) {
		focusclient(focustop(selmon), 1);
	}
}

void unmapnotify(struct wl_listener *listener, void *data) {

	Client *c = wl_container_of(listener, c, unmap);
	Monitor *m = NULL;
	Client *nextfocus = NULL;
	c->iskilling = 1;

	/* If the window the cycler is referencing is about to disappear,
	   rebuild the overlay from the surviving clients instead of
	   tearing it down -- the user wants Alt+Tab to stay open until
	   they release ALT or left-pick a tile, including across
	   right-click-to-kill and any other voluntary close. */
	if (window_cycler.active) {
		for (int32_t i = 0; i < window_cycler.count; i++) {
			if (window_cycler.clients[i] == c) {
				Monitor *cyc_mon = window_cycler.mon;
				window_cycler_destroy();
				if (cyc_mon) {
					int32_t built = window_cycler_build(cyc_mon);
					if (built > 0)
						window_cycler_hover_at(cursor->x, cursor->y);
				}
				break;
			}
		}
	}
	/* Clear the cycler-raised tile pointer if its window is about to
	   tear down -- avoid reparenting a freed scene tree later. */
	if (cycler_raised_tile == c)
		cycler_raised_tile = NULL;

	/* Persist last-known size for this app_id before the client tears down,
	   so the next launch can configure straight to the right geometry. */
	if (!client_is_x11(c) && c->geom.width > 0 && c->geom.height > 0) {
		surface_cache_store(client_get_appid(c),
		                    c->geom.width - 2 * (int32_t)c->bw,
		                    c->geom.height - 2 * (int32_t)c->bw);
	}
	struct ScrollerStackNode *target_node =
		c->mon ? find_scroller_node(
					 c->mon->pertag->scroller_state[c->mon->pertag->curtag], c)
			   : NULL;
	struct ScrollerStackNode *prev_node =
		target_node ? target_node->prev_in_stack : NULL;
	struct ScrollerStackNode *next_node =
		target_node ? target_node->next_in_stack : NULL;

	if (config.animations && !c->is_clip_to_hide && !c->isminimized &&
		(!c->mon || VISIBLEON(c, c->mon)))
		init_fadeout_client(c);

	if (c->swallowedby) {
		c->swallowedby->mon = c->mon;
		swallow(c->swallowedby, c);
	} else {
		scroller_remove_client(c);
		dwindle_remove_client(c);
	}

	if (c == grabc) {
		cursor_mode = CurNormal;
		grabc = NULL;
	}

	if (c == dropc) {
		dropc = NULL;
	}

	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled) {
			continue;
		}
		if (c == m->sel) {
			m->sel = NULL;
		}
		if (c == m->prevsel) {
			m->prevsel = NULL;
		}
	}

	if (c->mon && c->mon == selmon) {
		if (next_node && !c->swallowedby) {
			nextfocus = next_node->client;
		} else if (prev_node && !c->swallowedby) {
			nextfocus = prev_node->client;
		} else {
			nextfocus = focustop(selmon);
		}

		if (nextfocus) {
			focusclient(nextfocus, 0);
		}

		if (!nextfocus && selmon->isoverview) {
			Arg arg = {0};
			toggleoverview(&arg);
		}
	}

	if (client_is_unmanaged(c)) {
#ifdef XWAYLAND
		if (client_is_x11(c)) {
			wl_list_remove(&c->set_geometry.link);
		}
#endif
		if (c == exclusive_focus)
			exclusive_focus = NULL;
		if (client_surface(c) == seat->keyboard_state.focused_surface)
			focusclient(focustop(selmon), 1);
	} else {
		if (!c->swallowing)
			wl_list_remove(&c->link);
		setmon(c, NULL, 0, true);
		if (!c->swallowing)
			wl_list_remove(&c->flink);
	}

	if (c->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_destroy(c->foreign_toplevel);
		c->foreign_toplevel = NULL;
	}

	if (c->swallowedby) {
		setmaximizescreen(c->swallowedby, c->ismaximizescreen);
		setfullscreen(c->swallowedby, c->isfullscreen);
		c->swallowedby->swallowing = NULL;
		c->swallowedby = NULL;
	}

	if (c->swallowing) {
		c->swallowing->swallowedby = NULL;
		c->swallowing = NULL;
	}

	c->stack_proportion = 0.0f;

	wlr_scene_node_destroy(&c->scene->node);
	printstatus();
	motionnotify(0, NULL, 0, 0, 0, 0);
}
