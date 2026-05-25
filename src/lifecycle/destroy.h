#pragma once

/* Cursor-state helpers (constrain, frame, warp-to-hint) and the small
   destroy / cleanup listeners: drag icon, idle inhibitor, layer-shell
   scene node, session-lock + lock surface, pointer constraint,
   keyboard group. Single-TU header included once. */

void cursorconstrain(struct wlr_pointer_constraint_v1 *constraint) {
	if (active_constraint == constraint)
		return;

	if (active_constraint) {
		if (constraint == NULL) {
			cursorwarptohint();
		}
		wlr_pointer_constraint_v1_send_deactivated(active_constraint);
	}

	active_constraint = constraint;

	if (constraint) {
		wlr_pointer_constraint_v1_send_activated(constraint);
	}
}

void cursorframe(struct wl_listener *listener, void *data) {

	wlr_seat_pointer_notify_frame(seat);
}

void cursorwarptohint(void) {
	Client *c = NULL;
	double sx = active_constraint->current.cursor_hint.x;
	double sy = active_constraint->current.cursor_hint.y;

	toplevel_from_wlr_surface(active_constraint->surface, &c, NULL);
	if (c && active_constraint->current.cursor_hint.enabled) {
		wlr_cursor_warp(cursor, NULL, sx + c->geom.x + c->bw,
						sy + c->geom.y + c->bw);
		wlr_seat_pointer_warp(active_constraint->seat, sx, sy);
	}
}

void destroydragicon(struct wl_listener *listener, void *data) {

	focusclient(focustop(selmon), 1);
	motionnotify(0, NULL, 0, 0, 0, 0);
	wl_list_remove(&listener->link);
	free(listener);
}

void destroyidleinhibitor(struct wl_listener *listener, void *data) {

	checkidleinhibitor(wlr_surface_get_root_surface(data));
	wl_list_remove(&listener->link);
	free(listener);
}

void destroylayernodenotify(struct wl_listener *listener, void *data) {
	LayerSurface *l = wl_container_of(listener, l, destroy);

	wl_list_remove(&l->link);
	wl_list_remove(&l->destroy.link);
	wl_list_remove(&l->map.link);
	wl_list_remove(&l->unmap.link);
	wl_list_remove(&l->surface_commit.link);
	wlr_scene_node_destroy(&l->popups->node);
	free(l);
}

void destroylock(SessionLock *lock, int32_t unlock) {
	wlr_seat_keyboard_notify_clear_focus(seat);
	if ((locked = !unlock))
		goto destroy;

	if (locked_bg->node.enabled) {
		wlr_scene_node_set_enabled(&locked_bg->node, false);
	}

	focusclient(focustop(selmon), 0);
	motionnotify(0, NULL, 0, 0, 0, 0);

destroy:
	wl_list_remove(&lock->new_surface.link);
	wl_list_remove(&lock->unlock.link);
	wl_list_remove(&lock->destroy.link);

	wlr_scene_node_destroy(&lock->scene->node);
	cur_lock = NULL;
	free(lock);
}

void destroylocksurface(struct wl_listener *listener, void *data) {
	Monitor *m = wl_container_of(listener, m, destroy_lock_surface);
	struct wlr_session_lock_surface_v1 *surface,
		*lock_surface = m->lock_surface;

	m->lock_surface = NULL;
	wl_list_remove(&m->destroy_lock_surface.link);

	if (lock_surface->surface != seat->keyboard_state.focused_surface) {
		if (exclusive_focus && !locked) {
			reset_exclusive_layers_focus(m);
		}
		return;
	}

	if (locked && cur_lock && !wl_list_empty(&cur_lock->surfaces)) {
		surface = wl_container_of(cur_lock->surfaces.next, surface, link);
		client_notify_enter(surface->surface, wlr_seat_get_keyboard(seat));
	} else if (!locked) {
		reset_exclusive_layers_focus(selmon);
	} else {
		wlr_seat_keyboard_clear_focus(seat);
	}
}

void destroypointerconstraint(struct wl_listener *listener, void *data) {
	PointerConstraint *pointer_constraint =
		wl_container_of(listener, pointer_constraint, destroy);

	if (active_constraint == pointer_constraint->constraint) {
		cursorwarptohint();
		active_constraint = NULL;
	}

	wl_list_remove(&pointer_constraint->destroy.link);
	free(pointer_constraint);
}

void destroysessionlock(struct wl_listener *listener, void *data) {
	SessionLock *lock = wl_container_of(listener, lock, destroy);
	destroylock(lock, 0);
}

void destroykeyboardgroup(struct wl_listener *listener, void *data) {
	KeyboardGroup *group = wl_container_of(listener, group, destroy);
	wl_event_source_remove(group->key_repeat_source);
	wl_list_remove(&group->key.link);
	wl_list_remove(&group->modifiers.link);
	wl_list_remove(&group->destroy.link);
	wlr_keyboard_group_destroy(group->wlr_group);
	free(group);
}
