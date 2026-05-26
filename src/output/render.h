#pragma once

/* Per-monitor render path, extracted from lemon.c. Single-TU pattern:
   this header is included exactly once from lemon.c after all type
   definitions, globals and forward declarations are visible. Functions
   keep static linkage; the LEMON_HOT entry points (rendermon,
   do_rendermon) are signature-compatible with the listener prototype
   exposed in lemon.c. */

/* Decide whether m's scene output must be committed this frame. Commit
   when the scene has pending damage, or when a legacy wlr_screencopy_v1
   capture is in flight: those frame requests do not reliably mark the
   vanilla wlroots scene as needing a frame, so without this an idle
   screen would never satisfy them. The modern ext-image-copy-capture
   path drives needs_frame itself. */
static bool output_should_commit(Monitor *m) {
	if (wlr_scene_output_needs_frame(m->scene_output))
		return true;
	if (screencopy_mgr && !wl_list_empty(&screencopy_mgr->frames))
		return true;
	return false;
}

/* draw every layer-shell surface on m; true if any wants another frame */
static bool render_layer_surfaces(Monitor *m) {
	LayerSurface *l = NULL, *tmpl = NULL;
	bool more = false;
	for (int32_t i = 0; i < LENGTH(m->layers); i++) {
		wl_list_for_each_safe(l, tmpl, &m->layers[i], link) {
			if (layer_draw_frame(l))
				more = true;
		}
	}
	return more;
}

/* drive fade-out animations for dying clients and layers; true if still
   animating. Scoped per monitor so a multi-output session does not
   re-walk every fadeout from every monitor's frame callback. Fadeouts
   without a bound monitor (rare: client died mid-handover) still run
   from the first monitor to call us so they don't get orphaned. */
static bool render_fadeouts(Monitor *m) {
	Client *c = NULL, *tmp = NULL;
	LayerSurface *l = NULL, *tmpl = NULL;
	bool more = false;
	wl_list_for_each_safe(c, tmp, &fadeout_clients, fadeout_link) {
		Monitor *cm = c->mon ? c->mon : m;
		if (cm != m)
			continue;
		if (client_draw_fadeout_frame(c))
			more = true;
	}
	wl_list_for_each_safe(l, tmpl, &fadeout_layers, fadeout_link) {
		Monitor *lm = l->mon ? l->mon : m;
		if (lm != m)
			continue;
		if (layer_draw_fadeout_frame(l))
			more = true;
	}
	return more;
}

/* draw clients owned by m. Sets *skip when a no-animation configure
   round-trip means the commit must be deferred this frame. Returns true
   if any client wants another frame. We finish iterating even after a
   skip is observed so other animated clients still tick (otherwise a
   single mid-resize client would freeze every animation behind it).
   Walks the per-monitor index built by setmon() so multi-output
   sessions don't pay O(N_clients * N_outputs) per frame. */
static bool render_clients(Monitor *m, bool *skip) {
	Client *c = NULL, *tmp = NULL;
	bool more = false;
	bool want_skip = false;
	*skip = false;
	wl_list_for_each_safe(c, tmp, &m->clients, mon_link) {
		if (client_draw_frame(c))
			more = true;
		if (!config.animations && !grabc && c->configure_serial)
			want_skip = true;
	}
	if (want_skip) {
		monitor_check_skip_frame_timeout(m);
		*skip = true;
	} else if (m->skiping_frame) {
		monitor_stop_skip_frame_timer(m);
	}
	return more;
}

/* fade the overview dim backdrop toward its target; true if still fading */
static bool render_overview_dim(Monitor *m) {
	if (!m->ov_dim)
		return false;

	float target =
		(m->isoverview && config.overview_dim) ? config.overview_dim_alpha : 0.0f;

	uint32_t now = frame_now_ms();
	uint32_t dt = now - m->ov_dim_last_ms;
	m->ov_dim_last_ms = now;
	if (dt > 50)
		dt = 50; /* clamp after an idle gap so the fade never jumps */

	if (m->ov_dim_cur != target) {
		float step = (float)dt / 130.0f; /* ~130 ms fade */
		if (m->ov_dim_cur < target)
			m->ov_dim_cur = fminf(m->ov_dim_cur + step, target);
		else
			m->ov_dim_cur = fmaxf(m->ov_dim_cur - step, target);
		if (fabsf(m->ov_dim_cur - target) < 0.004f)
			m->ov_dim_cur = target;
	}

	bool visible = m->ov_dim_cur > 0.004f;
	wlr_scene_node_set_enabled(&m->ov_dim->node, visible);
	if (visible) {
		wlr_scene_node_set_position(&m->ov_dim->node, m->m.x, m->m.y);
		wlr_scene_rect_set_size(m->ov_dim, m->m.width, m->m.height);
		wlr_scene_rect_set_color(m->ov_dim,
								 (float[4]){0, 0, 0, m->ov_dim_cur});
	}

	return m->ov_dim_cur != target;
}

