#pragma once

/* Monitor creation + teardown lifecycle. createmon handles allocation
   and mode/state setup; cleanupmon tears the Monitor down on output
   destroy; closemon migrates the dying monitor's clients to selmon (or
   detaches them when the last output is gone). Single-TU header
   included once. */

/* Listener: output destroyed - destroy layer surfaces, workspace handles, and the Monitor. */
void cleanupmon(struct wl_listener *listener, void *data) {
	Monitor *m = wl_container_of(listener, m, destroy);
	LayerSurface *l = NULL, *tmp = NULL;
	uint32_t i;

	m->iscleanuping = true;

	for (i = 0; i < LENGTH(m->layers); i++) {
		wl_list_for_each_safe(l, tmp, &m->layers[i], link)
			wlr_layer_surface_v1_destroy(l->layer_surface);
	}

	wlr_ext_workspace_group_handle_v1_output_leave(m->ext_group, m->wlr_output);
	wlr_ext_workspace_group_handle_v1_destroy(m->ext_group);
	cleanup_workspaces_by_monitor(m);

	wl_list_remove(&m->destroy.link);
	wl_list_remove(&m->frame.link);
	wl_list_remove(&m->link);
	wl_list_remove(&m->request_state.link);
	if (m->lock_surface)
		destroylocksurface(&m->destroy_lock_surface, NULL);
	m->wlr_output->data = NULL;
	wlr_output_layout_remove(output_layout, m->wlr_output);
	if (m->ov_dim) {
		wlr_scene_node_destroy(&m->ov_dim->node);
		m->ov_dim = NULL;
	}
	wlr_scene_output_destroy(m->scene_output);

	closemon(m);
	if (m->blur) {
		wlr_scene_node_destroy(&m->blur->node);
		m->blur = NULL;
	}
	if (m->skip_frame_timeout) {
		monitor_stop_skip_frame_timer(m);
		wl_event_source_remove(m->skip_frame_timeout);
		m->skip_frame_timeout = NULL;
	}
	if (m->battery_frame_throttle) {
		wl_event_source_remove(m->battery_frame_throttle);
		m->battery_frame_throttle = NULL;
	}
	if (m->render_deadline) {
		wl_event_source_remove(m->render_deadline);
		m->render_deadline = NULL;
	}
	m->wlr_output->data = NULL;

	cleanup_monitor_dwindle(m);
	cleanup_monitor_scroller(m);

	free(m->pertag);
	free(m);
}

/* Migrate every client from monitor m to selmon (or detach if no monitor remains). */
void closemon(Monitor *m) {

	Client *c = NULL;
	int32_t i = 0, nmons = wl_list_length(&mons);
	if (!nmons) {
		selmon = NULL;
	} else if (m == selmon) {
		do
			selmon = wl_container_of(mons.next, selmon, link);
		while (!selmon->wlr_output->enabled && i++ < nmons);

		if (!selmon->wlr_output->enabled)
			selmon = NULL;
	}

	wl_list_for_each(c, &clients, link) {
		if (c->mon == m) {

			if (selmon == NULL) {
				if (c->foreign_toplevel) {
					wlr_foreign_toplevel_handle_v1_output_leave(
						c->foreign_toplevel, c->mon->wlr_output);
					wlr_foreign_toplevel_handle_v1_destroy(c->foreign_toplevel);
					c->foreign_toplevel = NULL;
				}

				/* Last monitor going away. setmon's per-monitor index
				   would short-circuit (oldmon == new == NULL), so we
				   unlink manually before clearing the pointer. */
				if (c->mon_link_active) {
					wl_list_remove(&c->mon_link);
					c->mon_link_active = false;
				}
				c->mon = NULL;
			} else {
				client_change_mon(c, selmon);
			}

			if (c->oldmonname[0] == '\0') {
				client_update_oldmonname_record(c, m);
			}
		}
	}
	if (selmon) {
		focusclient(focustop(selmon), 1);
		printstatus();
	}
}


