#pragma once

/* Output-layout reconfiguration. Called whenever the
   wlr_output_layout reports a change (enable/disable, position move):
   tears down disabled outputs, lays out the remainder with the auto-
   layout fallback, repositions root + locked backgrounds, drags
   floating clients to keep their relative position, re-arranges every
   monitor, and notifies output-manager clients of the new config.
   Single-TU header included once. */

void updatemons(struct wl_listener *listener, void *data) {

	struct wlr_output_configuration_v1 *output_config =
		wlr_output_configuration_v1_create();
	Client *c = NULL;
	struct wlr_output_configuration_head_v1 *config_head;
	Monitor *m = NULL;
	int32_t mon_pos_offsetx, mon_pos_offsety, oldx, oldy;

	wl_list_for_each(m, &mons, link) {
		if (m->wlr_output->enabled || m->asleep)
			continue;
		config_head = wlr_output_configuration_head_v1_create(output_config,
															  m->wlr_output);
		config_head->state.enabled = 0;

		wlr_output_layout_remove(output_layout, m->wlr_output);
		closemon(m);
		m->m = m->w = (struct wlr_box){0};
	}

	wl_list_for_each(m, &mons, link) {
		if (m->wlr_output->enabled &&
			!wlr_output_layout_get(output_layout, m->wlr_output))
			wlr_output_layout_add_auto(output_layout, m->wlr_output);
	}

	wlr_output_layout_get_box(output_layout, NULL, &sgeom);

	wlr_scene_node_set_position(&root_bg->node, sgeom.x, sgeom.y);
	wlr_scene_rect_set_size(root_bg, sgeom.width, sgeom.height);

	wlr_scene_node_set_position(&locked_bg->node, sgeom.x, sgeom.y);
	wlr_scene_rect_set_size(locked_bg, sgeom.width, sgeom.height);

	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled)
			continue;
		config_head = wlr_output_configuration_head_v1_create(output_config,
															  m->wlr_output);

		oldx = m->m.x;
		oldy = m->m.y;

		wlr_output_layout_get_box(output_layout, m->wlr_output, &m->m);
		m->w = m->m;
		mon_pos_offsetx = m->m.x - oldx;
		mon_pos_offsety = m->m.y - oldy;

		wl_list_for_each(c, &clients, link) {

			if (c->isfloating && c->mon == m) {
				c->geom.x += mon_pos_offsetx;
				c->geom.y += mon_pos_offsety;
				c->float_geom = c->geom;
				resize(c, c->geom, 1);
			}

			if (c->mon && c->mon != m && client_surface(c)->mapped &&
				strcmp(c->oldmonname, m->wlr_output->name) == 0) {
				client_change_mon(c, m);
			}
		}

		wlr_scene_output_set_position(m->scene_output, m->m.x, m->m.y);

		if (config.blur && m->blur) {
			wlr_scene_node_set_position(&m->blur->node, m->m.x, m->m.y);
			wlr_scene_optimized_blur_set_size(m->blur, m->m.width, m->m.height);
		}

		if (m->lock_surface) {
			struct wlr_scene_tree *scene_tree = m->lock_surface->surface->data;
			wlr_scene_node_set_position(&scene_tree->node, m->m.x, m->m.y);
			wlr_session_lock_surface_v1_configure(m->lock_surface, m->m.width,
												  m->m.height);
		}

		arrangelayers(m);

		arrange(m, false, false);

		if ((c = focustop(m)) && c->isfullscreen)
			resize(c, m->m, 0);

		config_head->state.x = m->m.x;
		config_head->state.y = m->m.y;

		if (!selmon)
			selmon = m;
	}

	if (selmon && selmon->wlr_output->enabled) {
		wl_list_for_each(c, &clients, link) {
			if (!c->mon && client_surface(c)->mapped) {
				c->mon = selmon;
				reset_foreign_tolevel(c, NULL, c->mon);
			}
			if (c->tags == 0 && !c->is_in_scratchpad) {
				c->tags = selmon->tagset[selmon->seltags];
				set_size_per(selmon, c);
			}
		}
		focusclient(focustop(selmon), 1);
		if (selmon->lock_surface) {
			client_notify_enter(selmon->lock_surface->surface,
								wlr_seat_get_keyboard(seat));
			client_activate_surface(selmon->lock_surface->surface, 1);
		}
	}

	wlr_cursor_move(cursor, NULL, 0, 0);

	wlr_output_manager_v1_set_configuration(output_mgr, output_config);
}
