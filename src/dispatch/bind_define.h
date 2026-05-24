/* Action: view the target tag (arg->ui), with optional toggle-back to previous tag when re-selecting current. */
int32_t bind_to_view(const Arg *arg) {
	if (!selmon)
		return 0;
	uint32_t target = arg->ui;

	if (config.view_current_to_back && selmon->pertag->curtag &&
		(target & TAGMASK) == (selmon->tagset[selmon->seltags])) {
		if (selmon->pertag->prevtag)
			target = 1 << (selmon->pertag->prevtag - 1);
		else
			target = 0;
	}

	if (!config.view_current_to_back &&
		(target & TAGMASK) == (selmon->tagset[selmon->seltags])) {
		return 0;
	}

	if ((int32_t)target == INT_MIN && selmon->pertag->curtag == 0) {
		if (config.view_current_to_back && selmon->pertag->prevtag)
			target = 1 << (selmon->pertag->prevtag - 1);
		else
			target = 0;
	}

	if (target == 0 || (int32_t)target == INT_MIN) {
		view(&(Arg){.ui = ~0 & TAGMASK, .i = arg->i}, false);
	} else {
		view(&(Arg){.ui = target, .i = arg->i}, true);
	}
	return 0;
}

/* Action: switch the active VT to arg->ui, pausing frame scheduling briefly across the change. */
int32_t chvt(const Arg *arg) {
	struct timespec ts;

	allow_frame_scheduling = false;

	if (selmon) {
		chvt_backup_tag = selmon->pertag->curtag;
		strncpy(chvt_backup_selmon, selmon->wlr_output->name,
				sizeof(chvt_backup_selmon) - 1);
	}

	wlr_session_change_vt(session, arg->ui);

	ts.tv_sec = 0;
	ts.tv_nsec = 100000000;
	nanosleep(&ts, NULL);

	allow_frame_scheduling = true;
	return 1;
}

/* Action: create a new headless (virtual) output on the multi backend. */
int32_t create_virtual_output(const Arg *arg) {

	if (!wlr_backend_is_multi(backend)) {
		wlr_log(WLR_ERROR, "Expected a multi backend");
		return 0;
	}

	bool done = false;
	wlr_multi_for_each_backend(backend, create_output, &done);

	if (!done) {
		wlr_log(WLR_ERROR, "Failed to create virtual output");
		return 0;
	}

	wlr_log(WLR_INFO, "Virtual output created");
	return 0;
}

/* Action: destroy every headless (virtual) output currently attached. */
int32_t destroy_all_virtual_output(const Arg *arg) {

	if (!wlr_backend_is_multi(backend)) {
		wlr_log(WLR_ERROR, "Expected a multi backend");
		return 0;
	}

	Monitor *m, *tmp;
	wl_list_for_each_safe(m, tmp, &mons, link) {
		if (wlr_output_is_headless(m->wlr_output)) {

			wlr_output_destroy(m->wlr_output);
			wlr_log(WLR_INFO, "Virtual output destroyed");
		}
	}
	return 0;
}

/* Action: reset inner/outer gaps to their configured default values. */
int32_t defaultgaps(const Arg *arg) {
	setgaps(config.gappoh, config.gappov, config.gappih, config.gappiv);
	return 0;
}

/* Action: swap the focused client with the tiled neighbor in arg->i direction. */
int32_t exchange_client(const Arg *arg) {
	if (!selmon)
		return 0;
	Client *c = selmon->sel;
	if (!c || c->isfloating)
		return 0;

	if ((c->isfullscreen || c->ismaximizescreen) && !is_scroller_layout(c->mon))
		return 0;

	Client *tc = direction_select(arg);
	tc = get_focused_stack_client(tc);

	if (!tc)
		return 0;

	exchange_two_client(c, tc);
	return 0;
}

/* Action: swap the focused client with the next/previous client in its scroller stack. */
int32_t exchange_stack_client(const Arg *arg) {
	if (!selmon)
		return 0;

	Client *c = selmon->sel;
	Client *tc = NULL;
	if (!c || c->isfloating || c->isfullscreen || c->ismaximizescreen)
		return 0;
	if (arg->i == NEXT) {
		tc = get_next_stack_client(c, false);
	} else {
		tc = get_next_stack_client(c, true);
	}
	if (tc)
		exchange_two_client(c, tc);
	return 0;
}

/* Action: move focus to the client in arg->i direction (UP/DOWN/LEFT/RIGHT), optionally crossing tags or monitors. */
int32_t focusdir(const Arg *arg) {
	Client *c = NULL;
	c = direction_select(arg);
	c = get_focused_stack_client(c);
	if (c) {
		focusclient(c, 1);
		if (config.warpcursor)
			warp_cursor(c);
	} else {
		if (config.focus_cross_tag) {
			if (arg->i == LEFT || arg->i == UP)
				viewtoleft_have_client(&(Arg){0});
			if (arg->i == RIGHT || arg->i == DOWN)
				viewtoright_have_client(&(Arg){0});
		} else if (config.focus_cross_monitor) {
			focusmon(arg);
		}
	}
	return 0;
}

/* Action: refocus the most recently focused client from the focus stack and switch to its tag. */
int32_t focuslast(const Arg *arg) {

	Client *c = NULL;
	Client *tc = NULL;
	bool begin = false;
	uint32_t target = 0;

	wl_list_for_each(c, &fstack, flink) {
		if (c->iskilling || c->isminimized || c->isunglobal ||
			!client_surface(c)->mapped || client_is_unmanaged(c) ||
			client_is_x11_popup(c))
			continue;

		if (selmon && !selmon->sel) {
			tc = c;
			break;
		}

		if (selmon && c == selmon->sel && !begin) {
			begin = true;
			continue;
		}

		if (begin) {
			tc = c;
			break;
		}
	}

	if (!tc || !client_surface(tc)->mapped)
		return 0;

	if ((int32_t)tc->tags > 0) {
		focusclient(tc, 1);
		target = get_tags_first_tag(tc->tags);
		view(&(Arg){.ui = target}, true);
	}
	return 0;
}

/* Action: toggle the config flag that disables the trackpad. */
int32_t toggle_trackpad_enable(const Arg *arg) {
	config.disable_trackpad = !config.disable_trackpad;
	return 0;
}

/* Action: move focus to the monitor in arg->i direction or matching arg->v spec, warping cursor if configured. */
int32_t focusmon(const Arg *arg) {
	Client *c = NULL;
	Monitor *m = NULL;
	Monitor *tm = NULL;

	if (arg->i != UNDIR) {
		tm = dirtomon(arg->i);
	} else if (arg->v) {
		wl_list_for_each(m, &mons, link) {
			if (!m->wlr_output->enabled) {
				continue;
			}
			if (match_monitor_spec(arg->v, m)) {
				tm = m;
				break;
			}
		}
	} else {
		return 0;
	}

	if (!tm || !tm->wlr_output->enabled || tm == selmon)
		return 0;

	selmon = tm;
	if (config.warpcursor) {
		warp_cursor_to_selmon(selmon);
	}
	c = focustop(selmon);
	if (!c) {
		selmon->sel = NULL;
		wlr_seat_pointer_notify_clear_focus(seat);
		wlr_seat_keyboard_notify_clear_focus(seat);
		focusclient(NULL, 0);
	} else
		focusclient(c, 1);

	return 0;
}

/* Action: cycle focus to the next or previous client in the tiling stack. */
int32_t focusstack(const Arg *arg) {

	Client *sel = focustop(selmon);
	Client *tc = NULL;

	if (!sel)
		return 0;
	if (arg->i == NEXT) {
		tc = get_next_stack_client(sel, false);
	} else {
		tc = get_next_stack_client(sel, true);
	}

	if (!tc)
		return 0;

	focusclient(tc, 1);
	if (config.warpcursor)
		warp_cursor(tc);
	return 0;
}

/* Action: adjust the number of master-area clients on the current tag by arg->i and re-arrange. */
int32_t incnmaster(const Arg *arg) {
	if (!arg || !selmon)
		return 0;
	selmon->pertag->nmasters[selmon->pertag->curtag] =
		MAX(selmon->pertag->nmasters[selmon->pertag->curtag] + arg->i, 0);
	arrange(selmon, false, false);
	return 0;
}

/* Action: add arg->i to every gap (inner h/v and outer h/v) on the current monitor. */
int32_t incgaps(const Arg *arg) {
	if (!selmon)
		return 0;
	setgaps(selmon->gappoh + arg->i, selmon->gappov + arg->i,
			selmon->gappih + arg->i, selmon->gappiv + arg->i);
	return 0;
}

