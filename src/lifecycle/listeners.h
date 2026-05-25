#pragma once

/* Lifecycle / listener glue for xdg surfaces, popups, decorations,
   idle inhibitors, keyboards, layer-shell, session-lock surfaces, and
   the output-mode helpers used by createmon. Single-TU header
   included exactly once from lemon.c. */

/* Listener: xdg surface committed - handle initial commit setup and resize on geometry change. */
void commitnotify(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, commit);
	struct wlr_box *new_geo;

	if (c->surface.xdg->initial_commit) {

		init_client_properties(c);
		applyrules(c);
		if (c->mon) {
			client_set_scale(client_surface(c), c->mon->wlr_output->scale);
		}
		setmon(c, NULL, 0,
			   true);

		/* Surface-cache size hint disabled by default: browser-class apps
		   (Firefox, Zen, Chromium) interpret a stale geometry from the
		   previous session as "restore previous window state", which on
		   relaunch can produce a duplicate session-restore window and a
		   visibly slower first paint. The cache itself is still populated
		   on unmap so a future opt-in flag can re-enable the hint. */
		(void)client_get_appid;

		uint32_t serial = wlr_xdg_surface_schedule_configure(c->surface.xdg);
		if (serial > 0) {
			c->configure_serial = serial;
		}

		uint32_t wm_caps = WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN;

		if (!c->ignore_minimize)
			wm_caps |= WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE;

		if (!c->ignore_maximize)
			wm_caps |= WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE;

		wlr_xdg_toplevel_set_wm_capabilities(c->surface.xdg->toplevel, wm_caps);

		if (c->mon) {
			wlr_xdg_toplevel_set_bounds(c->surface.xdg->toplevel,
										c->mon->w.width - 2 * c->bw,
										c->mon->w.height - 2 * c->bw);
		}

		if (c->decoration)
			requestdecorationmode(&c->set_decoration_mode, c->decoration);
		return;
	}

	if (!c || c->iskilling || c->animation.tagouting || c->animation.tagouted ||
		c->animation.tagining)
		return;

	if (c->configure_serial &&
		c->configure_serial <= c->surface.xdg->current.configure_serial)
		c->configure_serial = 0;

	client_refresh_content_type(c);

	if (!c->dirty) {
		new_geo = &c->surface.xdg->geometry;
		c->dirty = new_geo->width != c->geom.width - 2 * c->bw ||
				   new_geo->height != c->geom.height - 2 * c->bw ||
				   new_geo->x != 0 || new_geo->y != 0;
	}

	if (c == grabc || !c->dirty)
		return;

	resize(c, c->geom, 0);

	new_geo = &c->surface.xdg->geometry;
	c->dirty = new_geo->width != c->geom.width - 2 * c->bw ||
			   new_geo->height != c->geom.height - 2 * c->bw ||
			   new_geo->x != 0 || new_geo->y != 0;
}

/* Listener: xdg-decoration destroyed - detach decoration-related listeners from the client. */
void destroydecoration(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, destroy_decoration);

	wl_list_remove(&c->destroy_decoration.link);
	wl_list_remove(&c->set_decoration_mode.link);
}

/* Constrain a popup to its toplevel's monitor; return true if it must be destroyed. */
static bool popup_unconstrain(Popup *popup) {
	struct wlr_xdg_popup *wlr_popup = popup->wlr_popup;
	Client *c = NULL;
	LayerSurface *l = NULL;
	int32_t type = -1;

	if (!wlr_popup || !wlr_popup->parent) {
		return false;
	}

	struct wlr_scene_node *parent_node = wlr_popup->parent->data;
	if (!parent_node) {
		wlr_log(WLR_ERROR, "Popup parent has no scene node");
		return false;
	}

	type = toplevel_from_wlr_surface(wlr_popup->base->surface, &c, &l);
	if ((l && !l->mon) || (c && !c->mon)) {
		return true;
	}

	struct wlr_box usable = type == LayerShell ? l->mon->m : c->mon->w;

	int lx, ly;
	struct wlr_box constraint_box;

	if (type == LayerShell) {
		wlr_scene_node_coords(&l->scene_layer->tree->node, &lx, &ly);
		constraint_box.x = usable.x - lx;
		constraint_box.y = usable.y - ly;
		constraint_box.width = usable.width;
		constraint_box.height = usable.height;
	} else {
		constraint_box.x =
			usable.x - (c->geom.x + c->bw - c->surface.xdg->current.geometry.x);
		constraint_box.y =
			usable.y - (c->geom.y + c->bw - c->surface.xdg->current.geometry.y);
		constraint_box.width = usable.width;
		constraint_box.height = usable.height;
	}

	wlr_xdg_popup_unconstrain_from_box(wlr_popup, &constraint_box);
	return false;
}

