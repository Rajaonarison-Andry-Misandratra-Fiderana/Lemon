#pragma once

/* Apply user-configured window rules to a freshly-mapped Client. Picks
   the rule's target monitor, sets initial geom + tags, runs the
   swallow handshake, applies size bounds, and finally hands off to
   setmon / setfullscreen / view_in_mon to make the window land in the
   right place. Single-TU header included once. */

/* Match the client against configured window rules and apply properties, geometry, and tags. */
void applyrules(Client *c) {

	const char *appid, *title;
	uint32_t i, newtags = 0;
	const ConfigWinRule *r;
	Monitor *m = NULL;
	Client *fc = NULL;
	Client *parent = NULL;

	if (!c)
		return;

	parent = client_get_parent(c);

	Monitor *mon = parent && parent->mon ? parent->mon : selmon;

	c->isfloating = client_is_float_type(c) || parent;

#ifdef XWAYLAND
	if (c->isfloating && client_is_x11(c)) {
		fix_xwayland_unmanaged_coordinate(c);
		c->float_geom = c->geom;
	}
#endif

	if (!(appid = client_get_appid(c)))
		appid = broken;
	if (!(title = client_get_title(c)))
		title = broken;

	for (i = 0; i < config.window_rules_count; i++) {

		r = &config.window_rules[i];

		if (!is_window_rule_matches(r, appid, title))
			continue;

		apply_rule_properties(c, r);

		if (r->tags > 0) {
			newtags |= r->tags;
		} else if (parent) {
			newtags = parent->tags;
		}

		wl_list_for_each(m, &mons, link) {
			if (match_monitor_spec(r->monitor, m)) {
				mon = m;
			}
		}

		if (c->isnamedscratchpad) {
			c->isfloating = 1;
		}

		if (r->width > 1)
			c->float_geom.width = r->width;
		else if (r->width > 0 && r->width <= 1)
			c->float_geom.width = round(mon->m.width * r->width);
		if (r->height > 1)
			c->float_geom.height = r->height;
		else if (r->height > 0 && r->height <= 1)
			c->float_geom.height = round(mon->m.height * r->height);

		if (r->width > 0 || r->height > 0) {
			c->iscustomsize = 1;
		}

		if (r->offsetx || r->offsety) {
			c->iscustompos = 1;
			c->float_geom = c->geom = setclient_coordinate_center(
				c, mon, c->float_geom, r->offsetx, r->offsety);
		}
		if (c->isfloating) {
			c->geom = c->float_geom.width > 0 && c->float_geom.height > 0
						  ? c->float_geom
						  : c->geom;
			if (!c->isnosizehint)
				client_set_size_bound(c);
		}
	}

	if (mon)
		set_size_per(mon, c);

	if (!c->iscustompos &&
		(!client_is_x11(c) || (c->geom.x == 0 && c->geom.y == 0))) {
		c->float_geom = c->geom =
			setclient_coordinate_center(c, mon, c->geom, 0, 0);
	} else {
		c->float_geom = c->geom;
	}

	struct wlr_surface *surface = client_surface(c);
	if (!surface || !surface->mapped)
		return;

	c->pid = client_get_pid(c);
	if (!c->noswallow && !c->isfloating && !client_is_float_type(c) &&
		!c->surface.xdg->initial_commit) {
		Client *p = termforwin(c);
		if (p && !p->isminimized) {
			c->swallowedby = p;
			p->swallowing = c;
			wl_list_remove(&c->link);
			wl_list_remove(&c->flink);
			swallow(c, p);
			wl_list_remove(&p->link);
			wl_list_remove(&p->flink);
			mon = p->mon;
			newtags = p->tags;
		}
	}

	int32_t fullscreen_state_backup =
		c->isfullscreen || client_wants_fullscreen(c);

	bool should_init_get_focus =
		!c->isopensilent &&
		!(client_is_x11_popup(c) && client_should_ignore_focus(c)) && mon &&
		(!c->istagsilent || !newtags || newtags & mon->tagset[mon->seltags]);

	if (!should_init_get_focus) {
		wl_list_remove(&c->flink);
		wl_list_insert(fstack.prev, &c->flink);
	}

	setmon(c, mon, newtags, should_init_get_focus);

	if (!c->isfloating) {
		c->old_stack_inner_per = c->stack_inner_per;
		c->old_master_inner_per = c->master_inner_per;
	}

	if (c->mon &&
		!(c->mon == selmon && c->tags & c->mon->tagset[c->mon->seltags]) &&
		!c->isopensilent && !c->istagsilent) {
		c->animation.tag_from_rule = true;
		view_in_mon(&(Arg){.ui = c->tags}, true, c->mon, true);
	}

	setfullscreen(c, fullscreen_state_backup);

	if (c->isfakefullscreen) {
		setfakefullscreen(c, 1);
	}

	wl_list_for_each(fc, &clients,
					 link) if (fc && fc != c && c->tags & fc->tags && c->mon &&
							   VISIBLEON(fc, c->mon) && ISFULLSCREEN(fc) &&
							   !c->isfloating) {
		clear_fullscreen_flag(fc);
		arrange(c->mon, false, false);
	}

	if (c->isfloating && !c->iscustompos && !c->isnamedscratchpad) {
		wl_list_remove(&c->link);
		wl_list_insert(clients.prev, &c->link);
		set_float_malposition(c);
	}

	if (c->isnamedscratchpad) {
		apply_named_scratchpad(c);
	}

	if (c->isoverlay && c->scene) {
		wlr_scene_node_reparent(&c->scene->node, layers[LyrOverlay]);
		wlr_scene_node_raise_to_top(&c->scene->node);
	}
}