/* Action: add arg->i to both inner gaps (horizontal and vertical) only. */
int32_t incigaps(const Arg *arg) {
	if (!selmon)
		return 0;
	setgaps(selmon->gappoh, selmon->gappov, selmon->gappih + arg->i,
			selmon->gappiv + arg->i);
	return 0;
}

/* Action: add arg->i to both outer gaps (horizontal and vertical) only. */
int32_t incogaps(const Arg *arg) {
	if (!selmon)
		return 0;
	setgaps(selmon->gappoh + arg->i, selmon->gappov + arg->i, selmon->gappih,
			selmon->gappiv);
	return 0;
}

/* Action: add arg->i to inner horizontal gap only. */
int32_t incihgaps(const Arg *arg) {
	if (!selmon)
		return 0;
	setgaps(selmon->gappoh, selmon->gappov, selmon->gappih + arg->i,
			selmon->gappiv);
	return 0;
}

/* Action: add arg->i to inner vertical gap only. */
int32_t incivgaps(const Arg *arg) {
	if (!selmon)
		return 0;
	setgaps(selmon->gappoh, selmon->gappov, selmon->gappih,
			selmon->gappiv + arg->i);
	return 0;
}

/* Action: add arg->i to outer horizontal gap only. */
int32_t incohgaps(const Arg *arg) {
	if (!selmon)
		return 0;
	setgaps(selmon->gappoh + arg->i, selmon->gappov, selmon->gappih,
			selmon->gappiv);
	return 0;
}

/* Action: add arg->i to outer vertical gap only. */
int32_t incovgaps(const Arg *arg) {
	if (!selmon)
		return 0;
	setgaps(selmon->gappoh, selmon->gappov + arg->i, selmon->gappih,
			selmon->gappiv);
	return 0;
}

/* Action: change the master/stack split factor on the current tag by arg->f and re-arrange. */
int32_t setmfact(const Arg *arg) {
	float f;
	Client *c = NULL;

	if (!arg || !selmon ||
		!selmon->pertag->ltidxs[selmon->pertag->curtag]->arrange)
		return 0;
	f = arg->f < 1.0 ? arg->f + selmon->pertag->mfacts[selmon->pertag->curtag]
					 : arg->f - 1.0;
	if (f < 0.1 || f > 0.9)
		return 0;

	selmon->pertag->mfacts[selmon->pertag->curtag] = f;
	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, selmon) && ISTILED(c)) {
			c->master_mfact_per = f;
		}
	}
	arrange(selmon, false, false);
	return 0;
}

/* Action: request the focused client to close. */
int32_t killclient(const Arg *arg) {
	Client *c = NULL;
	if (!selmon)
		return 0;
	c = selmon->sel;
	if (c) {
		pending_kill_client(c);
	}
	return 0;
}

/* Action: begin interactive cursor move or resize of the client under the cursor, based on arg->ui (CurMove/CurResize). */
int32_t moveresize(const Arg *arg) {
	const char *cursors[] = {"nw-resize", "ne-resize", "sw-resize",
							 "se-resize"};

	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return 0;
	xytonode(cursor->x, cursor->y, NULL, &grabc, NULL, NULL, NULL);
	if (!grabc || client_is_unmanaged(grabc) || grabc->isfullscreen ||
		grabc->ismaximizescreen) {
		grabc = NULL;
		return 0;
	}

	if (grabc->isfloating == 0 && arg->ui == CurMove) {
		grabc->drag_to_tile = true;
		exit_scroller_stack(grabc);
		setfloating(grabc, 1);
		grabc->drag_tile_float_backup_geom = grabc->float_geom;
		grabc->old_stack_inner_per = 0.0f;
		grabc->old_master_inner_per = 0.0f;
		set_size_per(grabc->mon, grabc);
	}

	if (grabc && grabc->drag_to_tile && config.drag_tile_small) {
		grabc->geom.x = cursor->x - 150;
		grabc->geom.y = cursor->y - 150;
		grabc->geom.width = 300;
		grabc->geom.height = 300;
		resize(grabc, grabc->geom, 1);
	}

	switch (cursor_mode = arg->ui) {
	case CurMove:

		grabcx = cursor->x - grabc->geom.x;
		grabcy = cursor->y - grabc->geom.y;
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "grab");
		break;
	case CurResize:

		if (grabc->isfloating) {
			rzcorner = config.drag_corner;
			grabcx = (int)round(cursor->x);
			grabcy = (int)round(cursor->y);
			if (rzcorner == 4)

				rzcorner = (grabcx - grabc->geom.x <
									grabc->geom.x + grabc->geom.width - grabcx
								? 0
								: 1) +
						   (grabcy - grabc->geom.y <
									grabc->geom.y + grabc->geom.height - grabcy
								? 0
								: 2);

			if (config.drag_warp_cursor) {
				grabcx = rzcorner & 1 ? grabc->geom.x + grabc->geom.width
									  : grabc->geom.x;
				grabcy = rzcorner & 2 ? grabc->geom.y + grabc->geom.height
									  : grabc->geom.y;
				wlr_cursor_warp_closest(cursor, NULL, grabcx, grabcy);
			}

			wlr_cursor_set_xcursor(cursor, cursor_mgr, cursors[rzcorner]);
		} else {
			wlr_cursor_set_xcursor(cursor, cursor_mgr, "grab");
		}
		break;
	}
	return 0;
}

/* Action: float and move the focused client to the (x,y) defined by arg->i / arg->i2 (absolute or delta per arg->ui/ui2). */
int32_t movewin(const Arg *arg) {
	Client *c = NULL;
	if (!selmon)
		return 0;
	c = selmon->sel;
	if (!c || c->isfullscreen)
		return 0;
	if (!c->isfloating)
		togglefloating(NULL);

	switch (arg->ui) {
	case NUM_TYPE_MINUS:
		c->geom.x -= arg->i;
		break;
	case NUM_TYPE_PLUS:
		c->geom.x += arg->i;
		break;
	default:
		c->geom.x = arg->i;
		break;
	}

	switch (arg->ui2) {
	case NUM_TYPE_MINUS:
		c->geom.y -= arg->i2;
		break;
	case NUM_TYPE_PLUS:
		c->geom.y += arg->i2;
		break;
	default:
		c->geom.y = arg->i2;
		break;
	}

	c->iscustomsize = 1;
	c->float_geom = c->geom;
	resize(c, c->geom, 0);
	return 0;
}

/* Action: terminate the Wayland display, exiting the compositor. */
int32_t quit(const Arg *arg) {
	wl_display_terminate(dpy);
	return 0;
}

/* Action: resize the focused client by arg->i/arg->i2 (absolute or delta per arg->ui/ui2), handling tiled and floating modes. */
int32_t resizewin(const Arg *arg) {
	Client *c = NULL;
	if (!selmon)
		return 0;
	c = selmon->sel;
	int32_t offsetx = 0, offsety = 0;

	if (!c || c->isfullscreen || c->ismaximizescreen)
		return 0;

	int32_t animations_state_backup = config.animations;
	if (!c->isfloating)
		config.animations = 0;

	if (ISTILED(c)) {
		switch (arg->ui) {
		case NUM_TYPE_MINUS:
			offsetx = -arg->i;
			break;
		case NUM_TYPE_PLUS:
			offsetx = arg->i;
			break;
		default:
			offsetx = arg->i;
			break;
		}

		switch (arg->ui2) {
		case NUM_TYPE_MINUS:
			offsety = -arg->i2;
			break;
		case NUM_TYPE_PLUS:
			offsety = arg->i2;
			break;
		default:
			offsety = arg->i2;
			break;
		}
		resize_tile_client(c, false, offsetx, offsety, 0);
		config.animations = animations_state_backup;
		return 0;
	}

	switch (arg->ui) {
	case NUM_TYPE_MINUS:
		c->geom.width -= arg->i;
		break;
	case NUM_TYPE_PLUS:
		c->geom.width += arg->i;
		break;
	default:
		c->geom.width = arg->i;
		break;
	}

	switch (arg->ui2) {
	case NUM_TYPE_MINUS:
		c->geom.height -= arg->i2;
		break;
	case NUM_TYPE_PLUS:
		c->geom.height += arg->i2;
		break;
	default:
		c->geom.height = arg->i2;
		break;
	}

	c->iscustomsize = 1;
	c->float_geom = c->geom;
	resize(c, c->geom, 0);
	config.animations = animations_state_backup;
	return 0;
}

