#pragma once

/* Idle action + pre-idle backlight dimming + per-bind idle timers.
   The built-in idle_action_callback fires after idle_timeout (DPMS /
   suspend / hibernate). The pre-idle dim path springs the backlight
   down to a configurable floor a few seconds before the full timeout
   so the impending action is signalled visually, then springs it back
   on any user activity. Single-TU header included once. */

/* Built-in action after idle_timeout: blank screen, suspend, or hibernate.
   Suspended while the session is inactive or a client inhibits idle. */
int32_t idle_action_callback(void *data) {
	if (session && !session->active)
		return 0;
	if (idle_inhibited) {
		wl_event_source_timer_update(idle_action_source,
									 idle_action_timeout_ms());
		return 0;
	}
	switch (config.idle_action) {
	case IDLE_ACTION_SUSPEND:
		spawn_shell(&(Arg){.v = "systemctl suspend"});
		break;
	case IDLE_ACTION_HIBERNATE:
		spawn_shell(&(Arg){.v = "systemctl hibernate"});
		break;
	case IDLE_ACTION_OFF:
	default:
		idle_set_screens(true);
		break;
	}
	return 0;
}

/* ---- Smooth pre-idle backlight dimming (driven by the spring integrator) ---- */
static struct wl_event_source *predim_lead_source = NULL; /* arms the fade */
static struct wl_event_source *predim_anim_source = NULL; /* spring ticker */
static bool predim_active = false;	   /* fade in progress / holding floor */
static int32_t predim_saved_pct = -1;  /* brightness percent before dimming */
static double predim_vis = 0.0;		   /* spring position, in percent */
static double predim_vel = 0.0;
static double predim_target = 0.0;
static uint32_t predim_last_ms = 0;

/* Read the first backlight's brightness as a 1..100 percent (0 on failure). */
static int32_t backlight_read_pct(void) {
	DIR *d = opendir("/sys/class/backlight");
	if (!d)
		return 0;
	struct dirent *e;
	char path[512];
	int32_t pct = 0;
	while ((e = readdir(d))) {
		if (e->d_name[0] == '.')
			continue;
		long cur = -1, max = -1;
		FILE *f;
		snprintf(path, sizeof(path), "/sys/class/backlight/%s/brightness",
				 e->d_name);
		if ((f = fopen(path, "r"))) {
			if (fscanf(f, "%ld", &cur) != 1)
				cur = -1;
			fclose(f);
		}
		snprintf(path, sizeof(path), "/sys/class/backlight/%s/max_brightness",
				 e->d_name);
		if ((f = fopen(path, "r"))) {
			if (fscanf(f, "%ld", &max) != 1)
				max = -1;
			fclose(f);
		}
		if (cur >= 0 && max > 0) {
			pct = (int32_t)((cur * 100 + max / 2) / max);
			break;
		}
	}
	closedir(d);
	return pct;
}

/* Apply a brightness percent via brightnessctl (reliable sysfs write). */
static void backlight_set_pct(int32_t pct) {
	if (pct < 1)
		pct = 1;
	if (pct > 100)
		pct = 100;
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "brightnessctl set %d%%", pct);
	spawn_shell(&(Arg){.v = cmd});
}

/* Spring-tick the backlight toward predim_target; stop once settled. */
int32_t predim_anim_tick(void *data) {
	uint32_t now = frame_now_ms();
	double dt =
		predim_last_ms ? (double)(now - predim_last_ms) / 1000.0 : 1.0 / 30.0;
	if (dt > SPRING_MAX_DT)
		dt = SPRING_MAX_DT;
	if (dt <= 0.0)
		dt = 1.0 / 1000.0;
	predim_last_ms = now;

	predim_vis = spring_axis_step(predim_vis, &predim_vel, predim_target, dt,
								  1.0, 120.0, 16.0);
	backlight_set_pct((int32_t)(predim_vis + 0.5));

	if (fabs(predim_vis - predim_target) < 0.6 && fabs(predim_vel) < 1.0) {
		backlight_set_pct((int32_t)(predim_target + 0.5));
		predim_last_ms = 0;
		return 0; /* settled: stop ticking */
	}
	if (predim_anim_source)
		wl_event_source_timer_update(predim_anim_source, 30);
	return 0;
}

/* Lead timer fired: capture current brightness and spring it down to floor. */
int32_t predim_lead_callback(void *data) {
	if ((session && !session->active) || idle_inhibited)
		return 0;
	predim_saved_pct = backlight_read_pct();
	if (predim_saved_pct <= 0)
		return 0; /* no backlight present */
	predim_active = true;
	predim_vis = predim_saved_pct;
	predim_vel = 0.0;
	predim_target = config.pre_idle_dim_floor;
	predim_last_ms = 0;
	if (predim_anim_source)
		wl_event_source_timer_update(predim_anim_source, 1);
	return 0;
}