void createmon(struct wl_listener *listener, void *data) {

	struct wlr_output *wlr_output = data;
	const ConfigMonitorRule *r;
	uint32_t i;
	int32_t ji, vrr, custom;
	struct wlr_output_state state;
	Monitor *m = NULL;
	bool custom_monitor_mode = false;

	if (!wlr_output_init_render(wlr_output, alloc, drw))
		return;

	if (wlr_output->non_desktop) {
		if (drm_lease_manager) {
			wlr_drm_lease_v1_manager_offer_output(drm_lease_manager,
												  wlr_output);
		}
		return;
	}

	struct wl_event_loop *loop = wl_display_get_event_loop(dpy);
	m = wlr_output->data = ecalloc(1, sizeof(*m));

	m->iscleanuping = false;
	m->skip_frame_timeout =
		wl_event_loop_add_timer(loop, monitor_skip_frame_timeout_callback, m);
	m->battery_frame_throttle =
		wl_event_loop_add_timer(loop, battery_frame_throttle_callback, m);
	m->render_deadline =
		wl_event_loop_add_timer(loop, render_deadline_callback, m);
	m->last_anim_schedule_ms = 0;
	m->skiping_frame = false;
	m->resizing_count_pending = 0;
	m->resizing_count_current = 0;

	m->wlr_output = wlr_output;
	m->wlr_output->data = m;

	wl_list_init(&m->dwl_ipc_outputs);
	wl_list_init(&m->clients);

	for (i = 0; i < LENGTH(m->layers); i++)
		wl_list_init(&m->layers[i]);

	m->gappih = config.gappih;
	m->gappiv = config.gappiv;
	m->gappoh = config.gappoh;
	m->gappov = config.gappov;
	m->isoverview = 0;
	m->sel = NULL;
	m->is_in_hotarea = 0;
	m->m.x = INT32_MAX;
	m->m.y = INT32_MAX;
	float scale = 1;
	enum wl_output_transform rr = WL_OUTPUT_TRANSFORM_NORMAL;

	wlr_output_state_init(&state);
	wlr_output_state_set_scale(&state, scale);
	wlr_output_state_set_transform(&state, rr);

	/* Force horizontal-RGB subpixel order so clients can do LCD subpixel AA.
	   Only correct on standard RGB-stripe panels; opt-in via config. */
	if (config.subpixel_rgb)
		wlr_output_state_set_subpixel(&state, WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB);

	for (ji = 0; ji < config.monitor_rules_count; ji++) {
		if (config.monitor_rules_count < 1)
			break;

		r = &config.monitor_rules[ji];

		if (monitor_matches_rule(m, r)) {
			m->m.x = r->x == INT32_MAX ? INT32_MAX : r->x;
			m->m.y = r->y == INT32_MAX ? INT32_MAX : r->y;
			vrr = r->vrr >= 0 ? r->vrr : 0;
			custom = r->custom >= 0 ? r->custom : 0;
			scale = r->scale;
			rr = r->rr;

			if (apply_rule_to_state(m, r, &state, vrr, custom)) {
				custom_monitor_mode = true;
			}
			break;
		}
	}

	if (!custom_monitor_mode)
		wlr_output_state_set_mode(&state,
								  wlr_output_preferred_mode(wlr_output));

	LISTEN(&wlr_output->events.frame, &m->frame, rendermon);
	LISTEN(&wlr_output->events.destroy, &m->destroy, cleanupmon);
	LISTEN(&wlr_output->events.request_state, &m->request_state,
		   requestmonstate);

	wlr_output_state_set_enabled(&state, 1);
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	wl_list_insert(&mons, &m->link);

	m->pertag = calloc(1, sizeof(Pertag));
	for (int i = 0; i < LENGTH(tags) + 1; i++)
		m->pertag->scroller_state[i] = NULL;

	if (chvt_backup_tag &&
		regex_match(chvt_backup_selmon, m->wlr_output->name)) {
		m->tagset[0] = m->tagset[1] = (1 << (chvt_backup_tag - 1)) & TAGMASK;
		m->pertag->curtag = m->pertag->prevtag = chvt_backup_tag;
		chvt_backup_tag = 0;
		memset(chvt_backup_selmon, 0, sizeof(chvt_backup_selmon));
	} else {
		m->tagset[0] = m->tagset[1] = 1;
		m->pertag->curtag = m->pertag->prevtag = 1;
	}

	for (i = 0; i <= LENGTH(tags); i++) {
		m->pertag->nmasters[i] = config.default_nmaster;
		m->pertag->mfacts[i] = config.default_mfact;
		m->pertag->ltidxs[i] = &layouts[0];
	}

	parse_tagrule(m);

	m->scene_output = wlr_scene_output_create(scene, wlr_output);

	/* Overview dim backdrop: a black rect sitting just under the tiled
	   clients, faded in while this monitor is in overview. */
	m->ov_dim = wlr_scene_rect_create(&scene->tree, 0, 0,
									  (float[4]){0, 0, 0, 0});
	wlr_scene_node_place_below(&m->ov_dim->node, &layers[LyrTile]->node);
	wlr_scene_node_set_enabled(&m->ov_dim->node, false);
	m->ov_dim_cur = 0.0f;

	if (m->m.x == INT32_MAX || m->m.y == INT32_MAX)
		wlr_output_layout_add_auto(output_layout, wlr_output);
	else
		wlr_output_layout_add(output_layout, wlr_output, m->m.x, m->m.y);

	if (config.blur) {
		m->blur = wlr_scene_optimized_blur_create(&scene->tree, 0, 0);
		wlr_scene_node_set_position(&m->blur->node, m->m.x, m->m.y);
		wlr_scene_node_reparent(&m->blur->node, layers[LyrBlur]);
		wlr_scene_optimized_blur_set_size(m->blur, m->m.width, m->m.height);
	}
	m->ext_group = wlr_ext_workspace_group_handle_v1_create(
		ext_manager, EXT_WORKSPACE_ENABLE_CAPS);
	wlr_ext_workspace_group_handle_v1_output_enter(m->ext_group, m->wlr_output);

	for (i = 1; i <= LENGTH(tags); i++) {
		add_workspace_by_tag(i, m);
	}

	printstatus();
}