/* Action: restore the first minimized (non-named-scratchpad) client, or hide a visible scratchpad client. */
int32_t restore_minimized(const Arg *arg) {
	Client *c = NULL;

	if (selmon && selmon->isoverview)
		return 0;

	if (selmon && selmon->sel && selmon->sel->is_in_scratchpad &&
		selmon->sel->is_scratchpad_show) {
		client_pending_minimized_state(selmon->sel, 0);
		selmon->sel->is_scratchpad_show = 0;
		selmon->sel->is_in_scratchpad = 0;
		selmon->sel->isnamedscratchpad = 0;
		setborder_color(selmon->sel);
		return 0;
	}

	wl_list_for_each(c, &clients, link) {
		if (c->isminimized && !c->isnamedscratchpad) {
			c->is_scratchpad_show = 0;
			c->is_in_scratchpad = 0;
			c->isnamedscratchpad = 0;
			show_hide_client(c);
			setborder_color(c);
			arrange(c->mon, false, false);
			focusclient(c, 0);
			warp_cursor(c);
			return 0;
		}
	}
	return 0;
}

/* Action: swipe the focused window in arg->i direction.
   - LEFT/RIGHT/UP: switch to the matching master-side layout (tile /
     right_tile / vertical_tile) and exchange the focused client with its
     spatial neighbor on that side, so the window visibly travels.
   - DOWN: minimize the focused client (no layout matches "master on bottom").
   When the current tag has 1 visible tiled client or less, no swap is
   possible -- fall back to the vertical_scroller layout instead. */
int32_t swipe_layout_dir(const Arg *arg) {
	if (!selmon || !arg)
		return 0;

	if (arg->i == DOWN)
		return minimized(&(Arg){0});

	const char *target = NULL;
	switch (arg->i) {
	case LEFT:  target = "tile"; break;
	case RIGHT: target = "right_tile"; break;
	case UP:    target = "vertical_tile"; break;
	default: return 0;
	}

	clear_fullscreen_and_maximized_state(selmon);

	/* Force every visible client on the current tag into the tile so a 4f
	   swipe still works when the focused window is floating / fullscreen /
	   maximized -- otherwise it would be excluded from exchange_client and
	   the swipe would silently no-op. Count tiled clients here (after
	   un-floating) since selmon->visible_tiling_clients is only refreshed
	   by arrange() and would still hold the pre-swipe count. */
	Client *fc = NULL;
	uint32_t tiled = 0;
	wl_list_for_each(fc, &clients, link) {
		if (!fc || fc->mon != selmon || fc->iskilling || fc->isunglobal ||
			client_is_unmanaged(fc) || client_is_x11_popup(fc))
			continue;
		if (!VISIBLEON(fc, selmon))
			continue;
		if (fc->isfloating)
			setfloating(fc, 0);
		if (!fc->isfloating && !fc->isminimized)
			tiled++;
	}

	const char *picked = (tiled <= 1) ? "vertical_scroller" : target;
	for (int32_t i = 0; i < LENGTH(layouts); i++) {
		if (strcmp(layouts[i].name, picked) == 0) {
			selmon->pertag->ltidxs[selmon->pertag->curtag] = &layouts[i];
			break;
		}
	}
	arrange(selmon, false, false);

	if (tiled > 1)
		exchange_client(&(Arg){.i = arg->i});
	printstatus();
	return 0;
}

/* Action: open / step the Alt+Tab style window cycler overlay forward.
   Falls back to plain focusstack(NEXT) when only one window is visible. */
int32_t window_cycler_next(const Arg *arg) {
	(void)arg;
	if (!selmon)
		return 0;
	if (!window_cycler_step(1))
		focusstack(&(Arg){.i = NEXT});
	return 0;
}

/* Action: open / step the Alt+Tab style window cycler overlay backward.
   Falls back to plain focusstack(PREV) when only one window is visible. */
int32_t window_cycler_prev(const Arg *arg) {
	(void)arg;
	if (!selmon)
		return 0;
	if (!window_cycler_step(-1))
		focusstack(&(Arg){.i = PREV});
	return 0;
}

/* Action: switch the current tag's layout to the one whose name matches arg->v. */
int32_t setlayout(const Arg *arg) {
	int32_t jk;
	if (!selmon)
		return 0;

	for (jk = 0; jk < LENGTH(layouts); jk++) {
		if (strcmp(layouts[jk].name, arg->v) == 0) {
			selmon->pertag->ltidxs[selmon->pertag->curtag] = &layouts[jk];
			clear_fullscreen_and_maximized_state(selmon);
			arrange(selmon, false, false);
			printstatus();
			return 0;
		}
	}
	return 0;
}

/* Action: set the current key mode name to arg->v (used by modal keybinding groups). */
int32_t setkeymode(const Arg *arg) {
	snprintf(keymode.mode, sizeof(keymode.mode), "%.27s", arg->v);
	if (strcmp(keymode.mode, "default") == 0) {
		keymode.isdefault = true;
	} else {
		keymode.isdefault = false;
	}
	printstatus();
	return 1;
}

/* Action: set the scroller width proportion of the focused stack-head client to arg->f. */
int32_t set_proportion(const Arg *arg) {
	if (!selmon)
		return 0;

	if (selmon->isoverview || !is_scroller_layout(selmon))
		return 0;

	if (selmon->visible_tiling_clients == 1 &&
		!config.scroller_ignore_proportion_single)
		return 0;

	Client *tc = selmon->sel;
	if (!tc)
		return 0;

	tc = scroll_get_stack_head_client(tc);
	if (!tc)
		return 0;

	Monitor *m = tc->mon;
	uint32_t tag = m->pertag->curtag;
	struct TagScrollerState *st = m->pertag->scroller_state[tag];
	struct ScrollerStackNode *node = NULL;

	if (st)
		node = find_scroller_node(st, tc);

	if (node)
		node->scroller_proportion = arg->f;
	tc->scroller_proportion = arg->f;

	uint32_t max_client_width =
		m->w.width - 2 * config.scroller_structs - config.gappih;
	tc->geom.width = max_client_width * arg->f;

	arrange(m, false, false);
	return 0;
}

/* Action: cycle the scroller proportion of the focused client through configured presets in arg->i direction. */
int32_t switch_proportion_preset(const Arg *arg) {
	float target_proportion = 0;
	if (!selmon)
		return 0;

	if (config.scroller_proportion_preset_count == 0)
		return 0;

	if (selmon->isoverview || !is_scroller_layout(selmon))
		return 0;

	if (selmon->visible_tiling_clients == 1 &&
		!config.scroller_ignore_proportion_single)
		return 0;

	Client *tc = selmon->sel;
	if (!tc)
		return 0;

	tc = scroll_get_stack_head_client(tc);
	if (!tc)
		return 0;

	Monitor *m = tc->mon;
	uint32_t tag = m->pertag->curtag;
	struct TagScrollerState *st = m->pertag->scroller_state[tag];
	struct ScrollerStackNode *node = NULL;

	if (st)
		node = find_scroller_node(st, tc);

	float current_proportion =
		node ? node->scroller_proportion : tc->scroller_proportion;

	for (int32_t i = 0; i < config.scroller_proportion_preset_count; i++) {
		if (config.scroller_proportion_preset[i] == current_proportion) {
			if (arg->i == NEXT) {
				if (i == config.scroller_proportion_preset_count - 1)
					target_proportion = config.scroller_proportion_preset[0];
				else
					target_proportion =
						config.scroller_proportion_preset[i + 1];
			} else {
				if (i == 0)
					target_proportion =
						config.scroller_proportion_preset
							[config.scroller_proportion_preset_count - 1];
				else
					target_proportion =
						config.scroller_proportion_preset[i - 1];
			}
			break;
		}
	}

	if (target_proportion == 0.0f)
		target_proportion = config.scroller_proportion_preset[0];

	if (node)
		node->scroller_proportion = target_proportion;
	tc->scroller_proportion = target_proportion;

	uint32_t max_client_width =
		m->w.width - 2 * config.scroller_structs - config.gappih;
	tc->geom.width = max_client_width * target_proportion;

	arrange(m, false, false);
	return 0;
}