/* On activity: spring brightness elastically back to its pre-dim value and
   re-arm the lead timer. No-op when not dimming. */
static void predim_restore_and_rearm(void) {
	if (predim_active && predim_saved_pct > 0) {
		predim_target = predim_saved_pct;
		predim_last_ms = 0;
		if (predim_anim_source)
			wl_event_source_timer_update(predim_anim_source, 1);
		predim_active = false;
	}
	if (predim_lead_source) {
		int32_t lead = config.idle_timeout - config.pre_idle_dim_lead;
		if (lead > 0)
			wl_event_source_timer_update(predim_lead_source, lead * 1000);
	}
}

/* Fire one idle binding's action once its timeout elapses with no activity.
   Skips while the session is inactive or a client inhibits idle, re-arming in
   the inhibit case so it can still fire once the inhibit clears. */
int32_t idle_timer_callback(void *data) {
	struct IdleTimer *t = data;
	if (session && !session->active)
		return 0;
	if (idle_inhibited) {
		wl_event_source_timer_update(t->source, t->binding->timeout_ms);
		return 0;
	}
	if (t->binding->func)
		t->binding->func(&t->binding->arg);
	return 0;
}

/* Re-arm every idle timer to its full timeout; called on user activity.
   Also wakes blanked screens and re-arms the built-in idle action. */
void reset_idle_timers(void) {
	if (idle_screen_off)
		idle_set_screens(false);
	predim_restore_and_rearm();
	for (int32_t i = 0; i < idle_timers_count; i++)
		wl_event_source_timer_update(idle_timers[i].source,
									 idle_timers[i].binding->timeout_ms);
	if (idle_action_source)
		wl_event_source_timer_update(idle_action_source,
									 idle_action_timeout_ms());
}

/* Tear down all idle timer event sources. */
void destroy_idle_timers(void) {
	if (idle_action_source) {
		wl_event_source_remove(idle_action_source);
		idle_action_source = NULL;
	}
	if (predim_lead_source) {
		wl_event_source_remove(predim_lead_source);
		predim_lead_source = NULL;
	}
	if (predim_anim_source) {
		wl_event_source_remove(predim_anim_source);
		predim_anim_source = NULL;
	}
	predim_active = false;
	predim_saved_pct = -1;
	if (!idle_timers)
		return;
	for (int32_t i = 0; i < idle_timers_count; i++) {
		if (idle_timers[i].source)
			wl_event_source_remove(idle_timers[i].source);
	}
	free(idle_timers);
	idle_timers = NULL;
	idle_timers_count = 0;
}

/* (Re)create armed event-loop timers: the built-in idle action plus one per
   configured idlebind. Called at setup and on config hot-reload. */
void setup_idle_timers(void) {
	destroy_idle_timers();

	if (config.idle_timeout > 0) {
		idle_action_source =
			wl_event_loop_add_timer(event_loop, idle_action_callback, NULL);
		if (idle_action_source)
			wl_event_source_timer_update(idle_action_source,
										 idle_action_timeout_ms());
	}

	/* Pre-idle dimming: a disarmed spring ticker plus a lead timer fired
	   pre_idle_dim_lead seconds before the full idle timeout. */
	if (config.pre_idle_dim && config.idle_timeout > config.pre_idle_dim_lead) {
		predim_anim_source =
			wl_event_loop_add_timer(event_loop, predim_anim_tick, NULL);
		predim_lead_source =
			wl_event_loop_add_timer(event_loop, predim_lead_callback, NULL);
		if (predim_lead_source)
			wl_event_source_timer_update(
				predim_lead_source,
				(config.idle_timeout - config.pre_idle_dim_lead) * 1000);
	}

	if (config.idle_bindings_count <= 0)
		return;
	idle_timers = ecalloc(config.idle_bindings_count, sizeof(*idle_timers));
	for (int32_t i = 0; i < config.idle_bindings_count; i++) {
		idle_timers[i].binding = &config.idle_bindings[i];
		idle_timers[i].source = wl_event_loop_add_timer(
			event_loop, idle_timer_callback, &idle_timers[i]);
		if (idle_timers[i].source)
			wl_event_source_timer_update(idle_timers[i].source,
										 config.idle_bindings[i].timeout_ms);
	}
	idle_timers_count = config.idle_bindings_count;
}