/* Listener: xdg popup destroyed - remove its listeners and free the Popup wrapper. */
static void destroypopup(struct wl_listener *listener, void *data) {
	Popup *popup = wl_container_of(listener, popup, destroy);
	wl_list_remove(&popup->destroy.link);
	wl_list_remove(&popup->reposition.link);
	free(popup);
}

/* Listener: popup surface initial commit - attach to scene tree and unconstrain to parent. */
static void commitpopup(struct wl_listener *listener, void *data) {
	Popup *popup = wl_container_of(listener, popup, commit);

	struct wlr_surface *surface = data;
	bool should_destroy = false;
	struct wlr_xdg_popup *wlr_popup =
		wlr_xdg_popup_try_from_wlr_surface(surface);

	if (!wlr_popup->base->initial_commit)
		return;

	if (!wlr_popup->parent || !wlr_popup->parent->data) {
		should_destroy = true;
		goto cleanup_popup_commit;
	}

	wlr_scene_node_raise_to_top(wlr_popup->parent->data);

	wlr_popup->base->surface->data =
		wlr_scene_xdg_surface_create(wlr_popup->parent->data, wlr_popup->base);

	popup->wlr_popup = wlr_popup;

	should_destroy = popup_unconstrain(popup);

cleanup_popup_commit:

	wl_list_remove(&popup->commit.link);
	popup->commit.notify = NULL;

	if (should_destroy) {
		wlr_xdg_popup_destroy(wlr_popup);
	}
}

/* Listener: popup repositioned - re-run unconstrain to keep it on screen. */
static void repositionpopup(struct wl_listener *listener, void *data) {
	Popup *popup = wl_container_of(listener, popup, reposition);
	(void)popup_unconstrain(popup);
}

/* Listener: new xdg popup - allocate Popup state and wire up destroy/commit/reposition listeners. */
static void createpopup(struct wl_listener *listener, void *data) {
	struct wlr_xdg_popup *wlr_popup = data;

	Popup *popup = calloc(1, sizeof(Popup));
	if (!popup)
		return;

	popup->destroy.notify = destroypopup;
	wl_signal_add(&wlr_popup->events.destroy, &popup->destroy);

	popup->commit.notify = commitpopup;
	wl_signal_add(&wlr_popup->base->surface->events.commit, &popup->commit);

	popup->reposition.notify = repositionpopup;
	wl_signal_add(&wlr_popup->events.reposition, &popup->reposition);
}

/* Listener: new xdg-toplevel decoration - attach mode/destroy listeners and apply mode. */
void createdecoration(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel_decoration_v1 *deco = data;
	Client *c = deco->toplevel->base->data;
	c->decoration = deco;

	LISTEN(&deco->events.request_mode, &c->set_decoration_mode,
		   requestdecorationmode);
	LISTEN(&deco->events.destroy, &c->destroy_decoration, destroydecoration);

	requestdecorationmode(&c->set_decoration_mode, deco);
}

/* Listener: new idle inhibitor - listen for its destruction and recompute inhibit state. */
void createidleinhibitor(struct wl_listener *listener, void *data) {
	struct wlr_idle_inhibitor_v1 *idle_inhibitor = data;
	LISTEN_STATIC(&idle_inhibitor->events.destroy, destroyidleinhibitor);

	checkidleinhibitor(NULL);
}

