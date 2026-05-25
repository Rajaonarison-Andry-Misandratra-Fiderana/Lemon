#pragma once

/* Input-device wiring: libinput pointer/trackpad acceleration and tap
   settings, pointer/switch device allocation, pointer-constraint
   listener, and the shared destroyinputdevice cleanup. Single-TU
   header included once. */

void destroyinputdevice(struct wl_listener *listener, void *data) {
	InputDevice *input_dev =
		wl_container_of(listener, input_dev, destroy_listener);

	if (input_dev->device_data) {

		switch (input_dev->wlr_device->type) {
		case WLR_INPUT_DEVICE_SWITCH: {
			Switch *sw = (Switch *)input_dev->device_data;

			wl_list_remove(&sw->toggle.link);

			free(sw);
			break;
		}

		default:
			break;
		}
		input_dev->device_data = NULL;
	}

	wl_list_remove(&input_dev->link);

	wl_list_remove(&input_dev->destroy_listener.link);

	free(input_dev);
}

void pointer_set_accel(struct libinput_device *device, bool natural_scrolling,
					   uint32_t mouse_accel_profile, double mouse_accel_speed) {
	libinput_device_config_scroll_set_natural_scroll_enabled(device,
															 natural_scrolling);
	if (mouse_accel_profile &&
		libinput_device_config_accel_is_available(device)) {
		libinput_device_config_accel_set_profile(device, mouse_accel_profile);
		libinput_device_config_accel_set_speed(device, mouse_accel_speed);
	} else {

		libinput_device_config_accel_set_profile(device, 1);
		libinput_device_config_accel_set_profile(device, 0);
		libinput_device_config_accel_set_speed(device, 0);
	}
}

void configure_pointer(struct libinput_device *device) {
	if (libinput_device_config_tap_get_finger_count(device)) {
		libinput_device_config_tap_set_enabled(device, config.tap_to_click);
		libinput_device_config_tap_set_drag_enabled(device,
													config.tap_and_drag);
		libinput_device_config_tap_set_drag_lock_enabled(device,
														 config.drag_lock);
		libinput_device_config_tap_set_button_map(device, config.button_map);
		pointer_set_accel(device, config.trackpad_natural_scrolling,
						  config.trackpad_accel_profile,
						  config.trackpad_accel_speed);
	} else {
		pointer_set_accel(device, config.mouse_natural_scrolling,
						  config.mouse_accel_profile, config.mouse_accel_speed);
	}

	if (libinput_device_config_dwt_is_available(device))
		libinput_device_config_dwt_set_enabled(device,
											   config.disable_while_typing);

	if (libinput_device_config_left_handed_is_available(device))
		libinput_device_config_left_handed_set(device, config.left_handed);

	if (libinput_device_config_middle_emulation_is_available(device))
		libinput_device_config_middle_emulation_set_enabled(
			device, config.middle_button_emulation);

	if (libinput_device_config_scroll_get_methods(device) !=
		LIBINPUT_CONFIG_SCROLL_NO_SCROLL)
		libinput_device_config_scroll_set_method(device, config.scroll_method);
	if (libinput_device_config_scroll_get_methods(device) ==
		LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN)
		libinput_device_config_scroll_set_button(device, config.scroll_button);

	if (libinput_device_config_click_get_methods(device) !=
		LIBINPUT_CONFIG_CLICK_METHOD_NONE)
		libinput_device_config_click_set_method(device, config.click_method);

	if (libinput_device_config_send_events_get_modes(device))
		libinput_device_config_send_events_set_mode(device,
													config.send_events_mode);
}

void createpointer(struct wlr_pointer *pointer) {

	struct libinput_device *device = NULL;

	if (wlr_input_device_is_libinput(&pointer->base) &&
		(device = wlr_libinput_get_device_handle(&pointer->base))) {

		configure_pointer(device);

		InputDevice *input_dev = calloc(1, sizeof(InputDevice));
		input_dev->wlr_device = &pointer->base;
		input_dev->libinput_device = device;

		input_dev->destroy_listener.notify = destroyinputdevice;
		wl_signal_add(&pointer->base.events.destroy,
					  &input_dev->destroy_listener);

		wl_list_insert(&inputdevices, &input_dev->link);
	}
	wlr_cursor_attach_input_device(cursor, &pointer->base);
}

void switch_toggle(struct wl_listener *listener, void *data) {

	Switch *sw = wl_container_of(listener, sw, toggle);

	struct wlr_switch_toggle_event *event = data;
	SwitchBinding *s;
	int32_t ji;

	for (ji = 0; ji < config.switch_bindings_count; ji++) {
		if (config.switch_bindings_count < 1)
			break;
		s = &config.switch_bindings[ji];
		if (event->switch_state == s->fold && s->func) {
			s->func(&s->arg);
			return;
		}
	}
}

void createswitch(struct wlr_switch *switch_device) {

	struct libinput_device *device = NULL;

	if (wlr_input_device_is_libinput(&switch_device->base) &&
		(device = wlr_libinput_get_device_handle(&switch_device->base))) {

		InputDevice *input_dev = calloc(1, sizeof(InputDevice));
		input_dev->wlr_device = &switch_device->base;
		input_dev->libinput_device = device;
		input_dev->device_data = NULL;

		input_dev->destroy_listener.notify = destroyinputdevice;
		wl_signal_add(&switch_device->base.events.destroy,
					  &input_dev->destroy_listener);

		Switch *sw = calloc(1, sizeof(Switch));
		sw->wlr_switch = switch_device;
		sw->toggle.notify = switch_toggle;
		sw->input_dev = input_dev;

		input_dev->device_data = sw;

		wl_signal_add(&switch_device->events.toggle, &sw->toggle);

		wl_list_insert(&inputdevices, &input_dev->link);
	}
}

void createpointerconstraint(struct wl_listener *listener, void *data) {
	PointerConstraint *pointer_constraint =
		ecalloc(1, sizeof(*pointer_constraint));
	pointer_constraint->constraint = data;
	LISTEN(&pointer_constraint->constraint->events.destroy,
		   &pointer_constraint->destroy, destroypointerconstraint);

	if (!selmon || !selmon->sel)
		return;

	struct wlr_surface *focused_surface = client_surface(selmon->sel);
	if (focused_surface &&
		focused_surface == pointer_constraint->constraint->surface) {
		cursorconstrain(pointer_constraint->constraint);
	}
}