/* Action: move the focused floating client toward arg->i direction, stopping at floating neighbors or screen edges. */
int32_t smartmovewin(const Arg *arg) {
	Client *c = NULL, *tc = NULL;
	int32_t nx, ny;
	int32_t buttom, top, left, right, tar;
	if (!selmon)
		return 0;
	c = selmon->sel;
	if (!c || c->isfullscreen)
		return 0;
	if (!c->isfloating)
		setfloating(selmon->sel, true);
	nx = c->geom.x;
	ny = c->geom.y;

	switch (arg->i) {
	case UP:
		tar = -99999;
		top = c->geom.y;
		ny -= c->mon->w.height / 4;

		wl_list_for_each(tc, &clients, link) {
			if (!VISIBLEON(tc, selmon) || !tc->isfloating || tc == c)
				continue;
			if (c->geom.x + c->geom.width < tc->geom.x ||
				c->geom.x > tc->geom.x + tc->geom.width)
				continue;
			buttom = tc->geom.y + tc->geom.height + config.gappiv;
			if (top > buttom && ny < buttom) {
				tar = MAX(tar, buttom);
			};
		}

		ny = tar == -99999 ? ny : tar;
		ny = MAX(ny, c->mon->w.y + c->mon->gappov);
		break;
	case DOWN:
		tar = 99999;
		buttom = c->geom.y + c->geom.height;
		ny += c->mon->w.height / 4;

		wl_list_for_each(tc, &clients, link) {
			if (!VISIBLEON(tc, selmon) || !tc->isfloating || tc == c)
				continue;
			if (c->geom.x + c->geom.width < tc->geom.x ||
				c->geom.x > tc->geom.x + tc->geom.width)
				continue;
			top = tc->geom.y - config.gappiv;
			if (buttom < top && (ny + c->geom.height) > top) {
				tar = MIN(tar, top - c->geom.height);
			};
		}
		ny = tar == 99999 ? ny : tar;
		ny = MIN(ny, c->mon->w.y + c->mon->w.height - c->geom.height -
						 c->mon->gappov);
		break;
	case LEFT:
		tar = -99999;
		left = c->geom.x;
		nx -= c->mon->w.width / 6;

		wl_list_for_each(tc, &clients, link) {
			if (!VISIBLEON(tc, selmon) || !tc->isfloating || tc == c)
				continue;
			if (c->geom.y + c->geom.height < tc->geom.y ||
				c->geom.y > tc->geom.y + tc->geom.height)
				continue;
			right = tc->geom.x + tc->geom.width + config.gappih;
			if (left > right && nx < right) {
				tar = MAX(tar, right);
			};
		}

		nx = tar == -99999 ? nx : tar;
		nx = MAX(nx, c->mon->w.x + c->mon->gappoh);
		break;
	case RIGHT:
		tar = 99999;
		right = c->geom.x + c->geom.width;
		nx += c->mon->w.width / 6;
		wl_list_for_each(tc, &clients, link) {
			if (!VISIBLEON(tc, selmon) || !tc->isfloating || tc == c)
				continue;
			if (c->geom.y + c->geom.height < tc->geom.y ||
				c->geom.y > tc->geom.y + tc->geom.height)
				continue;
			left = tc->geom.x - config.gappih;
			if (right < left && (nx + c->geom.width) > left) {
				tar = MIN(tar, left - c->geom.width);
			};
		}
		nx = tar == 99999 ? nx : tar;
		nx = MIN(nx, c->mon->w.x + c->mon->w.width - c->geom.width -
						 c->mon->gappoh);
		break;
	}

	c->float_geom = (struct wlr_box){
		.x = nx, .y = ny, .width = c->geom.width, .height = c->geom.height};
	c->iscustomsize = 1;
	resize(c, c->float_geom, 1);
	return 0;
}

/* Action: resize the focused floating client toward arg->i direction, stopping at floating neighbors or screen edges. */
int32_t smartresizewin(const Arg *arg) {
	Client *c = NULL, *tc = NULL;
	int32_t nw, nh;
	int32_t buttom, top, left, right, tar;
	if (!selmon)
		return 0;
	c = selmon->sel;
	if (!c || c->isfullscreen)
		return 0;
	if (!c->isfloating)
		setfloating(c, true);
	nw = c->geom.width;
	nh = c->geom.height;

	switch (arg->i) {
	case UP:
		nh -= selmon->w.height / 8;
		nh = MAX(nh, selmon->w.height / 10);
		break;
	case DOWN:
		tar = -99999;
		buttom = c->geom.y + c->geom.height;
		nh += selmon->w.height / 8;

		wl_list_for_each(tc, &clients, link) {
			if (!VISIBLEON(tc, selmon) || !tc->isfloating || tc == c)
				continue;
			if (c->geom.x + c->geom.width < tc->geom.x ||
				c->geom.x > tc->geom.x + tc->geom.width)
				continue;
			top = tc->geom.y - config.gappiv;
			if (buttom < top && (nh + c->geom.y) > top) {
				tar = MAX(tar, top - c->geom.y);
			};
		}
		nh = tar == -99999 ? nh : tar;
		if (c->geom.y + nh + config.gappov > selmon->w.y + selmon->w.height)
			nh = selmon->w.y + selmon->w.height - c->geom.y - config.gappov;
		break;
	case LEFT:
		nw -= selmon->w.width / 16;
		nw = MAX(nw, selmon->w.width / 10);
		break;
	case RIGHT:
		tar = 99999;
		right = c->geom.x + c->geom.width;
		nw += selmon->w.width / 16;
		wl_list_for_each(tc, &clients, link) {
			if (!VISIBLEON(tc, selmon) || !tc->isfloating || tc == c)
				continue;
			if (c->geom.y + c->geom.height < tc->geom.y ||
				c->geom.y > tc->geom.y + tc->geom.height)
				continue;
			left = tc->geom.x - config.gappih;
			if (right < left && (nw + c->geom.x) > left) {
				tar = MIN(tar, left - c->geom.x);
			};
		}

		nw = tar == 99999 ? nw : tar;
		if (c->geom.x + nw + config.gappoh > selmon->w.x + selmon->w.width)
			nw = selmon->w.x + selmon->w.width - c->geom.x - config.gappoh;
		break;
	}

	c->float_geom = (struct wlr_box){
		.x = c->geom.x, .y = c->geom.y, .width = nw, .height = nh};
	c->iscustomsize = 1;
	resize(c, c->float_geom, 1);
	return 0;
}

/* Action: center the focused client on its monitor (floating) or center its scroller stack head (tiled). */
int32_t centerwin(const Arg *arg) {
	Client *c = NULL;
	if (!selmon)
		return 0;
	c = selmon->sel;

	if (!c || c->isfullscreen || c->ismaximizescreen)
		return 0;

	if (c->isfloating) {
		c->float_geom = setclient_coordinate_center(c, c->mon, c->geom, 0, 0);
		c->iscustomsize = 1;
		resize(c, c->float_geom, 1);
		return 0;
	}

	if (!is_scroller_layout(selmon))
		return 0;

	Client *stack_head = scroll_get_stack_head_client(c);
	if (selmon->pertag->ltidxs[selmon->pertag->curtag]->id == SCROLLER) {
		stack_head->geom.x =
			selmon->w.x + (selmon->w.width - stack_head->geom.width) / 2;
	} else {
		stack_head->geom.y =
			selmon->w.y + (selmon->w.height - stack_head->geom.height) / 2;
	}

	arrange(selmon, false, false);
	return 0;
}

extern char **environ;

/* Build a posix_spawn file_actions object that redirects stdout to stderr,
   detaches from the controlling terminal via setsid, and resets the child's
   scheduling policy to normal SCHED_OTHER nice 0 so launched apps do not
   inherit the compositor's SCHED_RR / nice -10 (which would let a misbehaving
   app starve everything else). */
static int spawn_make_actions(posix_spawn_file_actions_t *fa,
                              posix_spawnattr_t *attr) {
	if (posix_spawn_file_actions_init(fa) != 0)
		return -1;
	if (posix_spawnattr_init(attr) != 0) {
		posix_spawn_file_actions_destroy(fa);
		return -1;
	}
	posix_spawn_file_actions_adddup2(fa, STDERR_FILENO, STDOUT_FILENO);
	short flags = 0;
#ifdef POSIX_SPAWN_SETSID
	flags |= POSIX_SPAWN_SETSID;
#endif
#ifdef POSIX_SPAWN_USEVFORK
	/* clone(CLONE_VM|CLONE_VFORK): child shares the address space until exec,
	   so the compositor's page tables are never duplicated. Modern glibc takes
	   this path by default; the flag makes the intent explicit. */
	flags |= POSIX_SPAWN_USEVFORK;
#endif
#ifdef POSIX_SPAWN_SETSCHEDULER
	{
		struct sched_param sp = {.sched_priority = 0};
		posix_spawnattr_setschedpolicy(attr, SCHED_OTHER);
		posix_spawnattr_setschedparam(attr, &sp);
		flags |= POSIX_SPAWN_SETSCHEDULER;
	}
#endif
	posix_spawnattr_setflags(attr, flags);
	return 0;
}

/* Action: launch arg->v through sh -c using posix_spawn (clone+vfork path, no fork() page-table copy).
   Falls back to bash if /bin/sh exec fails. */
