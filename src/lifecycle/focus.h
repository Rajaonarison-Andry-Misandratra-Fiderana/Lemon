#pragma once

/* Focus path. Reassigns keyboard focus, raises the new client to the
   top, handles cycler-promoted-tile drop, suspends/resumes the
   xdg-suspended flag, drives the focused/unfocused opacity animation,
   updates foreign-toplevel activated state, and re-applies pointer
   constraints. Single-TU header included once. */

void focusclient(Client *c, int32_t lift) {

	Client *last_focus_client = NULL;
	Monitor *um = NULL;

	struct wlr_surface *old_keyboard_focus_surface =
		seat->keyboard_state.focused_surface;

	if (locked)
		return;

	/* The window cycler may have hoisted the last picked tile to LyrTop
	   so it would draw over floating siblings. Drop it back into its
	   native layer as soon as focus moves anywhere else. */
	if (cycler_raised_tile && cycler_raised_tile != c)
		cycler_drop_raised_tile();

	if (c && c->iskilling)
		return;

	if (c && !client_surface(c)->mapped)
		return;

	if (c && client_should_ignore_focus(c) && client_is_x11_popup(c))
		return;

	if (c && c->nofocus)
		return;

	if (c && lift)
		wlr_scene_node_raise_to_top(&c->scene->node);

	if (c && client_surface(c) == old_keyboard_focus_surface && selmon &&
		selmon->sel)
		return;

	if (c) c->focus_opacity_dirty = true;
	if (selmon && selmon->sel) selmon->sel->focus_opacity_dirty = true;

	if (selmon && selmon->sel && selmon->sel != c) {
		CLIENT_SET_FOREIGN_ACTIVATED(selmon->sel, false);
	}

	if (c && !c->iskilling && !client_is_unmanaged(c) && c->mon) {

		last_focus_client = selmon->sel;
		selmon = c->mon;
		selmon->prevsel = selmon->sel;
		selmon->sel = c;
		c->isfocusing = true;
		c->last_active_ms = (uint32_t)get_now_in_ms();
		/* Wake the client from xdg suspended if we put it to sleep
		   earlier via the idle hibernation scanner. */
		if (c->suspended_sent && !client_is_x11(c))
			client_set_suspended(c, false);
		recompute_render_tiers();

		check_keep_idle_inhibit(c);

		if (last_focus_client && !last_focus_client->iskilling &&
			last_focus_client != c) {
			last_focus_client->isfocusing = false;
			client_set_unfocused_opacity_animation(last_focus_client);
			qos_demote(last_focus_client);
		}

		qos_promote(c);
		client_set_focused_opacity_animation(c);

		if (c && selmon->prevsel &&
			(selmon->prevsel->tags & selmon->tagset[selmon->seltags]) &&
			(c->tags & selmon->tagset[selmon->seltags]) && !c->isfloating &&
			is_scroller_layout(selmon)) {
			arrange(selmon, false, false);
		}

		wl_list_remove(&c->flink);
		wl_list_insert(&fstack, &c->flink);

		c->isurgent = 0;
	}

	wl_list_for_each(um, &mons, link) {
		if (um->wlr_output->enabled && um != selmon && um->sel &&
			!um->sel->iskilling && um->sel->isfocusing) {

			um->sel->isfocusing = false;
			client_set_unfocused_opacity_animation(um->sel);

			CLIENT_SET_FOREIGN_ACTIVATED(um->sel, false);
		}
	}

	if (c && !c->iskilling)
		CLIENT_SET_FOREIGN_ACTIVATED(c, true);

	if (old_keyboard_focus_surface &&
		(!c || client_surface(c) != old_keyboard_focus_surface)) {

		Client *w = NULL;
		LayerSurface *l = NULL;
		int32_t type =
			toplevel_from_wlr_surface(old_keyboard_focus_surface, &w, &l);
		if (type == LayerShell && l->scene->node.enabled &&
			l->layer_surface->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP &&
			l == exclusive_focus) {
			return;
		} else if (w && w == exclusive_focus && client_wants_focus(w)) {
			return;

		} else if (w && !client_is_unmanaged(w) &&
				   (!c || !client_wants_focus(c))) {
			client_activate_surface(old_keyboard_focus_surface, 0);
		}
	}
	printstatus();

	if (!c) {

		if (selmon && selmon->sel &&
			(!VISIBLEON(selmon->sel, selmon) || selmon->sel->iskilling ||
			 !client_surface(selmon->sel)->mapped))
			selmon->sel = NULL;

		dwl_im_relay_set_focus(dwl_input_method_relay, NULL);
		wlr_seat_keyboard_notify_clear_focus(seat);
		if (active_constraint) {
			cursorconstrain(NULL);
		}
		return;
	}

	motionnotify(0, NULL, 0, 0, 0, 0);

	dwl_im_relay_set_focus(dwl_input_method_relay, client_surface(c));

	client_notify_enter(client_surface(c), wlr_seat_get_keyboard(seat));

	client_activate_surface(client_surface(c), 1);

	if (active_constraint && active_constraint->surface != client_surface(c)) {
		cursorconstrain(NULL);
	}

	struct wlr_pointer_constraint_v1 *constraint;
	wl_list_for_each(constraint, &pointer_constraints->constraints, link) {
		if (constraint->surface == client_surface(c)) {
			cursorconstrain(constraint);
			break;
		}
	}
}
