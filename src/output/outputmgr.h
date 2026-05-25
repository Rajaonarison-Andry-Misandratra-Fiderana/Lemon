#pragma once

/* wlr_output_management_v1 glue: apply / test a client-requested output
   configuration. Walks heads, builds wlr_output_state per output and
   either tests or commits; updates the output layout for moved heads.
   Single-TU header included once from lemon.c. */

void outputmgrapply(struct wl_listener *listener, void *data) {
	struct wlr_output_configuration_v1 *config = data;
	outputmgrapplyortest(config, 0);
}

void
outputmgrapplyortest(struct wlr_output_configuration_v1 *config, int32_t test) {

	struct wlr_output_configuration_head_v1 *config_head;
	int32_t ok = 1;

	wl_list_for_each(config_head, &config->heads, link) {
		struct wlr_output *wlr_output = config_head->state.output;
		Monitor *m = wlr_output->data;
		struct wlr_output_state state;

		m->asleep = 0;

		wlr_output_state_init(&state);
		wlr_output_state_set_enabled(&state, config_head->state.enabled);
		if (!config_head->state.enabled)
			goto apply_or_test;

		if (config_head->state.mode)
			wlr_output_state_set_mode(&state, config_head->state.mode);
		else
			wlr_output_state_set_custom_mode(
				&state, config_head->state.custom_mode.width,
				config_head->state.custom_mode.height,
				config_head->state.custom_mode.refresh);

		wlr_output_state_set_transform(&state, config_head->state.transform);
		wlr_output_state_set_scale(&state, config_head->state.scale);
		wlr_output_state_set_adaptive_sync_enabled(
			&state, config_head->state.adaptive_sync_enabled);

	apply_or_test:
		ok &= test ? wlr_output_test_state(wlr_output, &state)
				   : wlr_output_commit_state(wlr_output, &state);

		if (!test && wlr_output->enabled &&
			(m->m.x != config_head->state.x || m->m.y != config_head->state.y))
			wlr_output_layout_add(output_layout, wlr_output,
								  config_head->state.x, config_head->state.y);

		wlr_output_state_finish(&state);
	}

	if (ok)
		wlr_output_configuration_v1_send_succeeded(config);
	else
		wlr_output_configuration_v1_send_failed(config);
	wlr_output_configuration_v1_destroy(config);

	updatemons(NULL, NULL);
}

void outputmgrtest(struct wl_listener *listener, void *data) {
	struct wlr_output_configuration_v1 *config = data;
	outputmgrapplyortest(config, 1);
}