int32_t spawn_shell(const Arg *arg) {
	if (!arg->v)
		return 0;

	posix_spawn_file_actions_t fa;
	posix_spawnattr_t attr;
	if (spawn_make_actions(&fa, &attr) != 0)
		return 0;

	pid_t pid;
	char *sh_argv[] = {(char *)"sh", (char *)"-c", (char *)arg->v, NULL};
	int rc = posix_spawn(&pid, "/bin/sh", &fa, &attr, sh_argv, environ);
	if (rc != 0) {
		char *bash_argv[] = {(char *)"bash", (char *)"-c", (char *)arg->v, NULL};
		rc = posix_spawn(&pid, "/bin/bash", &fa, &attr, bash_argv, environ);
	}
	if (rc != 0) {
		wlr_log(WLR_DEBUG,
		        "lemon: posix_spawn failed for '%s': %s\n",
		        arg->v, strerror(rc));
	}

	posix_spawn_file_actions_destroy(&fa);
	posix_spawnattr_destroy(&attr);
	return 0;
}

/* Action: launch arg->v via wordexp+posix_spawnp; uses PATH lookup. Avoids fork()'s COW cost. */
int32_t spawn(const Arg *arg) {
	if (!arg->v)
		return 0;

	wordexp_t p;
	if (wordexp(arg->v, &p, 0) != 0) {
		wlr_log(WLR_DEBUG, "lemon: wordexp failed for '%s'\n", arg->v);
		return 0;
	}

	posix_spawn_file_actions_t fa;
	posix_spawnattr_t attr;
	if (spawn_make_actions(&fa, &attr) != 0) {
		wordfree(&p);
		return 0;
	}

	pid_t pid;
	int rc = posix_spawnp(&pid, p.we_wordv[0], &fa, &attr, p.we_wordv, environ);
	if (rc != 0) {
		wlr_log(WLR_DEBUG, "lemon: posix_spawnp '%s' failed: %s\n",
		        p.we_wordv[0], strerror(rc));
	}

	posix_spawn_file_actions_destroy(&fa);
	posix_spawnattr_destroy(&attr);
	wordfree(&p);
	return 0;
}

/* Action: view arg->ui tag; if it has no clients, also spawn arg->v as a shell command on it. */
int32_t spawn_on_empty(const Arg *arg) {
	bool is_empty = true;
	Client *c = NULL;

	wl_list_for_each(c, &clients, link) {
		if (arg->ui & c->tags && c->mon == selmon) {
			is_empty = false;
			break;
		}
	}
	if (!is_empty) {
		view(arg, true);
		return 0;
	} else {
		view(arg, true);
		spawn_shell(arg);
	}
	return 0;
}

