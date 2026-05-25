#pragma once

/* Pointer-button listener. Heavy code path:
   - eats clicks while the alt-tab cycler or the clipboard popup is
     open (forward to their pick / dismiss paths);
   - refocuses the underlying client on press, runs mouse bindings;
   - on release of a drag/resize, hands the accumulated drag velocity
     to the target client's spring as a fling seed.
   Single-TU header included once from lemon.c. */

/* Listener: pointer button - refocus client, run mouse bindings, end drag/resize on release. */
LEMON_HOT void
buttonpress(struct wl_listener *listener, void *data) {
	struct wlr_pointer_button_event *event = data;
	struct wlr_keyboard *hard_keyboard, *keyboard;
	uint32_t hard_mods, mods;
	Client *c = NULL;
	LayerSurface *l = NULL;
	struct wlr_surface *surface;
	Client *tmpc = NULL;
	int32_t ji;
	const MouseBinding *m;
	struct wlr_surface *old_pointer_focus_surface =
		seat->pointer_state.focused_surface;

	handlecursoractivity();
	idle_notify_throttled(false);

	if (check_trackpad_disabled(event->pointer)) {
		return;
	}

	switch (event->state) {
	case WL_POINTER_BUTTON_STATE_PRESSED:
		cursor_mode = CurPressed;
		selmon = xytomon(cursor->x, cursor->y);
		if (locked)
			break;

		/* Window cycler eats clicks while open: left-click on a thumb
		   picks that window and commits; right-click closes it
		   in-place (cycler rebuilds). Click outside any thumb closes
		   the overlay without committing. */
		if (window_cycler.active && window_cycler.tiles) {
			int32_t hit = -1;
			for (int32_t k = 0; k < window_cycler.count; k++) {
				struct wlr_scene_rect *tile = window_cycler.tiles[k];
				if (!tile)
					continue;
				int32_t tx = tile->node.x;
				int32_t ty = tile->node.y;
				int32_t tw = tile->width;
				int32_t th = tile->height;
				if (cursor->x >= tx && cursor->x < tx + tw &&
					cursor->y >= ty && cursor->y < ty + th) {
					hit = k;
					break;
				}
			}
			/* Click outside any tile: swallow it (the overlay must
			   only ever close on ALT release or a left-click pick) so
			   the click can't leak through to whatever sits behind. */
			if (hit < 0) {
				cursor_mode = CurNormal;
				return;
			}
			if (event->button == BTN_LEFT) {
				window_cycler.index = hit;
				window_cycler_commit();
				cursor_mode = CurNormal;
				return;
			}
			if (event->button == BTN_RIGHT) {
				/* Right-click closes the hit window in place and
				   rebuilds the strip from whatever is left. The
				   overlay only goes away once the user releases ALT
				   or left-picks a tile, so a missed selection doesn't
				   force them to re-trigger Alt+Tab. */
				Client *victim = window_cycler.clients[hit];
				Monitor *m = window_cycler.mon;
				window_cycler_destroy();
				if (victim && !victim->iskilling)
					pending_kill_client(victim);
				if (m) {
					int32_t built = window_cycler_build(m);
					if (built > 0)
						window_cycler_hover_at(cursor->x, cursor->y);
				}
				cursor_mode = CurNormal;
				return;
			}
			cursor_mode = CurNormal;
			return;
		}

		/* Clipboard popup eats left clicks: a click inside a row picks
		   the entry (auto-paste); a click outside dismisses the popup
		   without touching client focus. Both consume the event so the
		   underlying window never sees it. */
		if (clipboard.popup_open && clipboard.popup_tree) {
			int32_t px = clipboard.popup_tree->node.x;
			int32_t py = clipboard.popup_tree->node.y;
			int32_t cx = (int32_t)cursor->x - px;
			int32_t cy = (int32_t)cursor->y - py;
			if (cx >= 0 && cx < clipboard.popup_w && cy >= 0 &&
				cy < clipboard.popup_h) {
				int32_t row = (cy - CLIP_POPUP_PAD) / CLIP_ROW_H;
				if (row >= 0 && row < (int32_t)clipboard.rows_count &&
					event->button == BTN_LEFT) {
					clipboard.selected = row;
					clip_popup_pick();
				}
				cursor_mode = CurNormal;
				return;
			} else {
				clip_popup_close();
				cursor_mode = CurNormal;
				return;
			}
		}

		xytonode(cursor->x, cursor->y, &surface, NULL, NULL, NULL, NULL);
		if (toplevel_from_wlr_surface(surface, &c, &l) >= 0) {
			if (c && c->scene->node.enabled &&
				(!client_is_unmanaged(c) || client_wants_focus(c)))
				focusclient(c, 1);

			if (surface != old_pointer_focus_surface) {
				wlr_seat_pointer_notify_clear_focus(seat);
				motionnotify(0, NULL, 0, 0, 0, 0);
			}

			if (l && !exclusive_focus &&
				l->layer_surface->current.keyboard_interactive ==
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND) {
				focuslayer(l);
			}
		}

		hard_keyboard = &kb_group->wlr_group->keyboard;
		hard_mods =
			hard_keyboard ? wlr_keyboard_get_modifiers(hard_keyboard) : 0;

		keyboard = wlr_seat_get_keyboard(seat);
		mods = keyboard && !l ? wlr_keyboard_get_modifiers(keyboard) : 0;

		mods = mods | hard_mods;

		for (ji = 0; ji < config.mouse_bindings_count; ji++) {
			if (config.mouse_bindings_count < 1)
				break;
			m = &config.mouse_bindings[ji];

			if (selmon->isoverview && event->button == BTN_LEFT && c) {
				toggleoverview(&(Arg){.i = 1});
				return;
			}

			if (selmon->isoverview && event->button == BTN_RIGHT && c) {
				pending_kill_client(c);
				return;
			}

			if (CLEANMASK(mods) == CLEANMASK(m->mod) &&
				event->button == m->button && m->func &&
				(CLEANMASK(m->mod) != 0 ||
				 (event->button != BTN_LEFT && event->button != BTN_RIGHT))) {
				m->func(&m->arg);
				return;
			}
		}
		break;
	case WL_POINTER_BUTTON_STATE_RELEASED:

		if (!locked && cursor_mode != CurNormal && cursor_mode != CurPressed) {
			uint32_t release_mode = cursor_mode;
			cursor_mode = CurNormal;

			wlr_seat_pointer_clear_focus(seat);
			motionnotify(0, NULL, 0, 0, 0, 0);

			if (grabc == selmon->sel) {
				selmon->sel = NULL;
			}
			selmon = xytomon(cursor->x, cursor->y);
			client_update_oldmonname_record(grabc, selmon);
			setmon(grabc, selmon, 0, true);
			selmon->prevsel = ISTILED(selmon->sel) ? selmon->sel : NULL;
			selmon->sel = grabc;
			tmpc = grabc;
			grabc = NULL;
			start_drag_window = false;
			last_apply_drap_time = 0;
			if (tmpc->drag_to_tile && config.drag_tile_to_tile) {
				place_drag_tile_client(tmpc);
				tmpc->float_geom = tmpc->drag_tile_float_backup_geom;
			} else {
				apply_window_snap(tmpc);
			}
			tmpc->drag_to_tile = false;
			/* Hand the drag's momentum to the window's spring so it flies to
			   its tiled/snapped target and settles, instead of snapping flat.
			   Consumed on the next spring tick (see spring_init seeding). */
			if (config.animation_momentum && tmpc) {
				double sc = config.animation_momentum_scale;
				if (release_mode == CurResize) {
					tmpc->animation.vel_seed[2] = drag_vel_x * sc;
					tmpc->animation.vel_seed[3] = drag_vel_y * sc;
				} else {
					tmpc->animation.vel_seed[0] = drag_vel_x * sc;
					tmpc->animation.vel_seed[1] = drag_vel_y * sc;
				}
			}
			drag_vel_x = drag_vel_y = 0.0;
			drag_last_ms = 0;
			if (dropc) {
				dropc->enable_drop_area_draw = false;
				client_set_drop_area(dropc);
				dropc = NULL;
			}
			return;
		} else {
			cursor_mode = CurNormal;
		}
		break;
	}

	wlr_seat_pointer_notify_button(seat, event->time_msec, event->button,
								   event->state);
}