/* schedule the next output frame, honoring the on-battery throttle */
static void schedule_next_frame(Monitor *m) {
	if (on_battery && m->battery_frame_throttle) {
		uint32_t interval = config.battery_fps > 0
								? 1000u / (uint32_t)config.battery_fps
								: BATTERY_ANIM_INTERVAL_MS;
		uint32_t now_ms = frame_now_ms();
		uint32_t elapsed = now_ms - m->last_anim_schedule_ms;
		if (elapsed < interval) {
			wl_event_source_timer_update(m->battery_frame_throttle,
										 interval - elapsed);
			return;
		}
	}
	m->last_anim_schedule_ms = frame_now_ms();
	wlr_output_schedule_frame(m->wlr_output);
}

/* Draw all surfaces, commit, schedule next frame, and update the
   render-time EMA. Split from rendermon so the late-latch deadline
   timer can drive it too. */
LEMON_HOT void do_rendermon(Monitor *m) {
	struct timespec now;
	bool need_more_frames = false;
	bool skip = false;

	frame_clock_begin();

	struct timespec t0;
	bool timed = config.debug_frametime || config.late_latch;
	if (LEMON_UNLIKELY(timed))
		clock_gettime(CLOCK_MONOTONIC, &t0);

	bool frame_allow_tearing = check_tearing_frame_allow(m);

	bool layers_more = render_layer_surfaces(m);
	bool fadeouts_more = render_fadeouts(m);
	bool clients_more = render_clients(m, &skip);
	bool dim_more = render_overview_dim(m);
	bool cycler_more = false;
	if (window_cycler.active && window_cycler.mon == m)
		cycler_more = cycler_overlay_tick(frame_now_ms());
	need_more_frames = layers_more || fadeouts_more || clients_more ||
					   dim_more || cycler_more;

	if (!skip) {
		if (config.allow_tearing && frame_allow_tearing) {
			apply_tear_state(m);
		} else if (output_should_commit(m)) {
			wlr_scene_output_commit(m->scene_output, NULL);
		}
	}

	frame_clock_now_timespec(&now);
	wlr_scene_output_send_frame_done(m->scene_output, &now);

	if (LEMON_UNLIKELY(need_more_frames && allow_frame_scheduling))
		schedule_next_frame(m);

	if (LEMON_UNLIKELY(timed)) {
		struct timespec t1;
		clock_gettime(CLOCK_MONOTONIC, &t1);
		int64_t us = (t1.tv_sec - t0.tv_sec) * 1000000 +
					 (t1.tv_nsec - t0.tv_nsec) / 1000;
		/* Asymmetric EMA: react fast (1/2 weight) when the frame got
		   heavier so the late-latch deadline never overshoots into the
		   next vblank, decay slowly (1/16) when frames get cheaper so a
		   single light frame can't shrink the safety margin. Feeds the
		   render_deadline computation in rendermon. */
		if ((int64_t)us > (int64_t)m->render_ema_us)
			m->render_ema_us = (uint32_t)((m->render_ema_us + us) / 2);
		else
			m->render_ema_us = (uint32_t)((m->render_ema_us * 15 + us) / 16);
		if (LEMON_UNLIKELY(config.debug_frametime)) {
			int64_t budget_us = m->wlr_output->refresh > 0
									? 1000000000LL / m->wlr_output->refresh
									: 16666;
			if (us > budget_us)
				wlr_log(WLR_INFO, "%s: frame %lldus over %lldus budget",
						m->wlr_output->name, (long long)us, (long long)budget_us);
		}
	}

	frame_clock_end();
}

/* Late-latch deadline fired: render now, just before the next vblank. */
static int render_deadline_callback(void *data) {
	Monitor *m = data;
	m->render_pending = false;
	if (LEMON_UNLIKELY(session && !session->active))
		return 0;
	if (LEMON_UNLIKELY(!m->wlr_output->enabled || !allow_frame_scheduling))
		return 0;
	do_rendermon(m);
	return 0;
}

/* per-monitor frame callback: render immediately, or defer to just
   before the next vblank when late input latching is enabled
   (config-gated, default off). */
LEMON_HOT void rendermon(struct wl_listener *listener, void *data) {
	Monitor *m = wl_container_of(listener, m, frame);

	if (LEMON_UNLIKELY(session && !session->active))
		return;

	if (LEMON_UNLIKELY(!m->wlr_output->enabled || !allow_frame_scheduling))
		return;

	if (LEMON_UNLIKELY(config.late_latch && !config.allow_tearing &&
					   m->render_deadline && !m->render_pending &&
					   m->wlr_output->refresh > 0)) {
		int64_t period_us = 1000000000LL / m->wlr_output->refresh;
		int64_t delay_us =
			period_us - (int64_t)m->render_ema_us - config.latency_margin_us;
		if (delay_us > 1000) {
			m->render_pending = true;
			wl_event_source_timer_update(m->render_deadline,
										 (uint32_t)(delay_us / 1000));
			return;
		}
	}

	do_rendermon(m);
}