/* Action: cycle the active xkb keyboard layout to the next (or arg->i-th) layout across all keyboards. */
int32_t switch_keyboard_layout(const Arg *arg) {
	if (!kb_group || !kb_group->wlr_group || !seat) {
		wlr_log(WLR_ERROR, "Invalid keyboard group or seat");
		return 0;
	}

	struct wlr_keyboard *keyboard = &kb_group->wlr_group->keyboard;
	if (!keyboard || !keyboard->keymap) {
		wlr_log(WLR_ERROR, "Invalid keyboard or keymap");
		return 0;
	}

	xkb_layout_index_t current = xkb_state_serialize_layout(
		keyboard->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
	const int32_t num_layouts = xkb_keymap_num_layouts(keyboard->keymap);
	if (num_layouts < 2) {
		wlr_log(WLR_INFO, "Only one layout available");
		return 0;
	}

	xkb_layout_index_t next = 0;
	if (arg->i > 0 && arg->i <= num_layouts) {
		next = arg->i - 1;
	} else {
		next = (current + 1) % num_layouts;
	}

	uint32_t depressed = keyboard->modifiers.depressed;
	uint32_t latched = keyboard->modifiers.latched;
	uint32_t locked = keyboard->modifiers.locked;

	wlr_keyboard_notify_modifiers(keyboard, depressed, latched, locked, next);

	wlr_seat_set_keyboard(seat, keyboard);
	wlr_seat_keyboard_notify_modifiers(seat, &keyboard->modifiers);

	InputDevice *id;
	wl_list_for_each(id, &inputdevices, link) {
		if (id->wlr_device->type != WLR_INPUT_DEVICE_KEYBOARD) {
			continue;
		}

		struct wlr_keyboard *tkb = (struct wlr_keyboard *)id->device_data;

		wlr_keyboard_notify_modifiers(tkb, depressed, latched, locked, next);

		wlr_seat_set_keyboard(seat, tkb);
		wlr_seat_keyboard_notify_modifiers(seat, &tkb->modifiers);
	}

	printstatus();
	return 0;
}

/* Action: cycle to the next layout in the configured circle list (or all layouts if no circle is configured). */
int32_t switch_layout(const Arg *arg) {

	int32_t jk, ji;
	char *target_layout_name = NULL;
	uint32_t len;

	if (!selmon)
		return 0;

	if (config.circle_layout_count != 0) {
		for (jk = 0; jk < config.circle_layout_count; jk++) {

			len = MAX(
				strlen(config.circle_layout[jk]),
				strlen(selmon->pertag->ltidxs[selmon->pertag->curtag]->name));

			if (strncmp(config.circle_layout[jk],
						selmon->pertag->ltidxs[selmon->pertag->curtag]->name,
						len) == 0) {
				target_layout_name = jk == config.circle_layout_count - 1
										 ? config.circle_layout[0]
										 : config.circle_layout[jk + 1];
				break;
			}
		}

		if (!target_layout_name) {
			target_layout_name = config.circle_layout[0];
		}

		for (ji = 0; ji < LENGTH(layouts); ji++) {
			len = MAX(strlen(layouts[ji].name), strlen(target_layout_name));
			if (strncmp(layouts[ji].name, target_layout_name, len) == 0) {
				selmon->pertag->ltidxs[selmon->pertag->curtag] = &layouts[ji];

				break;
			}
		}
		clear_fullscreen_and_maximized_state(selmon);
		arrange(selmon, false, false);
		printstatus();
		return 0;
	}

	for (jk = 0; jk < LENGTH(layouts); jk++) {
		if (strcmp(layouts[jk].name,
				   selmon->pertag->ltidxs[selmon->pertag->curtag]->name) == 0) {
			selmon->pertag->ltidxs[selmon->pertag->curtag] =
				jk == LENGTH(layouts) - 1 ? &layouts[0] : &layouts[jk + 1];
			clear_fullscreen_and_maximized_state(selmon);
			arrange(selmon, false, false);
			printstatus();
			return 0;
		}
	}
	return 0;
}

/* Action: assign the focused client to tag(s) arg->ui and switch view accordingly. */
int32_t tag(const Arg *arg) {
	if (!selmon)
		return 0;
	Client *target_client = selmon->sel;
	tag_client(arg, target_client);
	return 0;
}

/* Action: move the focused client to the monitor in arg->i direction or matching arg->v, retiling and refocusing. */
int32_t tagmon(const Arg *arg) {
	Monitor *m = NULL, *cm = NULL, *oldmon = NULL;
	if (!selmon)
		return 0;
	Client *c = focustop(selmon);

	if (!c)
		return 0;

	oldmon = c->mon;

	if (arg->i != UNDIR) {
		m = dirtomon(arg->i);
	} else if (arg->v) {
		wl_list_for_each(cm, &mons, link) {
			if (!cm->wlr_output->enabled) {
				continue;
			}
			if (match_monitor_spec(arg->v, cm)) {
				m = cm;
				break;
			}
		}
	} else {
		return 0;
	}

	if (!m || !m->wlr_output->enabled)
		return 0;

	uint32_t newtags = arg->ui ? arg->ui : arg->i2 ? c->tags : 0;
	uint32_t target;

	if (c->mon == m) {
		view(&(Arg){.ui = newtags}, true);
		return 0;
	}

	if (c == selmon->sel) {
		selmon->sel = NULL;
	}

	setmon(c, m, newtags, true);
	client_update_oldmonname_record(c, m);

	reset_foreign_tolevel(c, oldmon, c->mon);

	c->float_geom.width =
		(int32_t)(c->float_geom.width * c->mon->w.width / oldmon->w.width);
	c->float_geom.height =
		(int32_t)(c->float_geom.height * c->mon->w.height / oldmon->w.height);
	selmon = c->mon;
	c->float_geom = setclient_coordinate_center(c, c->mon, c->float_geom, 0, 0);

	if (c->isfloating) {
		c->geom = c->float_geom;
		target = get_tags_first_tag(c->tags);
		view(&(Arg){.ui = target}, true);
		focusclient(c, 1);
		resize(c, c->geom, 1);
	} else {
		selmon = c->mon;
		target = get_tags_first_tag(c->tags);
		view(&(Arg){.ui = target}, true);
		focusclient(c, 1);
		arrange(selmon, false, false);
	}
	if (config.warpcursor) {
		warp_cursor_to_selmon(c->mon);
	}
	return 0;
}

/* Action: move the focused client to tag(s) arg->ui without switching the current view. */
int32_t tagsilent(const Arg *arg) {
	Client *fc = NULL;
	Client *target_client = NULL;

	if (!selmon || !selmon->sel)
		return 0;

	target_client = selmon->sel;
	target_client->tags = arg->ui & TAGMASK;
	wl_list_for_each(fc, &clients, link) {
		if (fc && fc != target_client && target_client->tags & fc->tags &&
			ISFULLSCREEN(fc) && !target_client->isfloating) {
			clear_fullscreen_flag(fc);
		}
	}
	focusclient(focustop(selmon), 1);
	arrange(target_client->mon, false, false);
	return 0;
}

/* Action: move the focused client to the tag immediately left of the current single-tag view. */
int32_t tagtoleft(const Arg *arg) {
	if (!selmon)
		return 0;

	if (selmon->sel != NULL &&
		__builtin_popcount(selmon->tagset[selmon->seltags] & TAGMASK) == 1 &&
		selmon->tagset[selmon->seltags] > 1) {
		tag(&(Arg){.ui = selmon->tagset[selmon->seltags] >> 1, .i = arg->i});
	}
	return 0;
}

/* Action: move the focused client to the tag immediately right of the current single-tag view. */
int32_t tagtoright(const Arg *arg) {
	if (!selmon)
		return 0;

	if (selmon->sel != NULL &&
		__builtin_popcount(selmon->tagset[selmon->seltags] & TAGMASK) == 1 &&
		selmon->tagset[selmon->seltags] & (TAGMASK >> 1)) {
		tag(&(Arg){.ui = selmon->tagset[selmon->seltags] << 1, .i = arg->i});
	}
	return 0;
}

/* Action: toggle the named scratchpad identified by arg->v (id) or arg->v2 (title), spawning arg->v3 if not found. */
int32_t toggle_named_scratchpad(const Arg *arg) {
	Client *target_client = NULL;
	char *arg_id = arg->v;
	char *arg_title = arg->v2;

	target_client = get_client_by_id_or_title(arg_id, arg_title);

	if (!target_client && arg->v3) {
		Arg arg_spawn = {.v = arg->v3};
		spawn_shell(&arg_spawn);
		return 0;
	}

	target_client->isnamedscratchpad = 1;

	apply_named_scratchpad(target_client);
	return 0;
}

/* Action: toggle whether client borders are rendered, then re-arrange. */
int32_t toggle_render_border(const Arg *arg) {
	if (!selmon)
		return 0;
	render_border = !render_border;
	arrange(selmon, false, false);
	return 0;
}

/* Action: toggle visibility of unnamed scratchpad clients (minimize/restore), honoring single/cross-monitor settings. */
int32_t toggle_scratchpad(const Arg *arg) {
	Client *c = NULL;
	bool hit = false;
	Client *tmp = NULL;

	if (selmon && selmon->isoverview)
		return 0;

	wl_list_for_each_safe(c, tmp, &clients, link) {
		if (!config.scratchpad_cross_monitor && c->mon != selmon) {
			continue;
		}

		if (config.single_scratchpad && c->isnamedscratchpad &&
			!c->isminimized) {
			set_minimized(c);
			continue;
		}

		if (c->isnamedscratchpad)
			continue;

		if (hit)
			continue;

		hit = switch_scratchpad_client_state(c);
	}
	return 0;
}

/* Action: toggle fake-fullscreen state on the focused client (fullscreens visually without unmanaging tiling). */
int32_t togglefakefullscreen(const Arg *arg) {
	if (!selmon)
		return 0;
	Client *sel = focustop(selmon);
	if (sel)
		setfakefullscreen(sel, !sel->isfakefullscreen);
	return 0;
}

/* Action: toggle the focused client between floating and tiled. */
int32_t togglefloating(const Arg *arg) {
	if (!selmon)
		return 0;

	if (selmon->isoverview)
		return 0;

	/* Prefer the explicitly-focused client. focustop walks the focus stack
	   and may return a different visible client when the active window has
	   transient state (e.g. minimized, pending kill, scratchpad). */
	Client *sel = selmon->sel;
	if (!sel || sel->iskilling || sel->isunglobal || !VISIBLEON(sel, selmon))
		sel = focustop(selmon);

	if (!sel)
		return 0;

	bool isfloating;
	if (sel->isfullscreen || sel->ismaximizescreen) {
		isfloating = 1;
	} else {
		isfloating = !sel->isfloating;
	}

	setfloating(sel, isfloating);
	return 0;
}

/* Action: toggle fullscreen on the focused client (clearing scratchpad flags first). */
int32_t togglefullscreen(const Arg *arg) {
	if (!selmon)
		return 0;

	Client *sel = focustop(selmon);
	if (!sel)
		return 0;

	sel->is_scratchpad_show = 0;
	sel->is_in_scratchpad = 0;
	sel->isnamedscratchpad = 0;

	if (sel->isfullscreen)
		setfullscreen(sel, 0);
	else
		setfullscreen(sel, 1);
	return 0;
}

/* Action: toggle the "global" (sticky across tags) flag on the focused client. */
int32_t toggleglobal(const Arg *arg) {
	if (!selmon)
		return 0;

	if (!selmon->sel)
		return 0;
	if (selmon->sel->is_in_scratchpad) {
		selmon->sel->is_in_scratchpad = 0;
		selmon->sel->is_scratchpad_show = 0;
		selmon->sel->isnamedscratchpad = 0;
	}
	selmon->sel->isglobal ^= 1;
	setborder_color(selmon->sel);
	return 0;
}

/* Action: toggle the global gaps-enabled flag and re-arrange. */
int32_t togglegaps(const Arg *arg) {
	if (!selmon)
		return 0;

	enablegaps ^= 1;
	arrange(selmon, false, false);
	return 0;
}

/* Action: toggle maximize-to-screen on the focused client (fills monitor but stays in layout). */
int32_t togglemaximizescreen(const Arg *arg) {
	if (!selmon)
		return 0;

	Client *sel = focustop(selmon);
	if (!sel)
		return 0;

	sel->is_scratchpad_show = 0;
	sel->is_in_scratchpad = 0;
	sel->isnamedscratchpad = 0;

	if (sel->ismaximizescreen)
		setmaximizescreen(sel, 0);
	else
		setmaximizescreen(sel, 1);

	setborder_color(sel);
	return 0;
}

/* Action: toggle overlay state on the focused client, reparenting its scene node between layers. */
int32_t toggleoverlay(const Arg *arg) {
	if (!selmon)
		return 0;

	if (!selmon->sel || !selmon->sel->mon || selmon->sel->isfullscreen) {
		return 0;
	}

	selmon->sel->isoverlay ^= 1;

	if (selmon->sel->isoverlay) {
		wlr_scene_node_reparent(&selmon->sel->scene->node, layers[LyrOverlay]);
		wlr_scene_node_raise_to_top(&selmon->sel->scene->node);
	} else if (client_should_overtop(selmon->sel) && selmon->sel->isfloating) {
		wlr_scene_node_reparent(&selmon->sel->scene->node, layers[LyrTop]);
	} else {
		wlr_scene_node_reparent(
			&selmon->sel->scene->node,
			layers[selmon->sel->isfloating ? LyrTop : LyrTile]);
	}
	setborder_color(selmon->sel);
	return 0;
}

/* Action: XOR the focused client's tag mask with arg->ui (special-case INT_MIN toggles between all-tags and current tag). */
int32_t toggletag(const Arg *arg) {
	if (!selmon)
		return 0;

	uint32_t newtags;
	Client *sel = focustop(selmon);
	if (!sel)
		return 0;

	if ((int32_t)arg->ui == INT_MIN && sel->tags != (~0 & TAGMASK)) {
		newtags = ~0 & TAGMASK;
	} else if ((int32_t)arg->ui == INT_MIN && sel->tags == (~0 & TAGMASK)) {
		newtags = 1 << (sel->mon->pertag->curtag - 1);
	} else {
		newtags = sel->tags ^ (arg->ui & TAGMASK);
	}

	if (newtags) {
		sel->tags = newtags;
		focusclient(focustop(selmon), 1);
		arrange(selmon, false, false);
	}
	printstatus();
	return 0;
}

/* Action: XOR the current monitor's view tag mask with arg->ui (or all-tags if 0), refocusing and re-arranging. */
int32_t toggleview(const Arg *arg) {
	if (!selmon)
		return 0;

	uint32_t newtagset;
	uint32_t target;
	Client *c = NULL;

	target = arg->ui == 0 ? ~0 & TAGMASK : arg->ui;

	newtagset = selmon->tagset[selmon->seltags] ^ (target & TAGMASK);

	if (newtagset) {
		selmon->tagset[selmon->seltags] = newtagset;
		focusclient(focustop(selmon), 1);
		wl_list_for_each(c, &clients, link) {
			if (VISIBLEON(c, selmon) && ISTILED(c)) {
				set_size_per(selmon, c);
			}
		}
		arrange(selmon, false, false);
	}
	printstatus();
	return 0;
}

/* Action: view the tag immediately to the left of the current one. */
int32_t viewtoleft(const Arg *arg) {
	if (!selmon)
		return 0;

	uint32_t target = selmon->tagset[selmon->seltags];

	if (selmon->isoverview || selmon->pertag->curtag == 0) {
		return 0;
	}

	target >>= 1;

	if (target == 0) {
		return 0;
	}

	if (!selmon || (target) == selmon->tagset[selmon->seltags])
		return 0;

	view(&(Arg){.ui = target & TAGMASK, .i = arg->i}, true);
	return 0;
}

/* Action: view the tag immediately to the right of the current one. */
int32_t viewtoright(const Arg *arg) {
	if (!selmon)
		return 0;

	if (selmon->isoverview || selmon->pertag->curtag == 0) {
		return 0;
	}
	uint32_t target = selmon->tagset[selmon->seltags];
	target <<= 1;

	if (!selmon || (target) == selmon->tagset[selmon->seltags])
		return 0;
	if (!(target & TAGMASK)) {
		return 0;
	}

	view(&(Arg){.ui = target & TAGMASK, .i = arg->i}, true);
	return 0;
}

/* Action: view the nearest tag to the left that contains at least one client. */
int32_t viewtoleft_have_client(const Arg *arg) {
	if (!selmon)
		return 0;

	uint32_t n;
	uint32_t current = get_tags_first_tag_num(selmon->tagset[selmon->seltags]);
	bool found = false;

	if (selmon->isoverview) {
		return 0;
	}

	if (current <= 1)
		return 0;

	for (n = current - 1; n >= 1; n--) {
		if (get_tag_status(n, selmon)) {
			found = true;
			break;
		}
	}

	if (found)
		view(&(Arg){.ui = (1 << (n - 1)) & TAGMASK, .i = arg->i}, true);
	return 0;
}

/* Action: view the nearest tag to the right that contains at least one client. */
int32_t viewtoright_have_client(const Arg *arg) {
	if (!selmon)
		return 0;

	uint32_t n;
	uint32_t current = get_tags_first_tag_num(selmon->tagset[selmon->seltags]);
	bool found = false;

	if (selmon->isoverview) {
		return 0;
	}

	if (current >= LENGTH(tags))
		return 0;

	for (n = current + 1; n <= LENGTH(tags); n++) {
		if (get_tag_status(n, selmon)) {
			found = true;
			break;
		}
	}

	if (found)
		view(&(Arg){.ui = (1 << (n - 1)) & TAGMASK, .i = arg->i}, true);
	return 0;
}

/* Action: switch focus to monitor arg->v and view arg->ui's tag there. */
int32_t viewcrossmon(const Arg *arg) {
	if (!selmon)
		return 0;

	focusmon(&(Arg){.v = arg->v, .i = UNDIR});
	view_in_mon(arg, true, selmon, true);
	return 0;
}

/* Action: tag the focused client to tag arg->ui on monitor arg->v (current monitor uses tag, otherwise tagmon). */
int32_t tagcrossmon(const Arg *arg) {
	if (!selmon || !selmon->sel)
		return 0;

	if (match_monitor_spec(arg->v, selmon)) {
		tag_client(arg, selmon->sel);
		return 0;
	}

	tagmon(&(Arg){.ui = arg->ui, .i = UNDIR, .v = arg->v});
	return 0;
}

/* Action: chord-combo view; first press views arg->ui, subsequent presses OR new tags into the current view. */
int32_t comboview(const Arg *arg) {
	uint32_t newtags = arg->ui & TAGMASK;

	if (!newtags || !selmon)
		return 0;

	if (tag_combo) {
		selmon->tagset[selmon->seltags] |= newtags;
		focusclient(focustop(selmon), 1);
		arrange(selmon, false, false);
	} else {
		tag_combo = true;
		view(&(Arg){.ui = newtags}, false);
	}

	printstatus();
	return 0;
}

/* Action: zoom — promote the focused client to the master area (or swap with the previous master). */
int32_t zoom(const Arg *arg) {
	Client *c = NULL, *sel = focustop(selmon);

	if (!sel || !selmon ||
		!selmon->pertag->ltidxs[selmon->pertag->curtag]->arrange ||
		sel->isfloating)
		return 0;

	wl_list_for_each(c, &clients,
					 link) if (VISIBLEON(c, selmon) && !c->isfloating) {
		if (c != sel)
			break;
		sel = NULL;
	}

	if (&c->link == &clients)
		return 0;

	if (!sel)
		sel = c;
	wl_list_remove(&sel->link);
	wl_list_insert(&clients, &sel->link);

	focusclient(sel, 1);
	arrange(selmon, false, false);
	return 0;
}

/* Action: apply a runtime config option key=value pair (arg->v=key, arg->v2=value) and refresh state. */
int32_t setoption(const Arg *arg) {
	parse_option(&config, arg->v, arg->v2);
	override_config();
	reset_option();
	return 0;
}

/* Action: minimize the focused client (hide from layout, keep state). */
int32_t minimized(const Arg *arg) {
	if (!selmon)
		return 0;

	if (selmon && selmon->isoverview)
		return 0;

	if (selmon->sel && !selmon->sel->isminimized) {
		set_minimized(selmon->sel);
	}
	return 0;
}

/* Restore the monitor's previous-tag bookkeeping after exiting overview mode. */
void fix_mon_tagset_from_overview(Monitor *m) {
	if (m->tagset[m->seltags] == (m->ovbk_prev_tagset & TAGMASK)) {
		m->tagset[m->seltags ^ 1] = m->ovbk_current_tagset;
		m->pertag->prevtag = get_tags_first_tag_num(m->ovbk_current_tagset);
	} else {
		m->tagset[m->seltags ^ 1] = m->ovbk_prev_tagset;
		m->pertag->prevtag = get_tags_first_tag_num(m->ovbk_prev_tagset);
	}
}

/* Action: toggle overview mode — show all clients across all tags at once (or restore previous view). */
int32_t toggleoverview(const Arg *arg) {
	Client *c = NULL;
	if (!selmon)
		return 0;

	if (selmon->isoverview && config.ov_tab_mode && arg->i != 1 &&
		selmon->sel) {
		focusstack(&(Arg){.i = 1});
		return 0;
	}

	selmon->isoverview ^= 1;
	selmon->ov_dim_last_ms = get_now_in_ms();
	if (selmon->wlr_output)
		wlr_output_schedule_frame(selmon->wlr_output);
	uint32_t target;
	uint32_t visible_client_number = 0;

	if (selmon->isoverview) {
		wl_list_for_each(c, &clients, link) if (c && c->mon == selmon &&
												!client_is_unmanaged(c) &&
												!client_is_x11_popup(c) &&
												!c->isunglobal) {
			visible_client_number++;
		}
		if (visible_client_number > 0) {
			selmon->ovbk_current_tagset = selmon->tagset[selmon->seltags];
			selmon->ovbk_prev_tagset = selmon->tagset[selmon->seltags ^ 1];
			target = ~0 & TAGMASK;
			/* Temporarily restore minimized clients so they appear in the
			   overview grid; flagged so we re-minimize the unclicked ones on
			   exit. */
			wl_list_for_each(c, &clients, link) {
				if (c && c->mon == selmon && c->isminimized &&
					!c->iskilling && !c->isunglobal) {
					c->ov_was_minimized = true;
					c->tags = c->mini_restore_tag ? c->mini_restore_tag : 1;
					client_pending_minimized_state(c, 0);
				}
			}
		} else {
			selmon->isoverview ^= 1;
			return 0;
		}
	} else if (!selmon->isoverview && selmon->sel) {
		target = get_tags_first_tag(selmon->sel->tags);
	} else if (!selmon->isoverview && !selmon->sel) {
		target = (1 << (selmon->pertag->prevtag - 1));
		view(&(Arg){.ui = target}, false);
		fix_mon_tagset_from_overview(selmon);
		refresh_monitors_workspaces_status(selmon);
		return 0;
	}

	if (selmon->isoverview) {

		wlr_seat_pointer_clear_focus(seat);
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");

		wl_list_for_each(c, &clients, link) {
			if (c && c->mon == selmon && !client_is_unmanaged(c) &&
				!client_is_x11_popup(c) && !c->isunglobal)
				overview_backup(c);
		}
	} else {
		Client *picked = selmon->sel;
		wl_list_for_each(c, &clients, link) {
			if (c && c->mon == selmon && !c->iskilling &&
				!client_is_unmanaged(c) && !c->isunglobal &&
				!client_is_x11_popup(c) && client_surface(c)->mapped)
				overview_restore(c, &(Arg){.ui = target});
		}
		/* Re-minimize every client that was unminimized only for overview
		   display, except the one the user picked (now selmon->sel). */
		wl_list_for_each(c, &clients, link) {
			if (c && c->mon == selmon && c->ov_was_minimized) {
				c->ov_was_minimized = false;
				if (c != picked)
					set_minimized(c);
			}
		}
	}

	view(&(Arg){.ui = target}, false);
	fix_mon_tagset_from_overview(selmon);
	refresh_monitors_workspaces_status(selmon);
	return 0;
}

/* Action: disable (turn off) the monitor matching arg->v spec. */
int32_t disable_monitor(const Arg *arg) {
	Monitor *m = NULL;
	struct wlr_output_state state = {0};
	wl_list_for_each(m, &mons, link) {
		if (match_monitor_spec(arg->v, m)) {
			wlr_output_state_set_enabled(&state, false);
			wlr_output_commit_state(m->wlr_output, &state);
			m->asleep = 1;
			updatemons(NULL, NULL);
			break;
		}
	}
	return 0;
}

/* Action: enable (turn on) the monitor matching arg->v spec. */
int32_t enable_monitor(const Arg *arg) {
	Monitor *m = NULL;
	struct wlr_output_state state = {0};
	wl_list_for_each(m, &mons, link) {
		if (match_monitor_spec(arg->v, m)) {
			wlr_output_state_set_enabled(&state, true);
			wlr_output_commit_state(m->wlr_output, &state);
			m->asleep = 0;
			updatemons(NULL, NULL);
			break;
		}
	}
	return 0;
}

/* Action: toggle on/off state of the monitor matching arg->v spec. */
int32_t toggle_monitor(const Arg *arg) {
	Monitor *m = NULL;
	struct wlr_output_state state = {0};
	wl_list_for_each(m, &mons, link) {
		if (match_monitor_spec(arg->v, m)) {
			wlr_output_state_set_enabled(&state, !m->wlr_output->enabled);
			wlr_output_commit_state(m->wlr_output, &state);
			m->asleep = !m->wlr_output->enabled;
			updatemons(NULL, NULL);
			break;
		}
	}
	return 0;
}

/* Stack client c onto target_client (or remove from stack) in the scroller layout, honoring layout direction. */
int32_t scroller_apply_stack(Client *c, Client *target_client,
							 int32_t direction) {
	if (!c || !c->mon || c->isfloating || !is_scroller_layout(c->mon))
		return 0;

	Monitor *m = c->mon;
	uint32_t tag = m->pertag->curtag;

	bool is_horizontal = (m->pertag->ltidxs[tag]->id == SCROLLER);

	if (is_horizontal && (direction == UP || direction == DOWN))
		return 0;
	if (!is_horizontal && (direction == LEFT || direction == RIGHT))
		return 0;

	struct TagScrollerState *st = ensure_scroller_state(m, tag);

	struct ScrollerStackNode *cnode = find_scroller_node(st, c);

	if (!cnode)
		return 0;

	struct ScrollerStackNode *tnode =
		target_client ? find_scroller_node(st, target_client) : NULL;

	if (direction == UNDIR && target_client && target_client->mon == c->mon) {
		scroller_insert_stack(c, target_client, false);
		return 0;
	}

	if (cnode->prev_in_stack || cnode->next_in_stack) {
		struct ScrollerStackNode *move_out_refer_node =
			cnode->prev_in_stack ? cnode->prev_in_stack : cnode->next_in_stack;
		scroller_node_remove(st, cnode);

		update_scroller_state(c->mon);

		Client *stack_head =
			scroll_get_stack_head_client(move_out_refer_node->client);
		Client *stack_tail =
			scroll_get_stack_tail_client(move_out_refer_node->client);

		if (direction == LEFT || direction == UP) {
			if (c != stack_head) {
				wl_list_remove(&c->link);
				wl_list_insert(stack_head->link.prev, &c->link);
			}
		} else if (direction == RIGHT || direction == DOWN) {
			if (c != stack_tail) {
				wl_list_remove(&c->link);
				wl_list_insert(&stack_tail->link, &c->link);
			}
		}
		sync_scroller_state_to_clients(m, tag);
		arrange(m, false, false);
		return 0;
	}

	if (!tnode || target_client->mon != c->mon)
		return 0;

	struct ScrollerStackNode *tail = tnode;
	while (tail->next_in_stack)
		tail = tail->next_in_stack;

	scroller_insert_stack(c, tail->client, false);

	if (c != tail->client) {
		wl_list_remove(&c->link);
		wl_list_insert(&tail->client->link, &c->link);
	}
	return 0;
}

/* Action: in scroller layout, stack the focused client onto the neighbor found in arg->i direction. */
int32_t scroller_stack(const Arg *arg) {
	if (!selmon)
		return 0;
	Client *c = selmon->sel;
	if (!c || !c->mon || c->isfloating || !is_scroller_layout(selmon))
		return 0;

	Client *target_client = find_client_by_direction(c, arg, false, true);

	return scroller_apply_stack(c, target_client, arg->i);
}

/* Action: toggle floating state on every visible client to match the inverse of the focused client. */
int32_t toggle_all_floating(const Arg *arg) {
	if (!selmon || !selmon->sel)
		return 0;

	Client *c = NULL;
	bool should_floating = !selmon->sel->isfloating;

	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, selmon)) {

			if (c->isfloating && !should_floating) {
				c->old_master_inner_per = 0.0f;
				c->old_stack_inner_per = 0.0f;
				set_size_per(selmon, c);
			}

			if (c->isfloating != should_floating) {
				setfloating(c, should_floating);
			}
		}
	}
	return 0;
}