/* Register a new keyboard with the shared keyboard group and configure its keymap. */
void createkeyboard(struct wlr_keyboard *keyboard) {

	struct libinput_device *device = NULL;

	if (wlr_input_device_is_libinput(&keyboard->base) &&
		(device = wlr_libinput_get_device_handle(&keyboard->base))) {

		InputDevice *input_dev = calloc(1, sizeof(InputDevice));
		input_dev->wlr_device = &keyboard->base;
		input_dev->libinput_device = device;
		input_dev->device_data = keyboard;

		input_dev->destroy_listener.notify = destroyinputdevice;
		wl_signal_add(&keyboard->base.events.destroy,
					  &input_dev->destroy_listener);

		wl_list_insert(&inputdevices, &input_dev->link);
	}

	wlr_keyboard_set_keymap(keyboard, kb_group->wlr_group->keyboard.keymap);

	wlr_keyboard_notify_modifiers(keyboard, 0, 0, locked_mods, 0);

	wlr_keyboard_group_add_keyboard(kb_group->wlr_group, keyboard);
}

/* Allocate a keyboard group, compile xkb keymap, set locked mods, and wire key listeners. */
KeyboardGroup *createkeyboardgroup(void) {
	KeyboardGroup *group = ecalloc(1, sizeof(*group));
	struct xkb_context *context;
	struct xkb_keymap *keymap;

	group->wlr_group = wlr_keyboard_group_create();
	group->wlr_group->data = group;

	context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!(keymap = xkb_keymap_new_from_names(context, &config.xkb_rules,
											 XKB_KEYMAP_COMPILE_NO_FLAGS)))
		die("failed to compile keymap");

	wlr_keyboard_set_keymap(&group->wlr_group->keyboard, keymap);

	if (config.numlockon) {
		xkb_mod_index_t mod_index =
			xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_NUM);
		if (mod_index != XKB_MOD_INVALID)
			locked_mods |= (uint32_t)1 << mod_index;
	}

	if (config.capslock) {
		xkb_mod_index_t mod_index =
			xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_CAPS);
		if (mod_index != XKB_MOD_INVALID)
			locked_mods |= (uint32_t)1 << mod_index;
	}

	if (locked_mods)
		wlr_keyboard_notify_modifiers(&group->wlr_group->keyboard, 0, 0,
									  locked_mods, 0);

	xkb_keymap_unref(keymap);
	xkb_context_unref(context);

	wlr_keyboard_set_repeat_info(&group->wlr_group->keyboard,
								 config.repeat_rate, config.repeat_delay);

	LISTEN(&group->wlr_group->keyboard.events.key, &group->key, keypress);
	LISTEN(&group->wlr_group->keyboard.events.modifiers, &group->modifiers,
		   keypressmod);

	group->key_repeat_source =
		wl_event_loop_add_timer(event_loop, keyrepeat, group);

	wlr_seat_set_keyboard(seat, &group->wlr_group->keyboard);
	return group;
}

/* Listener: new wlr-layer-shell surface - build scene nodes and attach map/commit/unmap. */
void createlayersurface(struct wl_listener *listener, void *data) {
	struct wlr_layer_surface_v1 *layer_surface = data;
	LayerSurface *l = NULL;
	struct wlr_surface *surface = layer_surface->surface;
	struct wlr_scene_tree *scene_layer =
		layers[layermap[layer_surface->pending.layer]];

	if (!layer_surface->output &&
		!(layer_surface->output = selmon ? selmon->wlr_output : NULL)) {
		wlr_layer_surface_v1_destroy(layer_surface);
		return;
	}

	l = layer_surface->data = ecalloc(1, sizeof(*l));
	l->type = LayerShell;
	LISTEN(&surface->events.map, &l->map, maplayersurfacenotify);
	LISTEN(&surface->events.commit, &l->surface_commit,
		   commitlayersurfacenotify);
	LISTEN(&surface->events.unmap, &l->unmap, unmaplayersurfacenotify);

	l->layer_surface = layer_surface;
	l->mon = layer_surface->output->data;
	l->scene_layer =
		wlr_scene_layer_surface_v1_create(scene_layer, layer_surface);
	l->scene = l->scene_layer->tree;
	l->popups = surface->data = wlr_scene_tree_create(
		layer_surface->current.layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP
			? layers[LyrTop]
			: scene_layer);
	l->scene->node.data = l->popups->node.data = l;

	LISTEN(&l->scene->node.events.destroy, &l->destroy, destroylayernodenotify);

	wl_list_insert(&l->mon->layers[layer_surface->pending.layer], &l->link);
	wlr_surface_send_enter(surface, layer_surface->output);
}

