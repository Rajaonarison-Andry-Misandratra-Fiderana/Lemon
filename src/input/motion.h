#pragma once

/* Pointer-motion input path: absolute / relative cursor events and the
   shared motionnotify dispatcher. Resize-floating helper lives here
   too since it is only called from motionnotify's resize branch. */

LEMON_HOT void motionabsolute(struct wl_listener *listener, void *data) {

	struct wlr_pointer_motion_absolute_event *event = data;
	double lx, ly, dx, dy;

	if (check_trackpad_disabled(event->pointer)) {
		return;
	}

	if (!event->time_msec)
		wlr_cursor_warp_absolute(cursor, &event->pointer->base, event->x,
								 event->y);

	wlr_cursor_absolute_to_layout_coords(cursor, &event->pointer->base,
										 event->x, event->y, &lx, &ly);
	dx = lx - cursor->x;
	dy = ly - cursor->y;
	motionnotify(event->time_msec, &event->pointer->base, dx, dy, dx, dy);
}

void resize_floating_window(Client *grabc) {
	int cdx = (int)round(cursor->x) - grabcx;
	int cdy = (int)round(cursor->y) - grabcy;

	cdx = !(rzcorner & 1) && grabc->geom.width - 2 * (int)grabc->bw - cdx < 1
			  ? 0
			  : cdx;
	cdy = !(rzcorner & 2) && grabc->geom.height - 2 * (int)grabc->bw - cdy < 1
			  ? 0
			  : cdy;

	const struct wlr_box box = {
		.x = grabc->geom.x + (rzcorner & 1 ? 0 : cdx),
		.y = grabc->geom.y + (rzcorner & 2 ? 0 : cdy),
		.width = grabc->geom.width + (rzcorner & 1 ? cdx : -cdx),
		.height = grabc->geom.height + (rzcorner & 2 ? cdy : -cdy)};

	grabc->float_geom = box;

	resize(grabc, box, 1);
	grabcx += cdx;
	grabcy += cdy;
}