/* Set the dwindle split direction of c's leaf to horizontal/vertical (or toggle), and refresh split borders. */
int32_t dwindle_set_split_direction(Client *c, bool istoggle, bool horizontal) {

	const Layout *layout = c->mon->pertag->ltidxs[c->mon->pertag->curtag];

	if (layout->id != DWINDLE)
		return 0;

	DwindleNode **root = &selmon->pertag->dwindle_root[selmon->pertag->curtag];
	DwindleNode *leaf = dwindle_find_leaf(*root, c);

	if (!leaf)
		return 0;

	if (istoggle) {
		leaf->custom_leaf_split_h = !leaf->custom_leaf_split_h;
	} else if (horizontal) {
		leaf->custom_leaf_split_h = true;
	} else {
		leaf->custom_leaf_split_h = false;
	}
	bool hit_no_border = check_hit_no_border(c);
	apply_split_border(c, hit_no_border);
	return 0;
}

/* Action: toggle the dwindle split direction (horizontal/vertical) of the focused client's leaf. */
int32_t dwindle_toggle_split_direction(const Arg *arg) {
	if (!selmon || !selmon->sel)
		return 0;

	Client *c = selmon->sel;
	if (!c || !c->mon || c->isfloating)
		return 0;
	return dwindle_set_split_direction(selmon->sel, true, false);
}

/* Action: force the focused client's dwindle leaf to split horizontally. */
int32_t dwindle_split_horizontal(const Arg *arg) {
	if (!selmon || !selmon->sel)
		return 0;

	Client *c = selmon->sel;
	if (!c || !c->mon || c->isfloating)
		return 0;
	return dwindle_set_split_direction(selmon->sel, false, true);
}

/* Action: force the focused client's dwindle leaf to split vertically. */
int32_t dwindle_split_vertical(const Arg *arg) {
	if (!selmon || !selmon->sel)
		return 0;

	Client *c = selmon->sel;
	if (!c || !c->mon || c->isfloating)
		return 0;
	return dwindle_set_split_direction(selmon->sel, false, false);
}