void createlocksurface(struct wl_listener *listener, void *data) {
	SessionLock *lock = wl_container_of(listener, lock, new_surface);
	struct wlr_session_lock_surface_v1 *lock_surface = data;
	Monitor *m = lock_surface->output->data;
	struct wlr_scene_tree *scene_tree = lock_surface->surface->data =
		wlr_scene_subsurface_tree_create(lock->scene, lock_surface->surface);
	m->lock_surface = lock_surface;

	wlr_scene_node_set_position(&scene_tree->node, m->m.x, m->m.y);
	wlr_session_lock_surface_v1_configure(lock_surface, m->m.width,
										  m->m.height);

	LISTEN(&lock_surface->events.destroy, &m->destroy_lock_surface,
		   destroylocksurface);

	if (m == selmon)
		client_notify_enter(lock_surface->surface, wlr_seat_get_keyboard(seat));
}

struct wlr_output_mode *get_nearest_output_mode(struct wlr_output *output,
												int32_t width, int32_t height,
												float refresh) {
	struct wlr_output_mode *mode, *nearest_mode = NULL;
	float min_diff = 99999.0f;

	wl_list_for_each(mode, &output->modes, link) {
		if (mode->width == width && mode->height == height) {
			float mode_refresh = mode->refresh / 1000.0f;
			float diff = fabsf(mode_refresh - refresh);

			if (diff < min_diff) {
				min_diff = diff;
				nearest_mode = mode;
			}
		}
	}

	return nearest_mode;
}

void enable_adaptive_sync(Monitor *m, struct wlr_output_state *state) {
	wlr_output_state_set_adaptive_sync_enabled(state, true);
	if (!wlr_output_test_state(m->wlr_output, state)) {
		wlr_output_state_set_adaptive_sync_enabled(state, false);
		wlr_log(WLR_DEBUG, "failed to enable adaptive sync for output %s",
				m->wlr_output->name);
	} else {
		wlr_log(WLR_INFO, "adaptive sync enabled for output %s",
				m->wlr_output->name);
	}
}

bool monitor_matches_rule(Monitor *m, const ConfigMonitorRule *rule) {
	if (rule->name != NULL && !regex_match(rule->name, m->wlr_output->name))
		return false;
	if (rule->make != NULL && (m->wlr_output->make == NULL ||
							   strcmp(rule->make, m->wlr_output->make) != 0))
		return false;
	if (rule->model != NULL && (m->wlr_output->model == NULL ||
								strcmp(rule->model, m->wlr_output->model) != 0))
		return false;
	if (rule->serial != NULL &&
		(m->wlr_output->serial == NULL ||
		 strcmp(rule->serial, m->wlr_output->serial) != 0))
		return false;
	return true;
}

bool apply_rule_to_state(Monitor *m, const ConfigMonitorRule *rule,
						 struct wlr_output_state *state, int vrr, int custom) {
	bool mode_set = false;
	if (rule->width > 0 && rule->height > 0 && rule->refresh > 0) {
		struct wlr_output_mode *internal_mode = get_nearest_output_mode(
			m->wlr_output, rule->width, rule->height, rule->refresh);
		if (internal_mode) {
			wlr_output_state_set_mode(state, internal_mode);
			mode_set = true;
		} else if (custom || wlr_output_is_headless(m->wlr_output)) {
			wlr_output_state_set_custom_mode(
				state, rule->width, rule->height,
				(int32_t)roundf(rule->refresh * 1000));
			mode_set = true;
		}
	}
	if (vrr) {
		enable_adaptive_sync(m, state);
	} else {
		wlr_output_state_set_adaptive_sync_enabled(state, false);
	}
	wlr_output_state_set_scale(state, rule->scale);
	wlr_output_state_set_transform(state, rule->rr);
	return mode_set;
}
