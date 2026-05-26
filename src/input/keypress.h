#pragma once

/* Keyboard key + modifier listeners. Heavy code path: cycler / clipboard
   overlay key interception, idle-notify gate, configured cycler
   modifier release commit, then the normal keybinding dispatch with
   cycler-suppression. Single TU pattern; included exactly once. */

LEMON_HOT void keypress(struct wl_listener *listener, void *data) {
	int32_t i;

	KeyboardGroup *group = wl_container_of(listener, group, key);
	struct wlr_keyboard_key_event *event = data;

	struct wlr_surface *last_surface = seat->keyboard_state.focused_surface;
	struct wlr_xdg_surface *xdg_surface =
		last_surface ? wlr_xdg_surface_try_from_wlr_surface(last_surface)
					 : NULL;
	int32_t pass = 0;
	bool hit_global = false;
#ifdef XWAYLAND
	struct wlr_xwayland_surface *xsurface =
		last_surface ? wlr_xwayland_surface_try_from_wlr_surface(last_surface)
					 : NULL;
#endif

	uint32_t keycode = event->keycode + 8;

	const xkb_keysym_t *syms;
	int32_t nsyms = xkb_state_key_get_syms(group->wlr_group->keyboard.xkb_state,
										   keycode, &syms);

	int32_t handled = 0;
	uint32_t mods = wlr_keyboard_get_modifiers(&group->wlr_group->keyboard);

	/* Notify only when no cycler-style overlay swallowed the key; the
	   cycler dismiss path already reset idle timers via the modifier
	   release. Throttled to 250 ms anyway. */
	if (!window_cycler.active)
		idle_notify_throttled(false);
	reset_idle_timers();

	if (config.ov_tab_mode && !locked && group == kb_group &&
		event->state == WL_KEYBOARD_KEY_STATE_RELEASED &&
		(keycode == 133 || keycode == 37 || keycode == 64 || keycode == 50 ||
		 keycode == 134 || keycode == 105 || keycode == 108 || keycode == 62) &&
		selmon && selmon->sel) {
		if (selmon->isoverview && selmon->sel) {
			toggleoverview(&(Arg){.i = 1});
		}
	}

	/* Commit window cycler selection on the configured modifier's
	   release. cycler_modifier=0 -> Alt (keycodes 64/108);
	   cycler_modifier=1 -> Super (keycodes 133/134). The cycler is
	   opened by window_cycler_next/prev and held visible while the
	   modifier stays down. */
	uint32_t cycler_kc_l = (config.cycler_modifier == 1) ? 133 : 64;
	uint32_t cycler_kc_r = (config.cycler_modifier == 1) ? 134 : 108;
	uint32_t cycler_mod_bit = (config.cycler_modifier == 1)
								  ? WLR_MODIFIER_LOGO
								  : WLR_MODIFIER_ALT;
	if (window_cycler.active && !locked && group == kb_group &&
		event->state == WL_KEYBOARD_KEY_STATE_RELEASED &&
		(keycode == cycler_kc_l || keycode == cycler_kc_r)) {
		/* Sticky cycler: a non-cycler bind fired during this session
		   (typically a launcher like Alt+E). Don't commit on modifier
		   release -- the cycler stays open so the freshly-spawned
		   window has time to map and join the grid (see add_client in
		   mapnotify). User dismisses with Escape, a click on a cell,
		   or Alt+digit jump. */
		if (!window_cycler.sticky)
			window_cycler_commit();
	}

	/* While the cycler is open: digit 1..9 jumps directly to that
	   thumbnail when no Shift is held; with Shift, the hovered (or
	   selected) window is moved to workspace N instead and the
	   cycler stays open with the moved client gone. Arrow keys step
	   the selection through the grid. All this only when the cycler
	   modifier is held; a stray '4' inside an app after the modifier
	   has been released must not commit. */
	if (window_cycler.active && !locked && group == kb_group &&
		event->state == WL_KEYBOARD_KEY_STATE_PRESSED &&
		(mods & cycler_mod_bit)) {
		/* Arrow nav. */
		bool arrow_hit = false;
		for (i = 0; i < nsyms; i++) {
			switch (syms[i]) {
			case XKB_KEY_Left:
				window_cycler_step_grid(-1, 0);
				arrow_hit = true;
				break;
			case XKB_KEY_Right:
				window_cycler_step_grid(1, 0);
				arrow_hit = true;
				break;
			case XKB_KEY_Up:
				window_cycler_step_grid(0, -1);
				arrow_hit = true;
				break;
			case XKB_KEY_Down:
				window_cycler_step_grid(0, 1);
				arrow_hit = true;
				break;
			default:
				break;
			}
			if (arrow_hit)
				break;
		}
		if (arrow_hit) {
			group->nsyms = 0;
			wl_event_source_timer_update(group->key_repeat_source, 0);
			return;
		}
		/* Digit detection. Use the raw evdev/X keycode rather than the
		   keysym so the lookup is layout-independent: with Shift
		   held on QWERTY (US) the keysym becomes `!`, `@`, ... not
		   `1`, `2`, so a keysym-based check would silently fail and
		   the press would leak to the normal keybinding dispatch
		   (where the user's `Alt+Shift+1 = tag 1` bind would fire on
		   the focused client, not the cycler target). X keycodes
		   10..18 are digits 1..9 in the standard layout. Keypad
		   digits go through the keysym path because they share no
		   common keycode block across keyboards. */
		int32_t digit = -1;
		if (keycode >= 10 && keycode <= 18)
			digit = keycode - 10;
		if (digit < 0) {
			for (i = 0; i < nsyms; i++) {
				xkb_keysym_t s = syms[i];
				if (s >= XKB_KEY_KP_1 && s <= XKB_KEY_KP_9) {
					digit = s - XKB_KEY_KP_1;
					break;
				}
			}
		}
		if (digit >= 0) {
			if (mods & WLR_MODIFIER_SHIFT) {
				/* Alt+Shift+N: move hovered/selected client to tag N
				   without dismissing the cycler. Bounded to the
				   compositor's tag count via TAGMASK. */
				uint32_t mask = (1u << digit) & TAGMASK;
				if (mask) {
					int32_t tgt = window_cycler_active_target();
					window_cycler_move_to_tag(tgt, mask);
				}
				group->nsyms = 0;
				wl_event_source_timer_update(group->key_repeat_source, 0);
				return;
			}
			if (digit < window_cycler.count) {
				window_cycler.index = digit;
				window_cycler_commit();
				group->nsyms = 0;
				wl_event_source_timer_update(group->key_repeat_source, 0);
				return;
			}
		}
	}

	/* Escape dismisses the cycler in place: no focus change, prior
	   minimized / hidden-tag clients revert. */
	if (window_cycler.active && !locked && group == kb_group &&
		event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		bool esc_hit = false;
		for (i = 0; i < nsyms; i++) {
			if (syms[i] == XKB_KEY_Escape) {
				esc_hit = true;
				break;
			}
		}
		if (esc_hit) {
			window_cycler_destroy();
			group->nsyms = 0;
			wl_event_source_timer_update(group->key_repeat_source, 0);
			return;
		}
	}

	/* While the cycler is open, Alt+Q closes the highlighted window
	   in-place: the user keeps Alt held, the overlay rebuilds without
	   the killed entry, and they can keep tabbing through the rest.
	   Captured before the normal keybinding pass so the regular
	   killclient bind doesn't also fire on the underlying window. */
	if (window_cycler.active && !locked && group == kb_group &&
		event->state == WL_KEYBOARD_KEY_STATE_PRESSED &&
		(mods & WLR_MODIFIER_ALT)) {
		bool kill_hit = false;
		for (i = 0; i < nsyms; i++) {
			if (syms[i] == XKB_KEY_q || syms[i] == XKB_KEY_Q) {
				kill_hit = true;
				break;
			}
		}
		if (kill_hit) {
			Client *target = NULL;
			if (window_cycler.index >= 0 &&
				window_cycler.index < window_cycler.count)
				target = window_cycler.clients[window_cycler.index];
			Monitor *m = window_cycler.mon;
			window_cycler_destroy();
			if (target && !target->iskilling)
				pending_kill_client(target);
			if (m && window_cycler_build(m) <= 1)
				window_cycler_destroy();
			group->nsyms = 0;
			wl_event_source_timer_update(group->key_repeat_source, 0);
			return;
		}
	}

	if (clipboard.popup_open && !locked && group == kb_group &&
		event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (i = 0; i < nsyms; i++) {
			switch (syms[i]) {
			case XKB_KEY_Down:
			case XKB_KEY_Tab:
				clip_popup_move(1);
				handled = 1;
				break;
			case XKB_KEY_Up:
			case XKB_KEY_ISO_Left_Tab:
				clip_popup_move(-1);
				handled = 1;
				break;
			case XKB_KEY_Page_Down:
				clip_popup_move(5);
				handled = 1;
				break;
			case XKB_KEY_Page_Up:
				clip_popup_move(-5);
				handled = 1;
				break;
			case XKB_KEY_Home:
				clip_popup_move(-1000000);
				handled = 1;
				break;
			case XKB_KEY_End:
				clip_popup_move(1000000);
				handled = 1;
				break;
			case XKB_KEY_Return:
			case XKB_KEY_KP_Enter:
				clip_popup_pick();
				handled = 1;
				break;
			case XKB_KEY_Escape:
				clip_popup_close();
				handled = 1;
				break;
			default:
				break;
			}
		}
		if (handled) {
			group->nsyms = 0;
			wl_event_source_timer_update(group->key_repeat_source, 0);
			return;
		}
	}

	/* While the cycler is open, only Tab / Shift+Tab (step) and the
	   four arrow keys (grid nav) reach keybinding(). Everything else
	   is silently swallowed so user binds (resize, tile,
	   togglefloating, workspace switch, launchers, ...) cannot fire
	   under the cycler -- matches the overview behaviour. Digit
	   jumps, Escape, Alt+Q kill and Alt+Shift+digit move-to-tag
	   were already handled and returned above. Key-repeat is
	   cancelled too: keyrepeat() bypasses this check and would
	   otherwise re-trigger a swallowed bind on the next tick. */
	bool cycler_ate = false;
	for (i = 0; i < nsyms; i++) {
		if (window_cycler.active && !locked) {
			xkb_keysym_t s = syms[i];
			bool nav_key =
				s == XKB_KEY_Tab || s == XKB_KEY_ISO_Left_Tab ||
				s == XKB_KEY_Left || s == XKB_KEY_Right ||
				s == XKB_KEY_Up || s == XKB_KEY_Down;
			if (!nav_key) {
				handled = 1;
				cycler_ate = true;
				continue;
			}
		}
		handled =
			keybinding(event->state, locked, mods, syms[i], keycode) || handled;
	}
	if (cycler_ate) {
		group->nsyms = 0;
		wl_event_source_timer_update(group->key_repeat_source, 0);
		return;
	}

	if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
		tag_combo = false;
	}

	if (handled && group->wlr_group->keyboard.repeat_info.delay > 0) {
		group->mods = mods;
		group->keysyms = syms;
		group->keycode = keycode;
		group->nsyms = nsyms;
		wl_event_source_timer_update(
			group->key_repeat_source,
			group->wlr_group->keyboard.repeat_info.delay);
	} else {
		group->nsyms = 0;
		wl_event_source_timer_update(group->key_repeat_source, 0);
	}

	if (handled)
		return;

	pass = (xdg_surface && xdg_surface->role != WLR_XDG_SURFACE_ROLE_POPUP) ||
		   !last_surface
#ifdef XWAYLAND
		   || xsurface
#endif
		;

	if (pass && syms)
		hit_global = keypressglobal(last_surface, &group->wlr_group->keyboard,
									event, mods, syms[0], keycode);

	if (hit_global) {
		return;
	}
	if (!dwl_im_keyboard_grab_forward_key(group, event)) {
		wlr_seat_set_keyboard(seat, &group->wlr_group->keyboard);

		wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode,
									 event->state);
	}
}

void keypressmod(struct wl_listener *listener, void *data) {

	KeyboardGroup *group = wl_container_of(listener, group, modifiers);

	if (!dwl_im_keyboard_grab_forward_modifiers(group)) {

		wlr_seat_set_keyboard(seat, &group->wlr_group->keyboard);

		wlr_seat_keyboard_notify_modifiers(
			seat, &group->wlr_group->keyboard.modifiers);
	}
}
