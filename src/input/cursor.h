#pragma once

/* Cursor activity gate + idle-inhibit keepalive. Splits the
   idle-notifier 250 ms throttle, the cursor-hide timer reset and the
   keep-idle-inhibit timer out of lemon.c. Single-TU header included
   once. */

/* Rate-limited wrapper around wlr_idle_notifier_v1_notify_activity.
   Every input path (motion, axis, keypress, swipe, set_selection) was
   calling the notifier directly, flooding the idle-notify D-Bus signal
   under high-poll mice or fast typists. Sharing a single 250 ms gate
   bounds the bus chatter without measurably delaying idle-watchers.
   force=true ignores the gate (used for cursor hide reset). */
static void idle_notify_throttled(bool force) {
	static uint32_t last_ms = 0;
	uint32_t now = (uint32_t)get_now_in_ms();
	if (!force && now - last_ms < 250)
		return;
	last_ms = now;
	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);
}

void handlecursoractivity(void) {
	reset_idle_timers();
	wl_event_source_timer_update(hide_cursor_source,
								 config.cursor_hide_timeout * 1000);

	if (!cursor_hidden)
		return;

	cursor_hidden = false;

	if (last_cursor.shape)
		wlr_cursor_set_xcursor(cursor, cursor_mgr,
							   wlr_cursor_shape_v1_name(last_cursor.shape));
	else if (last_cursor.surface)
		wlr_cursor_set_surface(cursor, last_cursor.surface,
							   last_cursor.hotspot_x, last_cursor.hotspot_y);
}

int32_t hidecursor(void *data) {
	wlr_cursor_unset_image(cursor);
	cursor_hidden = true;
	return 1;
}

void check_keep_idle_inhibit(Client *c) {
	if (c && c->indleinhibit_when_focus && keep_idle_inhibit_source) {
		wl_event_source_timer_update(keep_idle_inhibit_source, 1000);
	}
}

int32_t keep_idle_inhibit(void *data) {

	if (!idle_inhibit_mgr) {
		wl_event_source_timer_update(keep_idle_inhibit_source, 0);
		return 1;
	}

	if (session && !session->active) {
		wl_event_source_timer_update(keep_idle_inhibit_source, 0);
		return 1;
	}

	if (!selmon || !selmon->sel || !selmon->sel->indleinhibit_when_focus) {
		wl_event_source_timer_update(keep_idle_inhibit_source, 0);
		return 1;
	}

	if (seat && idle_notifier) {
		wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);
		reset_idle_timers();
		wl_event_source_timer_update(keep_idle_inhibit_source, 1000);
	}
	return 1;
}