LEMON_HOT void motionnotify(uint32_t time, struct wlr_input_device *device, double dx,
				  double dy, double dx_unaccel, double dy_unaccel) {
	double sx = 0, sy = 0, sx_confined, sy_confined;
	Client *c = NULL, *w = NULL;
	Client *closet_drop_client = NULL;
	LayerSurface *l = NULL;
	struct wlr_surface *surface = NULL;
	bool should_lock = false;

	if (time) {
		wlr_relative_pointer_manager_v1_send_relative_motion(
			relative_pointer_mgr, seat, (uint64_t)time * 1000, dx, dy,
			dx_unaccel, dy_unaccel);

		if (active_constraint && cursor_mode != CurResize &&
			cursor_mode != CurMove) {
			if (active_constraint->surface ==
				seat->pointer_state.focused_surface) {

				if (active_constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED)
					return;

				toplevel_from_wlr_surface(active_constraint->surface, &c, NULL);
				if (c) {
					sx = cursor->x - c->geom.x - c->bw;
					sy = cursor->y - c->geom.y - c->bw;
					if (wlr_region_confine(&active_constraint->region, sx, sy,
										   sx + dx, sy + dy, &sx_confined,
										   &sy_confined)) {
						dx = sx_confined - sx;
						dy = sy_confined - sy;
					}
				}
			}
		}

		wlr_cursor_move(cursor, device, dx, dy);
		handlecursoractivity();

		/* Cycler eats motion: drag the held tile if one is being
		   moved, otherwise just update the hover index. Bail out
		   before any sloppy-focus / pointer routing fires (the
		   overlay sits on LyrFadeOut which xytonode skips, so
		   otherwise the cursor would silently re-focus whatever sits
		   under the grid). */
		if (window_cycler.active) {
			if (window_cycler.drag_idx >= 0) {
				window_cycler_drag_motion(cursor->x, cursor->y);
			} else {
				window_cycler_hover_at(cursor->x, cursor->y);
			}
			return;
		}

		/* Track pointer velocity (EMA) while dragging, for the momentum
		   hand-off on release. After a brief pause we now exponentially
		   decay the velocity instead of hard-resetting it -- a natural
		   beat between two flicks no longer wipes the fling intent. */
		if (config.animation_momentum &&
			(cursor_mode == CurMove || cursor_mode == CurResize)) {
			uint32_t vnow = frame_now_ms();
			double vdt = drag_last_ms ? (double)(vnow - drag_last_ms) / 1000.0 : 0;
			if (vdt > 0.001 && vdt < 0.1) {
				double ivx = (cursor->x - drag_last_x) / vdt;
				double ivy = (cursor->y - drag_last_y) / vdt;
				drag_vel_x = drag_vel_x * 0.6 + ivx * 0.4;
				drag_vel_y = drag_vel_y * 0.6 + ivy * 0.4;
			} else if (vdt >= 0.1) {
				/* Gap >= 100 ms: decay with ~200 ms half-life so a held
				   pause forgets stale velocity within a few hundred
				   millis without snapping it to zero on the next event. */
				double decay = exp(-vdt / 0.2);
				drag_vel_x *= decay;
				drag_vel_y *= decay;
			}
			drag_last_x = cursor->x;
			drag_last_y = cursor->y;
			drag_last_ms = vnow;
		}

		idle_notify_throttled(false);

		if (config.sloppyfocus) {
			if (!selmon ||
			    cursor->x < selmon->m.x ||
			    cursor->x >= selmon->m.x + selmon->m.width ||
			    cursor->y < selmon->m.y ||
			    cursor->y >= selmon->m.y + selmon->m.height) {
				selmon = xytomon(cursor->x, cursor->y);
			}
		}
	}

	xytonode(cursor->x, cursor->y, &surface, &c, NULL, &sx, &sy);

	if (cursor_mode == CurPressed && !seat->drag &&
		surface != seat->pointer_state.focused_surface &&
		toplevel_from_wlr_surface(seat->pointer_state.focused_surface, &w,
								  &l) >= 0) {
		c = w;
		surface = seat->pointer_state.focused_surface;
		sx = cursor->x - (l ? l->scene->node.x : w->geom.x);
		sy = cursor->y - (l ? l->scene->node.y : w->geom.y);
	}

	{
		static int32_t last_drag_icon_x = INT32_MIN;
		static int32_t last_drag_icon_y = INT32_MIN;
		int32_t cx = (int32_t)round(cursor->x);
		int32_t cy = (int32_t)round(cursor->y);
		if (cx != last_drag_icon_x || cy != last_drag_icon_y) {
			wlr_scene_node_set_position(&drag_icon->node, cx, cy);
			last_drag_icon_x = cx;
			last_drag_icon_y = cy;
		}
	}

	if (cursor_mode == CurMove) {

		grabc->iscustomsize = 1;
		grabc->float_geom =
			(struct wlr_box){.x = (int32_t)round(cursor->x) - grabcx,
							 .y = (int32_t)round(cursor->y) - grabcy,
							 .width = grabc->geom.width,
							 .height = grabc->geom.height};
		if (config.drag_tile_to_tile && grabc->drag_to_tile) {
			static double last_search_x = -1e9, last_search_y = -1e9;
			static Client *last_search_grabc = NULL;
			const double dx = cursor->x - last_search_x;
			const double dy = cursor->y - last_search_y;
			if (last_search_grabc != grabc || dx * dx + dy * dy >= 16.0) {
				closet_drop_client = find_closest_tiled_client(grabc);
				last_search_x = cursor->x;
				last_search_y = cursor->y;
				last_search_grabc = grabc;
			}
			if (closet_drop_client && dropc && closet_drop_client != dropc) {
				dropc->enable_drop_area_draw = false;
				client_set_drop_area(dropc);
				dropc = closet_drop_client;
				dropc->enable_drop_area_draw = true;
				client_set_drop_area(dropc);
			} else if (closet_drop_client) {
				dropc = closet_drop_client;
				dropc->enable_drop_area_draw = true;
				client_set_drop_area(dropc);
			} else if (dropc) {
				dropc->enable_drop_area_draw = false;
				client_set_drop_area(dropc);
				dropc = NULL;
			}
		}
		resize(grabc, grabc->float_geom, 1);
		return;
	} else if (cursor_mode == CurResize) {
		if (grabc->isfloating) {
			grabc->iscustomsize = 1;
			if (last_apply_drap_time == 0 ||
				time - last_apply_drap_time >
					config.drag_floating_refresh_interval) {
				resize_floating_window(grabc);
				last_apply_drap_time = time;
			}
			return;
		} else {
			resize_tile_client(grabc, true, 0, 0, time);
		}
	}

	if (!surface && !seat->drag && !cursor_hidden)
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");

	if (c && c->mon && !c->animation.running && (INSIDEMON(c) || !ISTILED(c))) {
		scroller_focus_lock = 0;
	}

	should_lock = false;
	if (!scroller_focus_lock || !(c && c->mon && !INSIDEMON(c))) {
		if (c && c->mon && is_scroller_layout(c->mon) && !INSIDEMON(c)) {
			should_lock = true;
		}

		if (!(!config.edge_scroller_pointer_focus && c && c->mon &&
			  is_scroller_layout(c->mon) && !INSIDEMON(c)))
			pointerfocus(c, surface, sx, sy, time);

		if (should_lock && c && c->mon && ISTILED(c) && c == c->mon->sel) {
			scroller_focus_lock = 1;
		}
	}
}

LEMON_HOT void motionrelative(struct wl_listener *listener, void *data) {

	struct wlr_pointer_motion_event *event = data;

	if (check_trackpad_disabled(event->pointer)) {
		return;
	}

	motionnotify(event->time_msec, &event->pointer->base, event->delta_x,
				 event->delta_y, event->unaccel_dx, event->unaccel_dy);
	toggle_hotarea(cursor->x, cursor->y);
}
