#include <ctype.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

#define CLAMP_INT(x, min, max)                                                 \
	((int32_t)(x) < (int32_t)(min)                                             \
		 ? (int32_t)(min)                                                      \
		 : ((int32_t)(x) > (int32_t)(max) ? (int32_t)(max) : (int32_t)(x)))

#define CLAMP_FLOAT(x, min, max)                                               \
	((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

enum { NUM_TYPE_MINUS, NUM_TYPE_PLUS, NUM_TYPE_DEFAULT };

enum { KEY_TYPE_CODE, KEY_TYPE_SYM };

typedef struct {
	uint32_t keycode1;
	uint32_t keycode2;
	uint32_t keycode3;
} MultiKeycode;

typedef struct {
	xkb_keysym_t keysym;
	MultiKeycode keycode;
	int32_t type;
} KeySymCode;

typedef struct {
	uint32_t mod;
	KeySymCode keysymcode;
	int32_t (*func)(const Arg *);
	Arg arg;
	char mode[28];
	bool iscommonmode;
	bool isdefaultmode;
	bool islockapply;
	bool isreleaseapply;
	bool ispassapply;
} KeyBinding;

typedef struct {
	char *type;
	char *value;
} ConfigEnv;

typedef struct {
	const char *id;
	const char *title;
	uint32_t tags;
	int32_t isfloating;
	int32_t isfullscreen;
	int32_t isfakefullscreen;
	float scroller_proportion;
	const char *animation_type_open;
	const char *animation_type_close;
	const char *layer_animation_type_open;
	const char *layer_animation_type_close;
	int32_t isnoborder;
	int32_t isnoshadow;
	int32_t isnoradius;
	int32_t isnoanimation;
	int32_t isopensilent;
	int32_t istagsilent;
	int32_t isnamedscratchpad;
	int32_t isunglobal;
	int32_t isglobal;
	int32_t isoverlay;
	int32_t allow_shortcuts_inhibit;
	int32_t ignore_maximize;
	int32_t ignore_minimize;
	int32_t isnosizehint;
	int32_t indleinhibit_when_focus;
	char *monitor;
	int32_t offsetx;
	int32_t offsety;
	float width;
	float height;
	int32_t nofocus;
	int32_t nofadein;
	int32_t nofadeout;
	int32_t no_force_center;
	int32_t isterm;
	int32_t allow_csd;
	int32_t force_fakemaximize;
	int32_t force_tiled_state;
	int32_t force_tearing;
	int32_t noswallow;
	int32_t noblur;
	float focused_opacity;
	float unfocused_opacity;
	float scroller_proportion_single;
	uint32_t passmod;
	xkb_keysym_t keysym;
	KeyBinding globalkeybinding;
} ConfigWinRule;

typedef struct {
	const char *name;
	char *make, *model, *serial;
	int32_t rr;
	float scale;
	int32_t x, y;
	int32_t width, height;
	float refresh;
	int32_t vrr;
	int32_t custom;
} ConfigMonitorRule;

#define CHVT(n)                                                                \
	{                                                                          \
		WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT,                                  \
			{.keysym = XKB_KEY_XF86Switch_VT_##n, .type = KEY_TYPE_SYM}, chvt, \
		{                                                                      \
			.ui = (n)                                                          \
		}                                                                      \
	}

KeyBinding default_key_bindings[] = {CHVT(1), CHVT(2),	CHVT(3),  CHVT(4),
									 CHVT(5), CHVT(6),	CHVT(7),  CHVT(8),
									 CHVT(9), CHVT(10), CHVT(11), CHVT(12)};

typedef struct {
	uint32_t mod;
	uint32_t button;
	int32_t (*func)(const Arg *);
	Arg arg;
} MouseBinding;

typedef struct {
	uint32_t mod;
	uint32_t dir;
	int32_t (*func)(const Arg *);
	Arg arg;
} AxisBinding;

typedef struct {
	uint32_t fold;
	int32_t (*func)(const Arg *);
	Arg arg;
} SwitchBinding;

typedef struct {
	uint32_t mod;
	uint32_t motion;
	uint32_t fingers_count;
	int32_t (*func)(const Arg *);
	Arg arg;
} GestureBinding;

typedef struct {
	uint32_t timeout_ms;
	int32_t (*func)(const Arg *);
	Arg arg;
} IdleBinding;

/* Built-in action run when idle_timeout elapses. */
enum IdleAction {
	IDLE_ACTION_OFF = 0, /* DPMS: blank screens, wake on input */
	IDLE_ACTION_SUSPEND, /* systemctl suspend */
	IDLE_ACTION_HIBERNATE, /* systemctl hibernate */
};

typedef struct {
	int32_t id;
	char *layout_name;
	char *monitor_name;
	char *monitor_make;
	char *monitor_model;
	char *monitor_serial;
	float mfact;
	int32_t nmaster;
	int32_t no_render_border;
	int32_t open_as_floating;
	int32_t no_hide;
} ConfigTagRule;

typedef struct {
	char *layer_name;
	char *animation_type_open;
	char *animation_type_close;
	int32_t noblur;
	int32_t noanim;
	int32_t noshadow;
} ConfigLayerRule;

typedef struct {
	int32_t animations;
	int32_t layer_animations;
	char animation_type_open[10];
	char animation_type_close[10];
	char layer_animation_type_open[10];
	char layer_animation_type_close[10];
	int32_t animation_fade_in;
	int32_t animation_fade_out;
	int32_t tag_animation_direction;
	float zoom_initial_ratio;
	float zoom_end_ratio;
	float fadein_begin_opacity;
	float fadeout_begin_opacity;
	uint32_t animation_duration_move;
	uint32_t animation_duration_open;
	uint32_t animation_duration_tag;
	uint32_t animation_duration_close;
	uint32_t animation_duration_focus;
	double animation_curve_move[4];
	double animation_curve_open[4];
	double animation_curve_tag[4];
	double animation_curve_close[4];
	double animation_curve_focus[4];
	double animation_curve_opafadein[4];
	double animation_curve_opafadeout[4];

	int32_t scroller_structs;
	float scroller_default_proportion;
	float scroller_default_proportion_single;
	int32_t scroller_ignore_proportion_single;
	int32_t scroller_focus_center;
	int32_t scroller_prefer_center;
	int32_t scroller_prefer_overspread;
	int32_t edge_scroller_pointer_focus;
	int32_t focus_cross_monitor;
	int32_t exchange_cross_monitor;
	int32_t scratchpad_cross_monitor;
	int32_t focus_cross_tag;
	int32_t view_current_to_back;
	int32_t no_border_when_single;
	int32_t no_radius_when_single;
	int32_t snap_distance;
	int32_t enable_floating_snap;
	int32_t drag_tile_to_tile;
	int32_t drag_tile_small;
	uint32_t swipe_min_threshold;
	float focused_opacity;
	float unfocused_opacity;
	float *scroller_proportion_preset;
	int32_t scroller_proportion_preset_count;

	char **circle_layout;
	int32_t circle_layout_count;

	uint32_t new_is_master;
	float default_mfact;
	uint32_t default_nmaster;
	int32_t center_master_overspread;
	int32_t center_when_single_stack;

	int32_t dwindle_vsplit;
	int32_t dwindle_hsplit;
	int32_t dwindle_preserve_split;
	int32_t dwindle_smart_split;
	int32_t dwindle_smart_resize;
	int32_t dwindle_drop_simple_split;
	int32_t dwindle_manual_split;
	float dwindle_split_ratio;

	uint32_t hotarea_size;
	uint32_t hotarea_corner;
	uint32_t enable_hotarea;
	uint32_t ov_tab_mode;
	int32_t overviewgappi;
	int32_t overviewgappo;
	int32_t overview_dim;
	double overview_dim_alpha;
	int32_t overview_borderpx;
	double animation_spring_overview_tension;
	double animation_spring_overview_friction;
	uint32_t cursor_hide_timeout;

	uint32_t axis_bind_apply_timeout;
	uint32_t focus_on_activate;
	int32_t idleinhibit_ignore_visible;
	int32_t sloppyfocus;
	int32_t warpcursor;
	int32_t drag_corner;
	int32_t drag_warp_cursor;

	int32_t repeat_rate;
	int32_t repeat_delay;
	uint32_t numlockon;

	int32_t disable_while_typing;
	int32_t left_handed;
	int32_t middle_button_emulation;
	uint32_t scroll_method;
	uint32_t scroll_button;
	uint32_t click_method;
	uint32_t send_events_mode;

	int32_t mouse_natural_scrolling;
	uint32_t mouse_accel_profile;
	double mouse_accel_speed;
	double axis_scroll_factor;

	int32_t trackpad_natural_scrolling;
	uint32_t trackpad_accel_profile;
	double trackpad_accel_speed;
	double trackpad_scroll_factor;
	int32_t disable_trackpad;
	int32_t tap_to_click;
	int32_t tap_and_drag;
	int32_t drag_lock;
	uint32_t button_map;

	int32_t blur;
	int32_t blur_layer;
	int32_t blur_optimized;
	int32_t border_radius;
	int32_t border_radius_location_default;
	struct blur_data blur_params;
	int32_t shadows;
	int32_t shadow_only_floating;
	int32_t layer_shadows;
	uint32_t shadows_size;
	float shadows_blur;
	int32_t shadows_position_x;
	int32_t shadows_position_y;
	float shadowscolor[4];

	int32_t smartgaps;
	uint32_t gappih;
	uint32_t gappiv;
	uint32_t gappoh;
	uint32_t gappov;
	uint32_t borderpx;
	float scratchpad_width_ratio;
	float scratchpad_height_ratio;
	float rootcolor[4];
	float bordercolor[4];
	float dropcolor[4];
	float splitcolor[4];
	float focuscolor[4];
	float maximizescreencolor[4];
	float urgentcolor[4];
	float scratchpadcolor[4];
	float globalcolor[4];
	float overlaycolor[4];

	int32_t log_level;
	uint32_t capslock;

	ConfigTagRule *tag_rules;
	int32_t tag_rules_count;

	ConfigLayerRule *layer_rules;
	int32_t layer_rules_count;

	ConfigWinRule *window_rules;
	int32_t window_rules_count;

	ConfigMonitorRule *monitor_rules;
	int32_t monitor_rules_count;

	KeyBinding *key_bindings;
	int32_t key_bindings_count;

	MouseBinding *mouse_bindings;
	int32_t mouse_bindings_count;

	AxisBinding *axis_bindings;
	int32_t axis_bindings_count;

	SwitchBinding *switch_bindings;
	int32_t switch_bindings_count;

	GestureBinding *gesture_bindings;
	int32_t gesture_bindings_count;

	IdleBinding *idle_bindings;
	int32_t idle_bindings_count;

	int32_t idle_timeout;
	int32_t idle_action;
	/* Shorter idle timeout used while on battery (seconds). 0 = reuse
	   idle_timeout on battery too. Lets the screen blank/suspend sooner
	   unplugged — the panel is the single biggest power draw. */
	int32_t idle_timeout_battery;
	/* Smooth pre-idle dimming: lead seconds before idle_timeout, spring the
	   backlight down to floor %, snap back elastically on any input. */
	int32_t pre_idle_dim;
	int32_t pre_idle_dim_lead;
	int32_t pre_idle_dim_floor;

	int32_t focus_qos;
	int32_t focus_qos_bg_nice;

	int32_t tag_suspend_hidden;

	/* Built-in clipboard history. RAM-only ring, flushed on every restart.
	   max_entries caps the count; max_bytes caps the size of each entry
	   (anything larger is dropped before storage). */
	int32_t clipboard_history;
	int32_t clipboard_history_max_entries;
	int32_t clipboard_history_max_bytes;

	/* Force the wl_output subpixel hint to horizontal RGB for LCD subpixel
	   anti-aliasing. Only correct on standard RGB-stripe panels. */
	int32_t subpixel_rgb;

	/* Log per-frame render time when it exceeds the refresh budget, plus
	   direct-scanout transitions. Diagnostic only, off by default. */
	int32_t debug_frametime;

	/* Aggressive on-battery power savings. battery_fps caps the animation
	   frame rate while unplugged (default 60; lower = fewer GPU wakeups).
	   battery_timer_slack_ms widens the kernel timer slack on battery so the
	   scheduler coalesces wakeups and the CPU reaches deeper C-states
	   (default 50 ms; 0 = leave the default ~50 us slack). */
	int32_t battery_fps;
	int32_t battery_timer_slack_ms;

	/* PM_QOS request via /dev/cpu_dma_latency: max DMA wakeup latency (us) the
	   kernel may impose. Low values pin CPUs in shallow C-states so IRQ
	   wakeups take ~1us instead of ~100us — steadier input/frame timing at a
	   battery cost. -1 disables. On battery, 1000us is added so deeper sleep
	   stays available. Requires CAP_SYS_ADMIN or world-writable device. */
	int32_t cpu_dma_latency_us;

	/* Late input latching: defer each render so it finishes just before vblank,
	   latching the freshest input (~1 frame less input->photon latency).
	   latency_margin_us is the safety budget reserved before vblank to absorb
	   render-time jitter. Off by default. */
	int32_t late_latch;
	int32_t latency_margin_us;

	/* Vertical-scroller top outer gap override. -1 = center the window in the
	   usable area (default); >=0 = anchor it that many px below the top
	   (e.g. 0 hugs a top bar, removing the extra top margin). */
	int32_t scroller_top_gap;

	int32_t animation_spring;
	double animation_spring_mass;
	double animation_spring_tension;
	double animation_spring_friction;
	/* Separate, usually faster spring for tag (workspace) slide transitions. */
	double animation_spring_tag_tension;
	double animation_spring_tag_friction;
	/* Momentum hand-off: on drag/resize release, inject the pointer velocity
	   into the window's spring so it keeps the gesture's momentum ("throw").
	   _scale multiplies the handed-off velocity (0 = off-feel, 1 = 1:1). */
	int32_t animation_momentum;
	double animation_momentum_scale;
	/* Kinetic motion blur: fade a window slightly while it moves fast, crisp
	   again once the spring settles. strength 0..1 = max opacity drop. */
	int32_t animation_motion_blur;
	double animation_motion_blur_strength;

	ConfigEnv **env;
	int32_t env_count;

	char **exec;
	int32_t exec_count;

	char **exec_once;
	int32_t exec_once_count;

	char *cursor_theme;
	uint32_t cursor_size;

	int32_t single_scratchpad;
	int32_t xwayland_persistence;
	int32_t syncobj_enable;
	float drag_tile_refresh_interval;
	float drag_floating_refresh_interval;
	int32_t allow_tearing;
	int32_t allow_shortcuts_inhibit;
	int32_t allow_lock_transparent;

	struct xkb_rule_names xkb_rules;
	char xkb_rules_rules[128];
	char xkb_rules_model[128];
	char xkb_rules_layout[128];
	char xkb_rules_variant[128];
	char xkb_rules_options[128];

	char keymode[28];

	struct xkb_context *ctx;
	struct xkb_keymap *keymap;
} Config;

typedef int32_t (*FuncType)(const Arg *);
Config config;

bool parse_config_file(Config *config, const char *file_path, bool must_exist);
bool apply_rule_to_state(Monitor *m, const ConfigMonitorRule *rule,
						 struct wlr_output_state *state, int vrr, int custom);
bool monitor_matches_rule(Monitor *m, const ConfigMonitorRule *rule);

/* Strip leading and trailing whitespace from str in place. */
void trim_whitespace(char *str) {
	if (str == NULL || *str == '\0')
		return;

	char *start = str;
	while (isspace((unsigned char)*start)) {
		start++;
	}

	char *end = str + strlen(str) - 1;
	while (end > start && isspace((unsigned char)*end)) {
		end--;
	}

	*(end + 1) = '\0';

	if (start != str) {
		memmove(str, start, end - start + 2);
	}
}

/* Parse a comma-separated list of doubles from input into output; returns parsed count or -1 on error. */
int32_t parse_double_array(const char *input, double *output,
						   int32_t max_count) {
	char *dup = strdup(input);
	char *token;
	int32_t count = 0;

	memset(output, 0, max_count * sizeof(double));

	token = strtok(dup, ",");
	while (token != NULL && count < max_count) {
		trim_whitespace(token);
		char *endptr;
		double val = strtod(token, &endptr);
		if (endptr == token || *endptr != '\0') {
			fprintf(
				stderr,
				"\033[1m\033[31m[ERROR]:\033[33m Invalid number in array: %s\n",
				token);
			free(dup);
			return -1;
		}
		output[count] = val;
		count++;
		token = strtok(NULL, ",");
	}

	free(dup);
	return count;
}

/* Trim leading and trailing non-printable characters from str and return the trimmed pointer. */
char *sanitize_string(char *str) {
	
	while (*str != '\0' && !isprint((unsigned char)*str))
		str++;
	
	char *end = str + strlen(str) - 1;
	while (end > str && !isprint((unsigned char)*end))
		end--;
	*(end + 1) = '\0';
	return str;
}

/* Decode the suffix flags (s/l/r/p) of a "bind*" config key and apply them to the KeyBinding. */
void parse_bind_flags(const char *str, KeyBinding *kb) {

	if (strncmp(str, "bind", 4) != 0) {
		return;
	}

	const char *suffix = str + 4;

	for (int32_t i = 0; suffix[i] != '\0'; i++) {
		switch (suffix[i]) {
		case 's':
			kb->keysymcode.type = KEY_TYPE_SYM;
			break;
		case 'l':
			kb->islockapply = true;
			break;
		case 'r':
			kb->isreleaseapply = true;
			break;
		case 'p':
			kb->ispassapply = true;
			break;
		default:
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Unknown bind flag: %c\n",
					suffix[i]);
			break;
		}
	}
}

typedef struct { const char *name; int32_t value; } enum_entry_t;

/* Case-insensitive lookup of str in an enum_entry_t map; returns matched value or fallback. */
static int32_t parse_enum_ci(const char *str, const enum_entry_t *map,
                             size_t count, int32_t fallback) {
	char lowerStr[16];
	int32_t i = 0;
	while (str[i] && i < (int32_t)sizeof(lowerStr) - 1) {
		lowerStr[i] = tolower((unsigned char)str[i]);
		i++;
	}
	lowerStr[i] = '\0';

	for (size_t k = 0; k < count; k++) {
		if (strcmp(lowerStr, map[k].name) == 0)
			return map[k].value;
	}
	return fallback;
}

/* Parse a circular iteration direction string ("next"/"prev") into the matching enum value. */
int32_t parse_circle_direction(const char *str) {
	static const enum_entry_t map[] = {
		{"next", NEXT},
	};
	return parse_enum_ci(str, map, sizeof(map)/sizeof(map[0]), PREV);
}

/* Parse a cardinal direction string (up/down/left/right) into the matching enum value. */
int32_t parse_direction(const char *str) {
	static const enum_entry_t map[] = {
		{"up", UP}, {"down", DOWN}, {"left", LEFT}, {"right", RIGHT},
	};
	return parse_enum_ci(str, map, sizeof(map)/sizeof(map[0]), UNDIR);
}

/* Parse a switch fold state string ("fold"/"unfold") into the matching enum value. */
int32_t parse_fold_state(const char *str) {
	static const enum_entry_t map[] = {
		{"fold", FOLD}, {"unfold", UNFOLD},
	};
	return parse_enum_ci(str, map, sizeof(map)/sizeof(map[0]), INVALIDFOLD);
}
/* Parse a hexadecimal color string into a 64-bit integer; returns -1 on parse failure. */
int64_t parse_color(const char *hex_str) {
	char *endptr;
	int64_t hex_num = strtol(hex_str, &endptr, 16);
	if (*endptr != '\0') {
		return -1;
	}
	return hex_num;
}

/* Join values[] with commas up to the first empty/"0" entry; returns a newly malloc'd string. */
static char *combine_args_until_empty(char *values[], int count) {
	
	int first_empty = count;
	for (int i = 0; i < count; i++) {
		
		if (values[i][0] == '\0' ||
			(strlen(values[i]) == 1 && values[i][0] == '0')) {
			first_empty = i;
			break;
		}
	}

	if (first_empty == 0) {
		return strdup("");
	}

	size_t total_len = 0;
	for (int i = 0; i < first_empty; i++) {
		total_len += strlen(values[i]);
	}
	
	total_len += (first_empty - 1);

	char *combined = malloc(total_len + 1);
	if (combined == NULL) {
		return strdup("");
	}

	combined[0] = '\0';
	for (int i = 0; i < first_empty; i++) {
		if (i > 0) {
			strcat(combined, ",");
		}
		strcat(combined, values[i]);
	}

	return combined;
}

/* Parse a "+"-separated modifier string (e.g. "super+shift") into a WLR modifier mask. */
uint32_t parse_mod(const char *mod_str) {
	if (!mod_str || !*mod_str) {
		return UINT32_MAX;
	}

	uint32_t mod = 0;
	char input_copy[256];
	char *token;
	char *saveptr = NULL;
	bool match_success = false;

	strncpy(input_copy, mod_str, sizeof(input_copy) - 1);
	input_copy[sizeof(input_copy) - 1] = '\0';
	for (char *p = input_copy; *p; p++) {
		*p = tolower(*p);
	}

	token = strtok_r(input_copy, "+", &saveptr);
	while (token != NULL) {
		
		trim_whitespace(token);

		if (*token == '\0') {
			token = strtok_r(NULL, "+", &saveptr);
			continue;
		}

		if (strncmp(token, "code:", 5) == 0) {
			
			char *endptr;
			long keycode = strtol(token + 5, &endptr, 10);
			if (endptr != token + 5 && (*endptr == '\0' || *endptr == ' ')) {
				switch (keycode) {
				case 133:
				case 134:
					mod |= WLR_MODIFIER_LOGO;
					break;
				case 37:
				case 105:
					mod |= WLR_MODIFIER_CTRL;
					break;
				case 50:
				case 62:
					mod |= WLR_MODIFIER_SHIFT;
					break;
				case 64:
				case 108:
					mod |= WLR_MODIFIER_ALT;
					break;
				default:
					fprintf(stderr,
							"unknown modifier keycode: \033[1m\033[31m%s\n",
							token);
					break;
				}
			}
		} else {
			if (!strcmp(token, "super") || !strcmp(token, "super_l") ||
				!strcmp(token, "super_r")) {
				mod |= WLR_MODIFIER_LOGO;
				match_success = true;
			}
			if (!strcmp(token, "ctrl") || !strcmp(token, "ctrl_l") ||
				!strcmp(token, "ctrl_r")) {
				mod |= WLR_MODIFIER_CTRL;
				match_success = true;
			}
			if (!strcmp(token, "shift") || !strcmp(token, "shift_l") ||
				!strcmp(token, "shift_r")) {
				mod |= WLR_MODIFIER_SHIFT;
				match_success = true;
			}
			if (!strcmp(token, "alt") || !strcmp(token, "alt_l") ||
				!strcmp(token, "alt_r")) {
				mod |= WLR_MODIFIER_ALT;
				match_success = true;
			}
			if (!strcmp(token, "hyper") || !strcmp(token, "hyper_l") ||
				!strcmp(token, "hyper_r")) {
				mod |= WLR_MODIFIER_MOD3;
				match_success = true;
			}
			if (!strcmp(token, "none")) {
				match_success = true;
			}
		}

		token = strtok_r(NULL, "+", &saveptr);
	}

	if (!match_success) {
		mod = UINT32_MAX;
		fprintf(stderr,
				"\033[1m\033[31m[ERROR]:\033[33m Unknown modifier: "
				"\033[1m\033[31m%s\n",
				mod_str);
	}

	return mod;
}

/* Scan the keymap for keycodes that produce sym and fill up to three of them into multi_kc. */
static int32_t find_keycodes_for_keysym(struct xkb_keymap *keymap,
										xkb_keysym_t sym,
										MultiKeycode *multi_kc) {
	xkb_keycode_t min_keycode = xkb_keymap_min_keycode(keymap);
	xkb_keycode_t max_keycode = xkb_keymap_max_keycode(keymap);

	multi_kc->keycode1 = 0;
	multi_kc->keycode2 = 0;
	multi_kc->keycode3 = 0;

	int32_t found_count = 0;

	for (xkb_keycode_t keycode = min_keycode;
		 keycode <= max_keycode && found_count < 3; keycode++) {
		
		const xkb_keysym_t *syms;
		int32_t num_syms =
			xkb_keymap_key_get_syms_by_level(keymap, keycode, 0, 0, &syms);

		for (int32_t i = 0; i < num_syms; i++) {
			if (syms[i] == sym) {
				switch (found_count) {
				case 0:
					multi_kc->keycode1 = keycode;
					break;
				case 1:
					multi_kc->keycode2 = keycode;
					break;
				case 2:
					multi_kc->keycode3 = keycode;
					break;
				}
				found_count++;
				break;
			}
		}
	}

	return found_count;
}

/* Unref and clear the XKB keymap and context held by the global config. */
void cleanup_config_keymap(void) {
	if (config.keymap != NULL) {
		xkb_keymap_unref(config.keymap);
		config.keymap = NULL;
	}
	if (config.ctx != NULL) {
		xkb_context_unref(config.ctx);
		config.ctx = NULL;
	}
}

/* Lazily create the XKB context and fallback keymap used while parsing key bindings. */
void create_config_keymap(void) {
	
	if (config.ctx == NULL) {
		config.ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	}

	if (config.keymap == NULL) {
		config.keymap = xkb_keymap_new_from_names(
			config.ctx, &xkb_fallback_rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	}
}

/* Parse a key string (keysym name or "code:N") into a KeySymCode, optionally forcing keysym mode. */
KeySymCode parse_key(const char *key_str, bool isbindsym) {
	KeySymCode kc = {0};

	if (config.keymap == NULL || config.ctx == NULL) {
		
		kc.type = KEY_TYPE_SYM;
		kc.keysym = XKB_KEY_NoSymbol;
		return kc;
	}

	if (strncmp(key_str, "code:", 5) == 0) {
		char *endptr;
		xkb_keycode_t keycode = (xkb_keycode_t)strtol(key_str + 5, &endptr, 10);
		kc.type = KEY_TYPE_CODE;
		kc.keycode.keycode1 = keycode;
		kc.keycode.keycode2 = 0;
		kc.keycode.keycode3 = 0;
		return kc;
	}

	xkb_keysym_t sym =
		xkb_keysym_from_name(key_str, XKB_KEYSYM_CASE_INSENSITIVE);

	if (isbindsym) {
		kc.type = KEY_TYPE_SYM;
		kc.keysym = sym;
		return kc;
	}

	if (sym != XKB_KEY_NoSymbol) {
		
		int32_t found_count =
			find_keycodes_for_keysym(config.keymap, sym, &kc.keycode);
		if (found_count > 0) {
			kc.type = KEY_TYPE_CODE;
			kc.keysym = sym;
		} else {
			kc.type = KEY_TYPE_SYM;
			kc.keysym = sym;
			
		}
	} else {
		
		kc.type = KEY_TYPE_SYM;
		kc.keysym = XKB_KEY_NoSymbol;
		fprintf(
			stderr,
			"\033[1m\033[31m[ERROR]:\033[33m Unknown key: \033[1m\033[31m%s\n",
			key_str);
		
	}

	return kc;
}

/* Parse a mouse button name (e.g. "btn_left") into its BTN_* code; returns UINT32_MAX on error. */
uint32_t parse_button(const char *str) {
	
	char lowerStr[20];
	int32_t i = 0;
	while (str[i] && i < 19) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0';

	if (strcmp(lowerStr, "btn_left") == 0) {
		return BTN_LEFT;
	} else if (strcmp(lowerStr, "btn_right") == 0) {
		return BTN_RIGHT;
	} else if (strcmp(lowerStr, "btn_middle") == 0) {
		return BTN_MIDDLE;
	} else if (strcmp(lowerStr, "btn_side") == 0) {
		return BTN_SIDE;
	} else if (strcmp(lowerStr, "btn_extra") == 0) {
		return BTN_EXTRA;
	} else if (strcmp(lowerStr, "btn_forward") == 0) {
		return BTN_FORWARD;
	} else if (strcmp(lowerStr, "btn_back") == 0) {
		return BTN_BACK;
	} else if (strcmp(lowerStr, "btn_task") == 0) {
		return BTN_TASK;
	} else {
		fprintf(stderr,
				"\033[1m\033[31m[ERROR]:\033[33m Unknown button: "
				"\033[1m\033[31m%s\n",
				str);
		return UINT32_MAX;
	}
}

/* Parse a mouse action keyword (curmove/curresize/curnormal/curpressed) into its enum value. */
int32_t parse_mouse_action(const char *str) {
	
	char lowerStr[20];
	int32_t i = 0;
	while (str[i] && i < 19) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0';

	if (strcmp(lowerStr, "curmove") == 0) {
		return CurMove;
	} else if (strcmp(lowerStr, "curresize") == 0) {
		return CurResize;
	} else if (strcmp(lowerStr, "curnormal") == 0) {
		return CurNormal;
	} else if (strcmp(lowerStr, "curpressed") == 0) {
		return CurPressed;
	} else {
		return 0;
	}
}

/* Split a 32-bit 0xRRGGBBAA value into four normalized float channels written to color[4]. */
void convert_hex_to_rgba(float *color, uint32_t hex) {
	color[0] = ((hex >> 24) & 0xFF) / 255.0f;
	color[1] = ((hex >> 16) & 0xFF) / 255.0f;
	color[2] = ((hex >> 8) & 0xFF) / 255.0f;
	color[3] = (hex & 0xFF) / 255.0f;
}

/* Detect a numeric prefix sign ('-' or '+') and return the matching NUM_TYPE_* enum. */
uint32_t parse_num_type(char *str) {
	switch (str[0]) {
	case '-':
		return NUM_TYPE_MINUS;
	case '+':
		return NUM_TYPE_PLUS;
	default:
		return NUM_TYPE_DEFAULT;
	}
}

/* Resolve a dispatcher name to its function pointer and pack its string arguments into arg. */
FuncType parse_func_name(char *func_name, Arg *arg, char *arg_value,
						 char *arg_value2, char *arg_value3, char *arg_value4,
						 char *arg_value5) {

	FuncType func = NULL;
	(*arg).i = 0;
	(*arg).i2 = 0;
	(*arg).f = 0.0f;
	(*arg).f2 = 0.0f;
	(*arg).ui = 0;
	(*arg).ui2 = 0;
	(*arg).v = NULL;
	(*arg).v2 = NULL;
	(*arg).v3 = NULL;

	if (strcmp(func_name, "focusstack") == 0) {
		func = focusstack;
		(*arg).i = parse_circle_direction(arg_value);
	} else if (strcmp(func_name, "focusdir") == 0) {
		func = focusdir;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "incnmaster") == 0) {
		func = incnmaster;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "setmfact") == 0) {
		func = setmfact;
		(*arg).f = atof(arg_value);
	} else if (strcmp(func_name, "zoom") == 0) {
		func = zoom;
	} else if (strcmp(func_name, "exchange_client") == 0) {
		func = exchange_client;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "exchange_stack_client") == 0) {
		func = exchange_stack_client;
		(*arg).i = parse_circle_direction(arg_value);
	} else if (strcmp(func_name, "toggleglobal") == 0) {
		func = toggleglobal;
	} else if (strcmp(func_name, "toggleoverview") == 0) {
		func = toggleoverview;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "toggle_clipboard_history") == 0) {
		func = toggle_clipboard_history;
	} else if (strcmp(func_name, "clipboard_history_select_next") == 0) {
		func = clipboard_history_select_next;
	} else if (strcmp(func_name, "clipboard_history_select_prev") == 0) {
		func = clipboard_history_select_prev;
	} else if (strcmp(func_name, "clipboard_history_pick") == 0) {
		func = clipboard_history_pick;
	} else if (strcmp(func_name, "clipboard_history_cancel") == 0) {
		func = clipboard_history_cancel;
	} else if (strcmp(func_name, "set_proportion") == 0) {
		func = set_proportion;
		(*arg).f = atof(arg_value);
	} else if (strcmp(func_name, "switch_proportion_preset") == 0) {
		func = switch_proportion_preset;
		(*arg).i = parse_circle_direction(arg_value);
	} else if (strcmp(func_name, "viewtoleft") == 0) {
		func = viewtoleft;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "viewtoright") == 0) {
		func = viewtoright;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "tagsilent") == 0) {
		func = tagsilent;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
	} else if (strcmp(func_name, "tagtoleft") == 0) {
		func = tagtoleft;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "tagtoright") == 0) {
		func = tagtoright;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "killclient") == 0) {
		func = killclient;
	} else if (strcmp(func_name, "centerwin") == 0) {
		func = centerwin;
	} else if (strcmp(func_name, "focuslast") == 0) {
		func = focuslast;
	} else if (strcmp(func_name, "toggle_trackpad_enable") == 0) {
		func = toggle_trackpad_enable;
	} else if (strcmp(func_name, "setoption") == 0) {
		func = setoption;

		(*arg).v = strdup(arg_value);

		const char *non_empty_params[4] = {NULL};
		int32_t param_index = 0;

		if (arg_value2 && arg_value2[0] != '\0')
			non_empty_params[param_index++] = arg_value2;
		if (arg_value3 && arg_value3[0] != '\0')
			non_empty_params[param_index++] = arg_value3;
		if (arg_value4 && arg_value4[0] != '\0')
			non_empty_params[param_index++] = arg_value4;
		if (arg_value5 && arg_value5[0] != '\0')
			non_empty_params[param_index++] = arg_value5;

		if (param_index == 0) {
			(*arg).v2 = strdup("");
		} else {
			
			size_t len = 0;
			for (int32_t i = 0; i < param_index; i++) {
				len += strlen(non_empty_params[i]);
			}
			len += (param_index - 1) + 1;

			char *temp = malloc(len);
			if (temp) {
				char *cursor = temp;
				for (int32_t i = 0; i < param_index; i++) {
					if (i > 0) {
						*cursor++ = ',';
					}
					size_t param_len = strlen(non_empty_params[i]);
					memcpy(cursor, non_empty_params[i], param_len);
					cursor += param_len;
				}
				*cursor = '\0';
				(*arg).v2 = temp;
			}
		}
	} else if (strcmp(func_name, "setkeymode") == 0) {
		func = setkeymode;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "switch_keyboard_layout") == 0) {
		func = switch_keyboard_layout;
		(*arg).i = CLAMP_INT(atoi(arg_value), 0, 100);
	} else if (strcmp(func_name, "setlayout") == 0) {
		func = setlayout;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "switch_layout") == 0) {
		func = switch_layout;
	} else if (strcmp(func_name, "swipe_layout_dir") == 0) {
		func = swipe_layout_dir;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "window_cycler_next") == 0) {
		func = window_cycler_next;
	} else if (strcmp(func_name, "window_cycler_prev") == 0) {
		func = window_cycler_prev;
	} else if (strcmp(func_name, "togglefloating") == 0) {
		func = togglefloating;
	} else if (strcmp(func_name, "togglefullscreen") == 0) {
		func = togglefullscreen;
	} else if (strcmp(func_name, "togglefakefullscreen") == 0) {
		func = togglefakefullscreen;
	} else if (strcmp(func_name, "toggleoverlay") == 0) {
		func = toggleoverlay;
	} else if (strcmp(func_name, "minimized") == 0) {
		func = minimized;
	} else if (strcmp(func_name, "restore_minimized") == 0) {
		func = restore_minimized;
	} else if (strcmp(func_name, "toggle_scratchpad") == 0) {
		func = toggle_scratchpad;
	} else if (strcmp(func_name, "toggle_render_border") == 0) {
		func = toggle_render_border;
	} else if (strcmp(func_name, "focusmon") == 0) {
		func = focusmon;
		(*arg).i = parse_direction(arg_value);
		if ((*arg).i == UNDIR) {
			(*arg).v = strdup(arg_value);
		}
	} else if (strcmp(func_name, "tagmon") == 0) {
		func = tagmon;
		(*arg).i = parse_direction(arg_value);
		(*arg).i2 = atoi(arg_value2);
		if ((*arg).i == UNDIR) {
			(*arg).v = strdup(arg_value);
		};
	} else if (strcmp(func_name, "incgaps") == 0) {
		func = incgaps;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "togglegaps") == 0) {
		func = togglegaps;
	} else if (strcmp(func_name, "chvt") == 0) {
		func = chvt;
		(*arg).ui = atoi(arg_value);
	} else if (strcmp(func_name, "spawn") == 0) {
		func = spawn;
		char *values[] = {arg_value, arg_value2, arg_value3, arg_value4,
						  arg_value5};
		(*arg).v = combine_args_until_empty(values, 5);
	} else if (strcmp(func_name, "spawn_shell") == 0) {
		func = spawn_shell;
		char *values[] = {arg_value, arg_value2, arg_value3, arg_value4,
						  arg_value5};
		(*arg).v = combine_args_until_empty(values, 5);
	} else if (strcmp(func_name, "spawn_on_empty") == 0) {
		func = spawn_on_empty;
		(*arg).v = strdup(arg_value);
		(*arg).ui = 1 << (atoi(arg_value2) - 1);
	} else if (strcmp(func_name, "quit") == 0) {
		func = quit;
	} else if (strcmp(func_name, "create_virtual_output") == 0) {
		func = create_virtual_output;
	} else if (strcmp(func_name, "destroy_all_virtual_output") == 0) {
		func = destroy_all_virtual_output;
	} else if (strcmp(func_name, "moveresize") == 0) {
		func = moveresize;
		(*arg).ui = parse_mouse_action(arg_value);
	} else if (strcmp(func_name, "togglemaximizescreen") == 0) {
		func = togglemaximizescreen;
	} else if (strcmp(func_name, "viewtoleft_have_client") == 0) {
		func = viewtoleft_have_client;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "viewtoright_have_client") == 0) {
		func = viewtoright_have_client;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "reload_config") == 0) {
		func = reload_config;
	} else if (strcmp(func_name, "tag") == 0) {
		func = tag;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
		(*arg).i = atoi(arg_value2);
	} else if (strcmp(func_name, "view") == 0) {
		func = bind_to_view;

		uint32_t mask = 0;
		char *token;
		char *arg_copy = strdup(arg_value);

		if (arg_copy != NULL) {
			char *saveptr = NULL;
			token = strtok_r(arg_copy, "|", &saveptr);

			while (token != NULL) {
				int32_t num = atoi(token);
				if (num > 0 && num <= LENGTH(tags)) {
					mask |= (1 << (num - 1));
				}
				token = strtok_r(NULL, "|", &saveptr);
			}

			free(arg_copy);
		}

		if (mask) {
			(*arg).ui = mask;
		} else {
			(*arg).ui = atoi(arg_value);
		}
		(*arg).i = atoi(arg_value2);
	} else if (strcmp(func_name, "viewcrossmon") == 0) {
		func = viewcrossmon;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
		(*arg).v = strdup(arg_value2);
	} else if (strcmp(func_name, "tagcrossmon") == 0) {
		func = tagcrossmon;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
		(*arg).v = strdup(arg_value2);
	} else if (strcmp(func_name, "toggletag") == 0) {
		func = toggletag;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
	} else if (strcmp(func_name, "toggleview") == 0) {
		func = toggleview;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
	} else if (strcmp(func_name, "comboview") == 0) {
		func = comboview;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
	} else if (strcmp(func_name, "smartmovewin") == 0) {
		func = smartmovewin;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "smartresizewin") == 0) {
		func = smartresizewin;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "resizewin") == 0) {
		func = resizewin;
		(*arg).ui = parse_num_type(arg_value);
		(*arg).ui2 = parse_num_type(arg_value2);
		(*arg).i = (*arg).ui == NUM_TYPE_DEFAULT ? atoi(arg_value)
												 : atoi(arg_value + 1);
		(*arg).i2 = (*arg).ui2 == NUM_TYPE_DEFAULT ? atoi(arg_value2)
												   : atoi(arg_value2 + 1);
	} else if (strcmp(func_name, "movewin") == 0) {
		func = movewin;
		(*arg).ui = parse_num_type(arg_value);
		(*arg).ui2 = parse_num_type(arg_value2);
		(*arg).i = (*arg).ui == NUM_TYPE_DEFAULT ? atoi(arg_value)
												 : atoi(arg_value + 1);
		(*arg).i2 = (*arg).ui2 == NUM_TYPE_DEFAULT ? atoi(arg_value2)
												   : atoi(arg_value2 + 1);
	} else if (strcmp(func_name, "toggle_named_scratchpad") == 0) {
		func = toggle_named_scratchpad;
		(*arg).v = strdup(arg_value);
		(*arg).v2 = strdup(arg_value2);
		(*arg).v3 = strdup(arg_value3);
	} else if (strcmp(func_name, "disable_monitor") == 0) {
		func = disable_monitor;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "enable_monitor") == 0) {
		func = enable_monitor;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "toggle_monitor") == 0) {
		func = toggle_monitor;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "scroller_stack") == 0) {
		func = scroller_stack;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "toggle_all_floating") == 0) {
		func = toggle_all_floating;
	} else if (strcmp(func_name, "dwindle_toggle_split_direction") == 0) {
		func = dwindle_toggle_split_direction;
	} else if (strcmp(func_name, "dwindle_split_horizontal") == 0) {
		func = dwindle_split_horizontal;
	} else if (strcmp(func_name, "dwindle_split_vertical") == 0) {
		func = dwindle_split_vertical;
	} else {
		return NULL;
	}
	return func;
}

/* Apply all configured env entries via setenv so spawned children inherit them. */
void set_env() {
	for (int32_t i = 0; i < config.env_count; i++) {
		setenv(config.env[i]->type, config.env[i]->value, 1);
	}
}

/* Spawn every command listed under config.exec (run on every config reload). */
void run_exec() {
	Arg arg;

	for (int32_t i = 0; i < config.exec_count; i++) {
		arg.v = config.exec[i];
		spawn_shell(&arg);
	}
}

/* Spawn every command listed under config.exec_once (run only on initial startup). */
void run_exec_once() {
	Arg arg;

	for (int32_t i = 0; i < config.exec_once_count; i++) {
		arg.v = config.exec_once[i];
		spawn_shell(&arg);
	}
}

/* Dispatch a single key/value pair to the proper setting, rule, or binding parser. */
bool parse_option(Config *config, char *key, char *value) {
	if (strcmp(key, "keymode") == 0) {
		snprintf(config->keymode, sizeof(config->keymode), "%.27s", value);
	} else if (strcmp(key, "animations") == 0) {
		config->animations = atoi(value);
	} else if (strcmp(key, "layer_animations") == 0) {
		config->layer_animations = atoi(value);
	} else if (strcmp(key, "animation_type_open") == 0) {
		snprintf(config->animation_type_open,
				 sizeof(config->animation_type_open), "%.9s",
				 value);
	} else if (strcmp(key, "animation_type_close") == 0) {
		snprintf(config->animation_type_close,
				 sizeof(config->animation_type_close), "%.9s",
				 value);
	} else if (strcmp(key, "layer_animation_type_open") == 0) {
		snprintf(config->layer_animation_type_open,
				 sizeof(config->layer_animation_type_open), "%.9s",
				 value);
	} else if (strcmp(key, "layer_animation_type_close") == 0) {
		snprintf(config->layer_animation_type_close,
				 sizeof(config->layer_animation_type_close), "%.9s",
				 value);
	} else if (strcmp(key, "animation_fade_in") == 0) {
		config->animation_fade_in = atoi(value);
	} else if (strcmp(key, "animation_fade_out") == 0) {
		config->animation_fade_out = atoi(value);
	} else if (strcmp(key, "tag_animation_direction") == 0) {
		config->tag_animation_direction = atoi(value);
	} else if (strcmp(key, "zoom_initial_ratio") == 0) {
		config->zoom_initial_ratio = atof(value);
	} else if (strcmp(key, "zoom_end_ratio") == 0) {
		config->zoom_end_ratio = atof(value);
	} else if (strcmp(key, "fadein_begin_opacity") == 0) {
		config->fadein_begin_opacity = atof(value);
	} else if (strcmp(key, "fadeout_begin_opacity") == 0) {
		config->fadeout_begin_opacity = atof(value);
	} else if (strcmp(key, "animation_duration_move") == 0) {
		config->animation_duration_move = atoi(value);
	} else if (strcmp(key, "animation_duration_open") == 0) {
		config->animation_duration_open = atoi(value);
	} else if (strcmp(key, "animation_duration_tag") == 0) {
		config->animation_duration_tag = atoi(value);
	} else if (strcmp(key, "animation_duration_close") == 0) {
		config->animation_duration_close = atoi(value);
	} else if (strcmp(key, "animation_duration_focus") == 0) {
		config->animation_duration_focus = atoi(value);
	} else if (strcmp(key, "animation_curve_move") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_move, 4);
		if (num != 4) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to parse "
					"animation_curve_move: %s\n",
					value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_open") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_open, 4);
		if (num != 4) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to parse "
					"animation_curve_open: %s\n",
					value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_tag") == 0) {
		int32_t num = parse_double_array(value, config->animation_curve_tag, 4);
		if (num != 4) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to parse "
					"animation_curve_tag: %s\n",
					value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_close") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_close, 4);
		if (num != 4) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to parse "
					"animation_curve_close: %s\n",
					value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_focus") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_focus, 4);
		if (num != 4) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to parse "
					"animation_curve_focus: %s\n",
					value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_opafadein") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_opafadein, 4);
		if (num != 4) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to parse "
					"animation_curve_opafadein: %s\n",
					value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_opafadeout") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_opafadeout, 4);
		if (num != 4) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to parse "
					"animation_curve_opafadeout: %s\n",
					value);
			return false;
		}
	} else if (strcmp(key, "scroller_structs") == 0) {
		config->scroller_structs = atoi(value);
	} else if (strcmp(key, "scroller_default_proportion") == 0) {
		config->scroller_default_proportion = atof(value);
	} else if (strcmp(key, "scroller_default_proportion_single") == 0) {
		config->scroller_default_proportion_single = atof(value);
	} else if (strcmp(key, "scroller_ignore_proportion_single") == 0) {
		config->scroller_ignore_proportion_single = atoi(value);
	} else if (strcmp(key, "scroller_focus_center") == 0) {
		config->scroller_focus_center = atoi(value);
	} else if (strcmp(key, "scroller_prefer_center") == 0) {
		config->scroller_prefer_center = atoi(value);
	} else if (strcmp(key, "scroller_prefer_overspread") == 0) {
		config->scroller_prefer_overspread = atoi(value);
	} else if (strcmp(key, "edge_scroller_pointer_focus") == 0) {
		config->edge_scroller_pointer_focus = atoi(value);
	} else if (strcmp(key, "focus_cross_monitor") == 0) {
		config->focus_cross_monitor = atoi(value);
	} else if (strcmp(key, "exchange_cross_monitor") == 0) {
		config->exchange_cross_monitor = atoi(value);
	} else if (strcmp(key, "scratchpad_cross_monitor") == 0) {
		config->scratchpad_cross_monitor = atoi(value);
	} else if (strcmp(key, "focus_cross_tag") == 0) {
		config->focus_cross_tag = atoi(value);
	} else if (strcmp(key, "view_current_to_back") == 0) {
		config->view_current_to_back = atoi(value);
	} else if (strcmp(key, "blur") == 0) {
		config->blur = atoi(value);
	} else if (strcmp(key, "blur_layer") == 0) {
		config->blur_layer = atoi(value);
	} else if (strcmp(key, "blur_optimized") == 0) {
		config->blur_optimized = atoi(value);
	} else if (strcmp(key, "border_radius") == 0) {
		config->border_radius = atoi(value);
	} else if (strcmp(key, "blur_params_num_passes") == 0) {
		config->blur_params.num_passes = atoi(value);
	} else if (strcmp(key, "blur_params_radius") == 0) {
		config->blur_params.radius = atoi(value);
	} else if (strcmp(key, "blur_params_noise") == 0) {
		config->blur_params.noise = atof(value);
	} else if (strcmp(key, "blur_params_brightness") == 0) {
		config->blur_params.brightness = atof(value);
	} else if (strcmp(key, "blur_params_contrast") == 0) {
		config->blur_params.contrast = atof(value);
	} else if (strcmp(key, "blur_params_saturation") == 0) {
		config->blur_params.saturation = atof(value);
	} else if (strcmp(key, "shadows") == 0) {
		config->shadows = atoi(value);
	} else if (strcmp(key, "shadow_only_floating") == 0) {
		config->shadow_only_floating = atoi(value);
	} else if (strcmp(key, "layer_shadows") == 0) {
		config->layer_shadows = atoi(value);
	} else if (strcmp(key, "shadows_size") == 0) {
		config->shadows_size = atoi(value);
	} else if (strcmp(key, "shadows_blur") == 0) {
		config->shadows_blur = atof(value);
	} else if (strcmp(key, "shadows_position_x") == 0) {
		config->shadows_position_x = atoi(value);
	} else if (strcmp(key, "shadows_position_y") == 0) {
		config->shadows_position_y = atoi(value);
	} else if (strcmp(key, "single_scratchpad") == 0) {
		config->single_scratchpad = atoi(value);
	} else if (strcmp(key, "xwayland_persistence") == 0) {
		config->xwayland_persistence = atoi(value);
	} else if (strcmp(key, "clipboard_history") == 0) {
		config->clipboard_history = atoi(value);
	} else if (strcmp(key, "clipboard_history_max_entries") == 0) {
		config->clipboard_history_max_entries = atoi(value);
	} else if (strcmp(key, "clipboard_history_max_bytes") == 0) {
		config->clipboard_history_max_bytes = atoi(value);
	} else if (strcmp(key, "syncobj_enable") == 0) {
		config->syncobj_enable = atoi(value);
	} else if (strcmp(key, "tag_suspend_hidden") == 0) {
		config->tag_suspend_hidden = atoi(value);
	} else if (strcmp(key, "subpixel_rgb") == 0) {
		config->subpixel_rgb = atoi(value);
	} else if (strcmp(key, "debug_frametime") == 0) {
		config->debug_frametime = atoi(value);
	} else if (strcmp(key, "scroller_top_gap") == 0) {
		config->scroller_top_gap = atoi(value);
	} else if (strcmp(key, "animation_spring") == 0) {
		config->animation_spring = atoi(value);
	} else if (strcmp(key, "animation_spring_mass") == 0) {
		config->animation_spring_mass = atof(value);
	} else if (strcmp(key, "animation_spring_tension") == 0) {
		config->animation_spring_tension = atof(value);
	} else if (strcmp(key, "animation_spring_friction") == 0) {
		config->animation_spring_friction = atof(value);
	} else if (strcmp(key, "animation_momentum") == 0) {
		config->animation_momentum = atoi(value);
	} else if (strcmp(key, "animation_momentum_scale") == 0) {
		config->animation_momentum_scale = atof(value);
	} else if (strcmp(key, "overview_dim") == 0) {
		config->overview_dim = atoi(value);
	} else if (strcmp(key, "overview_dim_alpha") == 0) {
		config->overview_dim_alpha = atof(value);
	} else if (strcmp(key, "overview_borderpx") == 0) {
		config->overview_borderpx = atoi(value);
	} else if (strcmp(key, "animation_spring_overview_tension") == 0) {
		config->animation_spring_overview_tension = atof(value);
	} else if (strcmp(key, "animation_spring_overview_friction") == 0) {
		config->animation_spring_overview_friction = atof(value);
	} else if (strcmp(key, "animation_spring_tag_tension") == 0) {
		config->animation_spring_tag_tension = atof(value);
	} else if (strcmp(key, "animation_spring_tag_friction") == 0) {
		config->animation_spring_tag_friction = atof(value);
	} else if (strcmp(key, "animation_motion_blur") == 0) {
		config->animation_motion_blur = atoi(value);
	} else if (strcmp(key, "animation_motion_blur_strength") == 0) {
		config->animation_motion_blur_strength = atof(value);
	} else if (strcmp(key, "focus_qos") == 0) {
		config->focus_qos = atoi(value);
	} else if (strcmp(key, "focus_qos_bg_nice") == 0) {
		config->focus_qos_bg_nice = atoi(value);
	} else if (strcmp(key, "pre_idle_dim") == 0) {
		config->pre_idle_dim = atoi(value);
	} else if (strcmp(key, "pre_idle_dim_lead") == 0) {
		config->pre_idle_dim_lead = atoi(value);
	} else if (strcmp(key, "pre_idle_dim_floor") == 0) {
		config->pre_idle_dim_floor = atoi(value);
	} else if (strcmp(key, "idle_timeout") == 0) {
		config->idle_timeout = atoi(value);
	} else if (strcmp(key, "idle_timeout_battery") == 0) {
		config->idle_timeout_battery = atoi(value);
	} else if (strcmp(key, "battery_fps") == 0) {
		config->battery_fps = atoi(value);
	} else if (strcmp(key, "battery_timer_slack_ms") == 0) {
		config->battery_timer_slack_ms = atoi(value);
	} else if (strcmp(key, "cpu_dma_latency_us") == 0) {
		config->cpu_dma_latency_us = atoi(value);
	} else if (strcmp(key, "late_latch") == 0) {
		config->late_latch = atoi(value);
	} else if (strcmp(key, "latency_margin_us") == 0) {
		config->latency_margin_us = atoi(value);
	} else if (strcmp(key, "idle_action") == 0) {
		if (strcmp(value, "suspend") == 0)
			config->idle_action = IDLE_ACTION_SUSPEND;
		else if (strcmp(value, "hibernate") == 0)
			config->idle_action = IDLE_ACTION_HIBERNATE;
		else
			config->idle_action = IDLE_ACTION_OFF;
	} else if (strcmp(key, "drag_tile_refresh_interval") == 0) {
		config->drag_tile_refresh_interval = atof(value);
	} else if (strcmp(key, "drag_floating_refresh_interval") == 0) {
		config->drag_floating_refresh_interval = atof(value);
	} else if (strcmp(key, "allow_tearing") == 0) {
		config->allow_tearing = atoi(value);
	} else if (strcmp(key, "allow_shortcuts_inhibit") == 0) {
		config->allow_shortcuts_inhibit = atoi(value);
	} else if (strcmp(key, "allow_lock_transparent") == 0) {
		config->allow_lock_transparent = atoi(value);
	} else if (strcmp(key, "no_border_when_single") == 0) {
		config->no_border_when_single = atoi(value);
	} else if (strcmp(key, "no_radius_when_single") == 0) {
		config->no_radius_when_single = atoi(value);
	} else if (strcmp(key, "snap_distance") == 0) {
		config->snap_distance = atoi(value);
	} else if (strcmp(key, "enable_floating_snap") == 0) {
		config->enable_floating_snap = atoi(value);
	} else if (strcmp(key, "drag_tile_to_tile") == 0) {
		config->drag_tile_to_tile = atoi(value);
	} else if (strcmp(key, "drag_tile_small") == 0) {
		config->drag_tile_small = atoi(value);
	} else if (strcmp(key, "swipe_min_threshold") == 0) {
		config->swipe_min_threshold = atoi(value);
	} else if (strcmp(key, "focused_opacity") == 0) {
		config->focused_opacity = atof(value);
	} else if (strcmp(key, "unfocused_opacity") == 0) {
		config->unfocused_opacity = atof(value);
	} else if (strcmp(key, "xkb_rules_rules") == 0) {
		strncpy(config->xkb_rules_rules, value,
				sizeof(config->xkb_rules_rules) - 1);
		config->xkb_rules_rules[sizeof(config->xkb_rules_rules) - 1] = '\0';
	} else if (strcmp(key, "xkb_rules_model") == 0) {
		strncpy(config->xkb_rules_model, value,
				sizeof(config->xkb_rules_model) - 1);
		config->xkb_rules_model[sizeof(config->xkb_rules_model) - 1] = '\0';
	} else if (strcmp(key, "xkb_rules_layout") == 0) {
		strncpy(config->xkb_rules_layout, value,
				sizeof(config->xkb_rules_layout) - 1);
		config->xkb_rules_layout[sizeof(config->xkb_rules_layout) - 1] = '\0';
	} else if (strcmp(key, "xkb_rules_variant") == 0) {
		strncpy(config->xkb_rules_variant, value,
				sizeof(config->xkb_rules_variant) - 1);
		config->xkb_rules_variant[sizeof(config->xkb_rules_variant) - 1] = '\0';
	} else if (strcmp(key, "xkb_rules_options") == 0) {
		strncpy(config->xkb_rules_options, value,
				sizeof(config->xkb_rules_options) - 1);
		config->xkb_rules_options[sizeof(config->xkb_rules_options) - 1] = '\0';
	} else if (strcmp(key, "scroller_proportion_preset") == 0) {
		
		int32_t count = 0;
		for (const char *p = value; *p; p++) {
			if (*p == ',')
				count++;
		}
		int32_t float_count = count + 1;

		config->scroller_proportion_preset =
			(float *)malloc(float_count * sizeof(float));
		if (!config->scroller_proportion_preset) {
			fprintf(stderr, "\033[1m\033[31m[ERROR]:\033[33m Memory "
							"allocation failed\n");
			return false;
		}

		char *value_copy =
			strdup(value);
		char *token = strtok(value_copy, ",");
		int32_t i = 0;
		float value_set;

		while (token != NULL && i < float_count) {
			if (sscanf(token, "%f", &value_set) != 1) {
				fprintf(stderr,
						"\033[1m\033[31m[ERROR]:\033[33m Invalid float "
						"value in "
						"scroller_proportion_preset: %s\n",
						token);
				free(value_copy);
				free(config->scroller_proportion_preset);
				config->scroller_proportion_preset = NULL;
				return false;
			}

			config->scroller_proportion_preset[i] =
				CLAMP_FLOAT(value_set, 0.1f, 1.0f);

			token = strtok(NULL, ",");
			i++;
		}

		if (i != float_count) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid "
					"scroller_proportion_preset format: %s\n",
					value);
			free(value_copy);
			free(config->scroller_proportion_preset);
			config->scroller_proportion_preset = NULL;
			config->scroller_proportion_preset_count = 0;
			return false;
		}
		config->scroller_proportion_preset_count = float_count;

		free(value_copy);
	} else if (strcmp(key, "circle_layout") == 0) {
		
		int32_t count = 0;
		for (const char *p = value; *p; p++) {
			if (*p == ',')
				count++;
		}
		int32_t string_count = count + 1;

		config->circle_layout = (char **)malloc(string_count * sizeof(char *));
		memset(config->circle_layout, 0, string_count * sizeof(char *));
		if (!config->circle_layout) {
			fprintf(stderr, "\033[1m\033[31m[ERROR]:\033[33m Memory "
							"allocation failed\n");
			return false;
		}

		char *value_copy =
			strdup(value);
		char *token = strtok(value_copy, ",");
		int32_t i = 0;
		char *cleaned_token;
		while (token != NULL && i < string_count) {
			
			cleaned_token = sanitize_string(token);
			config->circle_layout[i] = strdup(cleaned_token);
			if (!config->circle_layout[i]) {
				fprintf(stderr,
						"\033[1m\033[31m[ERROR]:\033[33m Memory allocation "
						"failed for "
						"string: %s\n",
						token);
				
				for (int32_t j = 0; j < i; j++) {
					free(config->circle_layout[j]);
				}
				free(config->circle_layout);
				free(value_copy);
				config->circle_layout = NULL;
				config->circle_layout_count = 0;
				return false;
			}
			token = strtok(NULL, ",");
			i++;
		}

		if (i != string_count) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid circle_layout "
					"format: %s\n",
					value);
			
			for (int32_t j = 0; j < i; j++) {
				free(config->circle_layout[j]);
			}
			free(config->circle_layout);
			free(value_copy);
			config->circle_layout = NULL;
			config->circle_layout_count = 0;
			return false;
		}
		config->circle_layout_count = string_count;

		free(value_copy);
	} else if (strcmp(key, "new_is_master") == 0) {
		config->new_is_master = atoi(value);
	} else if (strcmp(key, "default_mfact") == 0) {
		config->default_mfact = atof(value);
	} else if (strcmp(key, "default_nmaster") == 0) {
		config->default_nmaster = atoi(value);
	} else if (strcmp(key, "center_master_overspread") == 0) {
		config->center_master_overspread = atoi(value);
	} else if (strcmp(key, "center_when_single_stack") == 0) {
		config->center_when_single_stack = atoi(value);
	} else if (strcmp(key, "dwindle_vsplit") == 0) {
		config->dwindle_vsplit = atoi(value);
	} else if (strcmp(key, "dwindle_hsplit") == 0) {
		config->dwindle_hsplit = atoi(value);
	} else if (strcmp(key, "dwindle_preserve_split") == 0) {
		config->dwindle_preserve_split = atoi(value);
	} else if (strcmp(key, "dwindle_smart_split") == 0) {
		config->dwindle_smart_split = atoi(value);
	} else if (strcmp(key, "dwindle_smart_resize") == 0) {
		config->dwindle_smart_resize = atoi(value);
	} else if (strcmp(key, "dwindle_drop_simple_split") == 0) {
		config->dwindle_drop_simple_split = atoi(value);
	} else if (strcmp(key, "dwindle_manual_split") == 0) {
		config->dwindle_manual_split = atoi(value);
	} else if (strcmp(key, "dwindle_split_ratio") == 0) {
		config->dwindle_split_ratio = atof(value);
	} else if (strcmp(key, "hotarea_size") == 0) {
		config->hotarea_size = atoi(value);
	} else if (strcmp(key, "hotarea_corner") == 0) {
		config->hotarea_corner = atoi(value);
	} else if (strcmp(key, "enable_hotarea") == 0) {
		config->enable_hotarea = atoi(value);
	} else if (strcmp(key, "ov_tab_mode") == 0) {
		config->ov_tab_mode = atoi(value);
	} else if (strcmp(key, "overviewgappi") == 0) {
		config->overviewgappi = atoi(value);
	} else if (strcmp(key, "overviewgappo") == 0) {
		config->overviewgappo = atoi(value);
	} else if (strcmp(key, "cursor_hide_timeout") == 0) {
		config->cursor_hide_timeout = atoi(value);
	} else if (strcmp(key, "axis_bind_apply_timeout") == 0) {
		config->axis_bind_apply_timeout = atoi(value);
	} else if (strcmp(key, "focus_on_activate") == 0) {
		config->focus_on_activate = atoi(value);
	} else if (strcmp(key, "numlockon") == 0) {
		config->numlockon = atoi(value);
	} else if (strcmp(key, "idleinhibit_ignore_visible") == 0) {
		config->idleinhibit_ignore_visible = atoi(value);
	} else if (strcmp(key, "sloppyfocus") == 0) {
		config->sloppyfocus = atoi(value);
	} else if (strcmp(key, "warpcursor") == 0) {
		config->warpcursor = atoi(value);
	} else if (strcmp(key, "drag_corner") == 0) {
		config->drag_corner = atoi(value);
	} else if (strcmp(key, "drag_warp_cursor") == 0) {
		config->drag_warp_cursor = atoi(value);
	} else if (strcmp(key, "smartgaps") == 0) {
		config->smartgaps = atoi(value);
	} else if (strcmp(key, "repeat_rate") == 0) {
		config->repeat_rate = atoi(value);
	} else if (strcmp(key, "repeat_delay") == 0) {
		config->repeat_delay = atoi(value);
	} else if (strcmp(key, "disable_trackpad") == 0) {
		config->disable_trackpad = atoi(value);
	} else if (strcmp(key, "tap_to_click") == 0) {
		config->tap_to_click = atoi(value);
	} else if (strcmp(key, "tap_and_drag") == 0) {
		config->tap_and_drag = atoi(value);
	} else if (strcmp(key, "drag_lock") == 0) {
		config->drag_lock = atoi(value);
	} else if (strcmp(key, "mouse_natural_scrolling") == 0) {
		config->mouse_natural_scrolling = atoi(value);
	} else if (strcmp(key, "trackpad_natural_scrolling") == 0) {
		config->trackpad_natural_scrolling = atoi(value);
	} else if (strcmp(key, "cursor_size") == 0) {
		config->cursor_size = atoi(value);
	} else if (strcmp(key, "cursor_theme") == 0) {
		config->cursor_theme = strdup(value);
	} else if (strcmp(key, "disable_while_typing") == 0) {
		config->disable_while_typing = atoi(value);
	} else if (strcmp(key, "left_handed") == 0) {
		config->left_handed = atoi(value);
	} else if (strcmp(key, "middle_button_emulation") == 0) {
		config->middle_button_emulation = atoi(value);
	} else if (strcmp(key, "mouse_accel_profile") == 0) {
		config->mouse_accel_profile = atoi(value);
	} else if (strcmp(key, "mouse_accel_speed") == 0) {
		config->mouse_accel_speed = atof(value);
	} else if (strcmp(key, "trackpad_accel_profile") == 0) {
		config->trackpad_accel_profile = atoi(value);
	} else if (strcmp(key, "trackpad_accel_speed") == 0) {
		config->trackpad_accel_speed = atof(value);
	} else if (strcmp(key, "scroll_method") == 0) {
		config->scroll_method = atoi(value);
	} else if (strcmp(key, "scroll_button") == 0) {
		config->scroll_button = atoi(value);
	} else if (strcmp(key, "click_method") == 0) {
		config->click_method = atoi(value);
	} else if (strcmp(key, "send_events_mode") == 0) {
		config->send_events_mode = atoi(value);
	} else if (strcmp(key, "button_map") == 0) {
		config->button_map = atoi(value);
	} else if (strcmp(key, "axis_scroll_factor") == 0) {
		config->axis_scroll_factor = atof(value);
	} else if (strcmp(key, "trackpad_scroll_factor") == 0) {
		config->trackpad_scroll_factor = atof(value);
	} else if (strcmp(key, "gappih") == 0) {
		config->gappih = atoi(value);
	} else if (strcmp(key, "gappiv") == 0) {
		config->gappiv = atoi(value);
	} else if (strcmp(key, "gappoh") == 0) {
		config->gappoh = atoi(value);
	} else if (strcmp(key, "gappov") == 0) {
		config->gappov = atoi(value);
	} else if (strcmp(key, "scratchpad_width_ratio") == 0) {
		config->scratchpad_width_ratio = atof(value);
	} else if (strcmp(key, "scratchpad_height_ratio") == 0) {
		config->scratchpad_height_ratio = atof(value);
	} else if (strcmp(key, "borderpx") == 0) {
		config->borderpx = atoi(value);
	} else if (strcmp(key, "rootcolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid rootcolor "
					"format: "
					"%s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->rootcolor, color);
		}

	} else if (strcmp(key, "shadowscolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid shadowscolor "
					"format: %s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->shadowscolor, color);
		}
	} else if (strcmp(key, "bordercolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid bordercolor "
					"format: %s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->bordercolor, color);
		}
	} else if (strcmp(key, "dropcolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid dropcolor "
					"format: %s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->dropcolor, color);
		}
	} else if (strcmp(key, "splitcolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid splitcolor "
					"format: %s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->splitcolor, color);
		}
	} else if (strcmp(key, "focuscolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid focuscolor "
					"format: %s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->focuscolor, color);
		}
	} else if (strcmp(key, "maximizescreencolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid "
					"maximizescreencolor "
					"format: %s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->maximizescreencolor, color);
		}
	} else if (strcmp(key, "urgentcolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid urgentcolor "
					"format: %s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->urgentcolor, color);
		}
	} else if (strcmp(key, "scratchpadcolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid "
					"scratchpadcolor "
					"format: %s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->scratchpadcolor, color);
		}
	} else if (strcmp(key, "globalcolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid globalcolor "
					"format: %s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->globalcolor, color);
		}
	} else if (strcmp(key, "overlaycolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid overlaycolor "
					"format: %s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->overlaycolor, color);
		}
	} else if (strcmp(key, "monitorrule") == 0) {
		config->monitor_rules =
			realloc(config->monitor_rules, (config->monitor_rules_count + 1) *
											   sizeof(ConfigMonitorRule));
		if (!config->monitor_rules) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for monitor rules\n");
			return false;
		}

		ConfigMonitorRule *rule =
			&config->monitor_rules[config->monitor_rules_count];
		memset(rule, 0, sizeof(ConfigMonitorRule));

		rule->name = NULL;
		rule->make = NULL;
		rule->model = NULL;
		rule->serial = NULL;
		rule->rr = 0;
		rule->scale = 1.0f;
		rule->x = INT32_MAX;
		rule->y = INT32_MAX;
		rule->width = -1;
		rule->height = -1;
		rule->refresh = 0.0f;
		rule->vrr = 0;
		rule->custom = 0;

		bool parse_error = false;
		char *token = strtok(value, ",");
		while (token != NULL) {
			char *colon = strchr(token, ':');
			if (colon != NULL) {
				*colon = '\0';
				char *key = token;
				char *val = colon + 1;

				trim_whitespace(key);
				trim_whitespace(val);

				if (strcmp(key, "name") == 0) {
					rule->name = strdup(val);
				} else if (strcmp(key, "make") == 0) {
					rule->make = strdup(val);
				} else if (strcmp(key, "model") == 0) {
					rule->model = strdup(val);
				} else if (strcmp(key, "serial") == 0) {
					rule->serial = strdup(val);
				} else if (strcmp(key, "rr") == 0) {
					rule->rr = CLAMP_INT(atoi(val), 0, 7);
				} else if (strcmp(key, "scale") == 0) {
					rule->scale = CLAMP_FLOAT(atof(val), 0.001f, 1000.0f);
				} else if (strcmp(key, "x") == 0) {
					rule->x = atoi(val);
				} else if (strcmp(key, "y") == 0) {
					rule->y = atoi(val);
				} else if (strcmp(key, "width") == 0) {
					rule->width = CLAMP_INT(atoi(val), 1, INT32_MAX);
				} else if (strcmp(key, "height") == 0) {
					rule->height = CLAMP_INT(atoi(val), 1, INT32_MAX);
				} else if (strcmp(key, "refresh") == 0) {
					rule->refresh = CLAMP_FLOAT(atof(val), 0.001f, 1000.0f);
				} else if (strcmp(key, "vrr") == 0) {
					rule->vrr = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "custom") == 0) {
					rule->custom = CLAMP_INT(atoi(val), 0, 1);
				} else {
					fprintf(stderr,
							"\033[1m\033[31m[ERROR]:\033[33m Unknown "
							"monitor rule "
							"option:\033[1m\033[31m %s\n",
							key);
					parse_error = true;
				}
			}
			token = strtok(NULL, ",");
		}

		if (!rule->name && !rule->make && !rule->model && !rule->serial) {
			fprintf(stderr, "\033[1m\033[31m[ERROR]:\033[33m Monitor rule "
							"must have at least one of the following "
							"options: name, make, model, serial\n");
			return false;
		}

		config->monitor_rules_count++;
		return !parse_error;
	} else if (strcmp(key, "tagrule") == 0) {
		config->tag_rules =
			realloc(config->tag_rules,
					(config->tag_rules_count + 1) * sizeof(ConfigTagRule));
		if (!config->tag_rules) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for tag rules\n");
			return false;
		}

		ConfigTagRule *rule = &config->tag_rules[config->tag_rules_count];
		memset(rule, 0, sizeof(ConfigTagRule));

		rule->id = 0;
		rule->layout_name = NULL;
		rule->monitor_name = NULL;
		rule->monitor_make = NULL;
		rule->monitor_model = NULL;
		rule->monitor_serial = NULL;
		rule->nmaster = 0;
		rule->mfact = 0.0f;
		rule->no_render_border = 0;
		rule->open_as_floating = 0;
		rule->no_hide = 0;

		bool parse_error = false;
		char *token = strtok(value, ",");
		while (token != NULL) {
			char *colon = strchr(token, ':');
			if (colon != NULL) {
				*colon = '\0';
				char *key = token;
				char *val = colon + 1;

				trim_whitespace(key);
				trim_whitespace(val);

				if (strcmp(key, "id") == 0) {
					rule->id = CLAMP_INT(atoi(val), 0, LENGTH(tags));
				} else if (strcmp(key, "layout_name") == 0) {
					rule->layout_name = strdup(val);
				} else if (strcmp(key, "monitor_name") == 0) {
					rule->monitor_name = strdup(val);
				} else if (strcmp(key, "monitor_make") == 0) {
					rule->monitor_make = strdup(val);
				} else if (strcmp(key, "monitor_model") == 0) {
					rule->monitor_model = strdup(val);
				} else if (strcmp(key, "monitor_serial") == 0) {
					rule->monitor_serial = strdup(val);
				} else if (strcmp(key, "no_render_border") == 0) {
					rule->no_render_border = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "open_as_floating") == 0) {
					rule->open_as_floating = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "no_hide") == 0) {
					rule->no_hide = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "nmaster") == 0) {
					rule->nmaster = CLAMP_INT(atoi(val), 1, 99);
				} else if (strcmp(key, "mfact") == 0) {
					rule->mfact = CLAMP_FLOAT(atof(val), 0.1f, 0.9f);
				} else {
					fprintf(stderr,
							"\033[1m\033[31m[ERROR]:\033[33m Unknown "
							"tag rule "
							"option:\033[1m\033[31m %s\n",
							key);
					parse_error = true;
				}
			}
			token = strtok(NULL, ",");
		}

		config->tag_rules_count++;
		return !parse_error;
	} else if (strcmp(key, "layerrule") == 0) {
		config->layer_rules =
			realloc(config->layer_rules,
					(config->layer_rules_count + 1) * sizeof(ConfigLayerRule));
		if (!config->layer_rules) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for layer rules\n");
			return false;
		}

		ConfigLayerRule *rule = &config->layer_rules[config->layer_rules_count];
		memset(rule, 0, sizeof(ConfigLayerRule));

		rule->layer_name = NULL;
		rule->animation_type_open = NULL;
		rule->animation_type_close = NULL;
		rule->noblur = 0;
		rule->noanim = 0;
		rule->noshadow = 0;

		bool parse_error = false;
		char *token = strtok(value, ",");
		while (token != NULL) {
			char *colon = strchr(token, ':');
			if (colon != NULL) {
				*colon = '\0';
				char *key = token;
				char *val = colon + 1;

				trim_whitespace(key);
				trim_whitespace(val);

				if (strcmp(key, "layer_name") == 0) {
					rule->layer_name = strdup(val);
				} else if (strcmp(key, "animation_type_open") == 0) {
					rule->animation_type_open = strdup(val);
				} else if (strcmp(key, "animation_type_close") == 0) {
					rule->animation_type_close = strdup(val);
				} else if (strcmp(key, "noblur") == 0) {
					rule->noblur = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "noanim") == 0) {
					rule->noanim = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "noshadow") == 0) {
					rule->noshadow = CLAMP_INT(atoi(val), 0, 1);
				} else {
					fprintf(stderr,
							"\033[1m\033[31m[ERROR]:\033[33m Unknown "
							"layer rule "
							"option:\033[1m\033[31m %s\n",
							key);
					parse_error = true;
				}
			}
			token = strtok(NULL, ",");
		}

		if (rule->layer_name == NULL) {
			rule->layer_name = strdup("default");
		}

		config->layer_rules_count++;
		return !parse_error;
	} else if (strcmp(key, "windowrule") == 0) {
		config->window_rules =
			realloc(config->window_rules,
					(config->window_rules_count + 1) * sizeof(ConfigWinRule));
		if (!config->window_rules) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for window rules\n");
			return false;
		}

		ConfigWinRule *rule = &config->window_rules[config->window_rules_count];
		memset(rule, 0, sizeof(ConfigWinRule));

		rule->isfloating = -1;
		rule->isfullscreen = -1;
		rule->isfakefullscreen = -1;
		rule->isnoborder = -1;
		rule->isnoshadow = -1;
		rule->isnoradius = -1;
		rule->isnoanimation = -1;
		rule->isopensilent = -1;
		rule->istagsilent = -1;
		rule->isnamedscratchpad = -1;
		rule->isunglobal = -1;
		rule->isglobal = -1;
		rule->isoverlay = -1;
		rule->allow_shortcuts_inhibit = -1;
		rule->ignore_maximize = -1;
		rule->ignore_minimize = -1;
		rule->isnosizehint = -1;
		rule->indleinhibit_when_focus = -1;
		rule->isterm = -1;
		rule->allow_csd = -1;
		rule->force_fakemaximize = -1;
		rule->force_tiled_state = -1;
		rule->force_tearing = -1;
		rule->noswallow = -1;
		rule->noblur = -1;
		rule->nofocus = -1;
		rule->nofadein = -1;
		rule->nofadeout = -1;
		rule->no_force_center = -1;

		rule->animation_type_open = NULL;
		rule->animation_type_close = NULL;

		rule->focused_opacity = 0;
		rule->unfocused_opacity = 0;
		rule->scroller_proportion_single = 0.0f;
		rule->scroller_proportion = 0;

		rule->tags = 0;
		rule->offsetx = 0;
		rule->offsety = 0;
		rule->width = 0;
		rule->height = 0;
		rule->monitor = NULL;
		rule->id = NULL;
		rule->title = NULL;

		rule->globalkeybinding = (KeyBinding){0};

		bool parse_error = false;
		char *token = strtok(value, ",");
		while (token != NULL) {
			char *colon = strchr(token, ':');
			if (colon != NULL) {
				*colon = '\0';
				char *key = token;
				char *val = colon + 1;

				trim_whitespace(key);
				trim_whitespace(val);

				if (strcmp(key, "isfloating") == 0) {
					rule->isfloating = atoi(val);
				} else if (strcmp(key, "title") == 0) {
					rule->title = strdup(val);
				} else if (strcmp(key, "appid") == 0) {
					rule->id = strdup(val);
				} else if (strcmp(key, "animation_type_open") == 0) {
					rule->animation_type_open = strdup(val);
				} else if (strcmp(key, "animation_type_close") == 0) {
					rule->animation_type_close = strdup(val);
				} else if (strcmp(key, "tags") == 0) {
					rule->tags = 1 << (atoi(val) - 1);
				} else if (strcmp(key, "monitor") == 0) {
					rule->monitor = strdup(val);
				} else if (strcmp(key, "offsetx") == 0) {
					rule->offsetx = atoi(val);
				} else if (strcmp(key, "offsety") == 0) {
					rule->offsety = atoi(val);
				} else if (strcmp(key, "nofocus") == 0) {
					rule->nofocus = atoi(val);
				} else if (strcmp(key, "nofadein") == 0) {
					rule->nofadein = atoi(val);
				} else if (strcmp(key, "nofadeout") == 0) {
					rule->nofadeout = atoi(val);
				} else if (strcmp(key, "no_force_center") == 0) {
					rule->no_force_center = atoi(val);
				} else if (strcmp(key, "width") == 0) {
					rule->width = atof(val);
				} else if (strcmp(key, "height") == 0) {
					rule->height = atof(val);
				} else if (strcmp(key, "isnoborder") == 0) {
					rule->isnoborder = atoi(val);
				} else if (strcmp(key, "isnoshadow") == 0) {
					rule->isnoshadow = atoi(val);
				} else if (strcmp(key, "isnoradius") == 0) {
					rule->isnoradius = atoi(val);
				} else if (strcmp(key, "isnoanimation") == 0) {
					rule->isnoanimation = atoi(val);
				} else if (strcmp(key, "isopensilent") == 0) {
					rule->isopensilent = atoi(val);
				} else if (strcmp(key, "istagsilent") == 0) {
					rule->istagsilent = atoi(val);
				} else if (strcmp(key, "isnamedscratchpad") == 0) {
					rule->isnamedscratchpad = atoi(val);
				} else if (strcmp(key, "isunglobal") == 0) {
					rule->isunglobal = atoi(val);
				} else if (strcmp(key, "isglobal") == 0) {
					rule->isglobal = atoi(val);
				} else if (strcmp(key, "scroller_proportion_single") == 0) {
					rule->scroller_proportion_single = atof(val);
				} else if (strcmp(key, "unfocused_opacity") == 0) {
					rule->unfocused_opacity = atof(val);
				} else if (strcmp(key, "focused_opacity") == 0) {
					rule->focused_opacity = atof(val);
				} else if (strcmp(key, "isoverlay") == 0) {
					rule->isoverlay = atoi(val);
				} else if (strcmp(key, "allow_shortcuts_inhibit") == 0) {
					rule->allow_shortcuts_inhibit = atoi(val);
				} else if (strcmp(key, "ignore_maximize") == 0) {
					rule->ignore_maximize = atoi(val);
				} else if (strcmp(key, "ignore_minimize") == 0) {
					rule->ignore_minimize = atoi(val);
				} else if (strcmp(key, "isnosizehint") == 0) {
					rule->isnosizehint = atoi(val);
				} else if (strcmp(key, "indleinhibit_when_focus") == 0) {
					rule->indleinhibit_when_focus = atoi(val);
				} else if (strcmp(key, "isterm") == 0) {
					rule->isterm = atoi(val);
				} else if (strcmp(key, "allow_csd") == 0) {
					rule->allow_csd = atoi(val);
				} else if (strcmp(key, "force_fakemaximize") == 0) {
					rule->force_fakemaximize = atoi(val);
				} else if (strcmp(key, "force_tiled_state") == 0) {
					rule->force_tiled_state = atoi(val);
				} else if (strcmp(key, "force_tearing") == 0) {
					rule->force_tearing = atoi(val);
				} else if (strcmp(key, "noswallow") == 0) {
					rule->noswallow = atoi(val);
				} else if (strcmp(key, "noblur") == 0) {
					rule->noblur = atoi(val);
				} else if (strcmp(key, "scroller_proportion") == 0) {
					rule->scroller_proportion = atof(val);
				} else if (strcmp(key, "isfullscreen") == 0) {
					rule->isfullscreen = atoi(val);
				} else if (strcmp(key, "isfakefullscreen") == 0) {
					rule->isfakefullscreen = atoi(val);
				} else if (strcmp(key, "globalkeybinding") == 0) {
					char mod_str[256], keysym_str[256];
					sscanf(val, "%255[^-]-%255[a-zA-Z]", mod_str, keysym_str);
					trim_whitespace(mod_str);
					trim_whitespace(keysym_str);
					rule->globalkeybinding.mod = parse_mod(mod_str);
					rule->globalkeybinding.keysymcode =
						parse_key(keysym_str, false);
					if (rule->globalkeybinding.mod == UINT32_MAX) {
						return false;
					}
					if (rule->globalkeybinding.keysymcode.type ==
							KEY_TYPE_SYM &&
						rule->globalkeybinding.keysymcode.keysym ==
							XKB_KEY_NoSymbol) {
						return false;
					}
				} else {
					fprintf(stderr,
							"\033[1m\033[31m[ERROR]:\033[33m Unknown "
							"window rule "
							"option:\033[1m\033[31m %s\n",
							key);
					parse_error = true;
				}
			}
			token = strtok(NULL, ",");
		}
		config->window_rules_count++;
		return !parse_error;
	} else if (strncmp(key, "env", 3) == 0) {

		char env_type[256], env_value[256];
		if (sscanf(value, "%255[^,],%255[^\n]", env_type, env_value) < 2) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid bind format: "
					"\033[1m\033[31m%s\n",
					value);
			return false;
		}
		trim_whitespace(env_type);
		trim_whitespace(env_value);

		ConfigEnv *env = calloc(1, sizeof(ConfigEnv));
		env->type = strdup(env_type);
		env->value = strdup(env_value);

		config->env =
			realloc(config->env, (config->env_count + 1) * sizeof(ConfigEnv));
		if (!config->env) {
			free(env->type);
			free(env->value);
			free(env);
			fprintf(stderr, "\033[1m\033[31m[ERROR]:\033[33m Failed to "
							"allocate memory for env\n");
			return false;
		}

		config->env[config->env_count] = env;
		config->env_count++;

	} else if (strncmp(key, "exec", 9) == 0) {
		char **new_exec =
			realloc(config->exec, (config->exec_count + 1) * sizeof(char *));
		if (!new_exec) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for exec\n");
			return false;
		}
		config->exec = new_exec;

		config->exec[config->exec_count] = strdup(value);
		if (!config->exec[config->exec_count]) {
			fprintf(stderr, "\033[1m\033[31m[ERROR]:\033[33m Failed to "
							"duplicate exec string\n");
			return false;
		}

		config->exec_count++;

	} else if (strncmp(key, "exec-once", 9) == 0) {

		char **new_exec_once = realloc(
			config->exec_once, (config->exec_once_count + 1) * sizeof(char *));
		if (!new_exec_once) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for exec_once\n");
			return false;
		}
		config->exec_once = new_exec_once;

		config->exec_once[config->exec_once_count] = strdup(value);
		if (!config->exec_once[config->exec_once_count]) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to duplicate "
					"exec_once string\n");
			return false;
		}

		config->exec_once_count++;

	} else if (regex_match("^bind[s|l|r|p]*$", key)) {
		config->key_bindings =
			realloc(config->key_bindings,
					(config->key_bindings_count + 1) * sizeof(KeyBinding));
		if (!config->key_bindings) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for key bindings\n");
			return false;
		}

		KeyBinding *binding = &config->key_bindings[config->key_bindings_count];
		memset(binding, 0, sizeof(KeyBinding));

		char mod_str[256], keysym_str[256], func_name[256],
			arg_value[256] = "0\0", arg_value2[256] = "0\0",
			arg_value3[256] = "0\0", arg_value4[256] = "0\0",
			arg_value5[256] = "0\0";
		if (sscanf(value,
				   "%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],%255[^"
				   ",],%255["
				   "^,],%255[^\n]",
				   mod_str, keysym_str, func_name, arg_value, arg_value2,
				   arg_value3, arg_value4, arg_value5) < 3) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid bind format: "
					"\033[1m\033[31m%s\n",
					value);
			return false;
		}
		trim_whitespace(mod_str);
		trim_whitespace(keysym_str);
		trim_whitespace(func_name);
		trim_whitespace(arg_value);
		trim_whitespace(arg_value2);
		trim_whitespace(arg_value3);
		trim_whitespace(arg_value4);
		trim_whitespace(arg_value5);

		strcpy(binding->mode, config->keymode);
		if (strcmp(binding->mode, "common") == 0) {
			binding->iscommonmode = true;
			binding->isdefaultmode = false;
		} else if (strcmp(binding->mode, "default") == 0) {
			binding->isdefaultmode = true;
			binding->iscommonmode = false;
		} else {
			binding->isdefaultmode = false;
			binding->iscommonmode = false;
		}

		parse_bind_flags(key, binding);
		binding->keysymcode =
			parse_key(keysym_str, binding->keysymcode.type == KEY_TYPE_SYM);
		binding->mod = parse_mod(mod_str);
		binding->arg.i = 0;
		binding->arg.i2 = 0;
		binding->arg.f = 0.0f;
		binding->arg.f2 = 0.0f;
		binding->arg.ui = 0;
		binding->arg.ui2 = 0;
		binding->arg.v = NULL;
		binding->arg.v2 = NULL;
		binding->arg.v3 = NULL;
		binding->func =
			parse_func_name(func_name, &binding->arg, arg_value, arg_value2,
							arg_value3, arg_value4, arg_value5);
		if (!binding->func || binding->mod == UINT32_MAX ||
			(binding->keysymcode.type == KEY_TYPE_SYM &&
			 binding->keysymcode.keysym == XKB_KEY_NoSymbol)) {
			if (binding->arg.v) {
				free(binding->arg.v);
				binding->arg.v = NULL;
			}
			if (binding->arg.v2) {
				free(binding->arg.v2);
				binding->arg.v2 = NULL;
			}
			if (binding->arg.v3) {
				free(binding->arg.v3);
				binding->arg.v3 = NULL;
			}
			if (!binding->func)
				fprintf(stderr,
						"\033[1m\033[31m[ERROR]:\033[33m Unknown "
						"dispatch in bind: "
						"\033[1m\033[31m%s\n",
						func_name);
			return false;
		} else {
			config->key_bindings_count++;
		}

	} else if (strncmp(key, "mousebind", 9) == 0) {
		config->mouse_bindings =
			realloc(config->mouse_bindings,
					(config->mouse_bindings_count + 1) * sizeof(MouseBinding));
		if (!config->mouse_bindings) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for mouse bindings\n");
			return false;
		}

		MouseBinding *binding =
			&config->mouse_bindings[config->mouse_bindings_count];
		memset(binding, 0, sizeof(MouseBinding));

		char mod_str[256], button_str[256], func_name[256],
			arg_value[256] = "0\0", arg_value2[256] = "0\0",
			arg_value3[256] = "0\0", arg_value4[256] = "0\0",
			arg_value5[256] = "0\0";
		if (sscanf(value,
				   "%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],%255[^"
				   ",],%255["
				   "^,],%255[^\n]",
				   mod_str, button_str, func_name, arg_value, arg_value2,
				   arg_value3, arg_value4, arg_value5) < 3) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid mousebind "
					"format: "
					"%s\n",
					value);
			return false;
		}
		trim_whitespace(mod_str);
		trim_whitespace(button_str);
		trim_whitespace(func_name);
		trim_whitespace(arg_value);
		trim_whitespace(arg_value2);
		trim_whitespace(arg_value3);
		trim_whitespace(arg_value4);
		trim_whitespace(arg_value5);

		binding->mod = parse_mod(mod_str);
		binding->button = parse_button(button_str);
		binding->arg.i = 0;
		binding->arg.i2 = 0;
		binding->arg.f = 0.0f;
		binding->arg.f2 = 0.0f;
		binding->arg.ui = 0;
		binding->arg.ui2 = 0;
		binding->arg.v = NULL;
		binding->arg.v2 = NULL;
		binding->arg.v3 = NULL;

		if (binding->mod == 0 &&
			(binding->button == BTN_LEFT || binding->button == BTN_RIGHT)) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m \033[31m%s\033[33m can't "
					"bind to \033[31m%s\033[33m mod key\n",
					button_str, mod_str);
			return false;
		}

		binding->func =
			parse_func_name(func_name, &binding->arg, arg_value, arg_value2,
							arg_value3, arg_value4, arg_value5);
		if (!binding->func || binding->mod == UINT32_MAX ||
			binding->button == UINT32_MAX) {
			if (binding->arg.v) {
				free(binding->arg.v);
				binding->arg.v = NULL;
			}
			if (binding->arg.v2) {
				free(binding->arg.v2);
				binding->arg.v2 = NULL;
			}
			if (binding->arg.v3) {
				free(binding->arg.v3);
				binding->arg.v3 = NULL;
			}

			if (!binding->func)
				fprintf(stderr,
						"\033[1m\033[31m[ERROR]:\033[33m Unknown "
						"dispatch in "
						"mousebind: \033[1m\033[31m%s\n",
						func_name);
			return false;
		} else {
			config->mouse_bindings_count++;
		}
	} else if (strncmp(key, "axisbind", 8) == 0) {
		config->axis_bindings =
			realloc(config->axis_bindings,
					(config->axis_bindings_count + 1) * sizeof(AxisBinding));
		if (!config->axis_bindings) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for axis bindings\n");
			return false;
		}

		AxisBinding *binding =
			&config->axis_bindings[config->axis_bindings_count];
		memset(binding, 0, sizeof(AxisBinding));

		char mod_str[256], dir_str[256], func_name[256],
			arg_value[256] = "0\0", arg_value2[256] = "0\0",
			arg_value3[256] = "0\0", arg_value4[256] = "0\0",
			arg_value5[256] = "0\0";
		if (sscanf(value,
				   "%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],%255[^"
				   ",],%255["
				   "^,],%255[^\n]",
				   mod_str, dir_str, func_name, arg_value, arg_value2,
				   arg_value3, arg_value4, arg_value5) < 3) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid axisbind "
					"format: %s\n",
					value);
			return false;
		}

		trim_whitespace(mod_str);
		trim_whitespace(dir_str);
		trim_whitespace(func_name);
		trim_whitespace(arg_value);
		trim_whitespace(arg_value2);
		trim_whitespace(arg_value3);
		trim_whitespace(arg_value4);
		trim_whitespace(arg_value5);

		binding->mod = parse_mod(mod_str);
		binding->dir = parse_direction(dir_str);
		binding->arg.v = NULL;
		binding->arg.v2 = NULL;
		binding->arg.v3 = NULL;
		binding->func =
			parse_func_name(func_name, &binding->arg, arg_value, arg_value2,
							arg_value3, arg_value4, arg_value5);

		if (!binding->func || binding->mod == UINT32_MAX) {
			if (binding->arg.v) {
				free(binding->arg.v);
				binding->arg.v = NULL;
			}
			if (binding->arg.v2) {
				free(binding->arg.v2);
				binding->arg.v2 = NULL;
			}
			if (binding->arg.v3) {
				free(binding->arg.v3);
				binding->arg.v3 = NULL;
			}
			if (!binding->func)
				fprintf(stderr,
						"\033[1m\033[31m[ERROR]:\033[33m Unknown "
						"dispatch in "
						"axisbind: \033[1m\033[31m%s\n",
						func_name);
			return false;
		} else {
			config->axis_bindings_count++;
		}

	} else if (strncmp(key, "idlebind", 8) == 0) {
		config->idle_bindings =
			realloc(config->idle_bindings,
					(config->idle_bindings_count + 1) * sizeof(IdleBinding));
		if (!config->idle_bindings) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for idle bindings\n");
			return false;
		}

		IdleBinding *binding =
			&config->idle_bindings[config->idle_bindings_count];
		memset(binding, 0, sizeof(IdleBinding));

		char timeout_str[256], func_name[256], arg_value[256] = "0\0",
			arg_value2[256] = "0\0", arg_value3[256] = "0\0",
			arg_value4[256] = "0\0", arg_value5[256] = "0\0";
		if (sscanf(value,
				   "%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],"
				   "%255[^\n]",
				   timeout_str, func_name, arg_value, arg_value2, arg_value3,
				   arg_value4, arg_value5) < 2) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid idlebind "
					"format: %s\n",
					value);
			return false;
		}

		trim_whitespace(timeout_str);
		trim_whitespace(func_name);
		trim_whitespace(arg_value);
		trim_whitespace(arg_value2);
		trim_whitespace(arg_value3);
		trim_whitespace(arg_value4);
		trim_whitespace(arg_value5);

		int32_t timeout_s = atoi(timeout_str);
		if (timeout_s <= 0) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m idlebind timeout must be "
					"a positive number of seconds: %s\n",
					timeout_str);
			return false;
		}
		binding->timeout_ms = (uint32_t)timeout_s * 1000;
		binding->arg.v = NULL;
		binding->arg.v2 = NULL;
		binding->arg.v3 = NULL;
		binding->func =
			parse_func_name(func_name, &binding->arg, arg_value, arg_value2,
							arg_value3, arg_value4, arg_value5);

		if (!binding->func) {
			if (binding->arg.v) {
				free(binding->arg.v);
				binding->arg.v = NULL;
			}
			if (binding->arg.v2) {
				free(binding->arg.v2);
				binding->arg.v2 = NULL;
			}
			if (binding->arg.v3) {
				free(binding->arg.v3);
				binding->arg.v3 = NULL;
			}
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Unknown dispatch in "
					"idlebind: \033[1m\033[31m%s\n",
					func_name);
			return false;
		} else {
			config->idle_bindings_count++;
		}

	} else if (strncmp(key, "switchbind", 10) == 0) {
		config->switch_bindings = realloc(config->switch_bindings,
										  (config->switch_bindings_count + 1) *
											  sizeof(SwitchBinding));
		if (!config->switch_bindings) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for switch bindings\n");
			return false;
		}

		SwitchBinding *binding =
			&config->switch_bindings[config->switch_bindings_count];
		memset(binding, 0, sizeof(SwitchBinding));

		char fold_str[256], func_name[256],
			arg_value[256] = "0\0", arg_value2[256] = "0\0",
			arg_value3[256] = "0\0", arg_value4[256] = "0\0",
			arg_value5[256] = "0\0";
		if (sscanf(value,
				   "%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],%255[^"
				   ",],%255["
				   "^\n]",
				   fold_str, func_name, arg_value, arg_value2, arg_value3,
				   arg_value4, arg_value5) < 3) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid switchbind "
					"format: %s\n",
					value);
			return false;
		}
		trim_whitespace(fold_str);
		trim_whitespace(func_name);
		trim_whitespace(arg_value);
		trim_whitespace(arg_value2);
		trim_whitespace(arg_value3);
		trim_whitespace(arg_value4);
		trim_whitespace(arg_value5);

		binding->fold = parse_fold_state(fold_str);
		binding->func =
			parse_func_name(func_name, &binding->arg, arg_value, arg_value2,
							arg_value3, arg_value4, arg_value5);

		if (!binding->func) {
			if (binding->arg.v) {
				free(binding->arg.v);
				binding->arg.v = NULL;
			}
			if (binding->arg.v2) {
				free(binding->arg.v2);
				binding->arg.v2 = NULL;
			}
			if (binding->arg.v3) {
				free(binding->arg.v3);
				binding->arg.v3 = NULL;
			}

			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Unknown dispatch in "
					"switchbind: "
					"\033[1m\033[31m%s\n",
					func_name);
			return false;
		} else {
			config->switch_bindings_count++;
		}

	} else if (strncmp(key, "gesturebind", 11) == 0) {
		config->gesture_bindings = realloc(
			config->gesture_bindings,
			(config->gesture_bindings_count + 1) * sizeof(GestureBinding));
		if (!config->gesture_bindings) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for axis gesturebind\n");
			return false;
		}

		GestureBinding *binding =
			&config->gesture_bindings[config->gesture_bindings_count];
		memset(binding, 0, sizeof(GestureBinding));

		char mod_str[256], motion_str[256], fingers_count_str[256],
			func_name[256], arg_value[256] = "0\0", arg_value2[256] = "0\0",
							arg_value3[256] = "0\0", arg_value4[256] = "0\0",
							arg_value5[256] = "0\0";
		if (sscanf(value,
				   "%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],%255[^"
				   ",],%255["
				   "^,],%255[^,],%255[^\n]",
				   mod_str, motion_str, fingers_count_str, func_name, arg_value,
				   arg_value2, arg_value3, arg_value4, arg_value5) < 4) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid gesturebind "
					"format: %s\n",
					value);
			return false;
		}

		trim_whitespace(mod_str);
		trim_whitespace(motion_str);
		trim_whitespace(fingers_count_str);
		trim_whitespace(func_name);
		trim_whitespace(arg_value);
		trim_whitespace(arg_value2);
		trim_whitespace(arg_value3);
		trim_whitespace(arg_value4);
		trim_whitespace(arg_value5);

		binding->mod = parse_mod(mod_str);
		binding->motion = parse_direction(motion_str);
		binding->fingers_count = atoi(fingers_count_str);
		binding->arg.i = 0;
		binding->arg.i2 = 0;
		binding->arg.f = 0.0f;
		binding->arg.f2 = 0.0f;
		binding->arg.ui = 0;
		binding->arg.ui2 = 0;
		binding->arg.v = NULL;
		binding->arg.v2 = NULL;
		binding->arg.v3 = NULL;
		binding->func =
			parse_func_name(func_name, &binding->arg, arg_value, arg_value2,
							arg_value3, arg_value4, arg_value5);

		if (!binding->func || binding->mod == UINT32_MAX) {
			if (binding->arg.v) {
				free(binding->arg.v);
				binding->arg.v = NULL;
			}
			if (binding->arg.v2) {
				free(binding->arg.v2);
				binding->arg.v2 = NULL;
			}
			if (binding->arg.v3) {
				free(binding->arg.v3);
				binding->arg.v3 = NULL;
			}
			if (!binding->func)
				fprintf(stderr,
						"\033[1m\033[31m[ERROR]:\033[33m Unknown "
						"dispatch in "
						"axisbind: \033[1m\033[31m%s\n",
						func_name);
			return false;
		} else {
			config->gesture_bindings_count++;
		}

	} else if (strncmp(key, "source-optional", 15) == 0) {
		parse_config_file(config, value, false);
	} else if (strncmp(key, "source", 6) == 0) {
		parse_config_file(config, value, true);
	} else {
		fprintf(stderr,
				"\033[1m\033[31m[ERROR]:\033[33m Unknown keyword: "
				"\033[1m\033[31m%s\n",
				key);
		return false;
	}

	return true;
}

/* Split a "key=value" config line, trim both sides, and forward to parse_option. */
bool parse_config_line(Config *config, const char *line) {
	char key[256], value[256];
	if (sscanf(line, "%255[^=]=%255[^\n]", key, value) != 2) {
		fprintf(stderr,
				"\033[1m\033[31m[ERROR]:\033[33m Invalid line format: %s",
				line);
		return false;
	}

	trim_whitespace(key);
	trim_whitespace(value);

	return parse_option(config, key, value);
}

/* Resolve file_path (handling ./ and ~/) and parse each non-comment line into config. */
bool parse_config_file(Config *config, const char *file_path, bool must_exist) {
	FILE *file;
	char full_path[1024];

	if (file_path[0] == '.' && file_path[1] == '/') {
		
		if (cli_config_path) {
			char *config_path = strdup(cli_config_path);
			char *config_dir = dirname(config_path);
			snprintf(full_path, sizeof(full_path), "%s/%s", config_dir,
					 file_path + 1);
			free(config_path);
		} else {
			const char *home = getenv("HOME");
			if (!home) {
				fprintf(stderr,
						"\033[1m\033[31m[ERROR]:\033[33m HOME environment "
						"variable not set.\n");
				return false;
			}
			snprintf(full_path, sizeof(full_path), "%s/.config/lemon/%s", home,
					 file_path + 1);
		}
		file = fopen(full_path, "r");

	} else if (file_path[0] == '~' &&
			   (file_path[1] == '/' || file_path[1] == '\0')) {
		
		const char *home = getenv("HOME");
		if (!home) {
			fprintf(stderr, "\033[1m\033[31m[ERROR]:\033[33m HOME environment "
							"variable not set.\n");
			return false;
		}
		snprintf(full_path, sizeof(full_path), "%s%s", home, file_path + 1);
		file = fopen(full_path, "r");

	} else {
		
		file = fopen(file_path, "r");
	}

	if (!file) {
		if (must_exist) {
			fprintf(stderr,
					"\033[1;31m\033[1;33m[ERROR]:\033[0m Failed to open "
					"config file: %s\n",
					file_path);
			return false;
		} else {
			return true;
		}
	}

	char line[512];
	bool parse_correct = true;
	bool parse_line_correct = true;
	uint32_t line_count = 0;
	while (fgets(line, sizeof(line), file)) {
		line_count++;
		if (line[0] == '#' || line[0] == '\n') {
			continue;
		}
		parse_line_correct = parse_config_line(config, line);
		if (!parse_line_correct) {
			parse_correct = false;
			fprintf(stderr,
					"\033[1;31m╰─\033[1;33m[Index]\033[0m "
					"\033[1;36m%s\033[0m:\033[1;35m%d\033[0m\n"
					"   \033[1;36m╰─\033[0;33m%s\033[0m\n\n",
					file_path, line_count, line);
		}
	}

	fclose(file);
	return parse_correct;
}

/* Free every entry of config->circle_layout and reset its count to zero. */
void free_circle_layout(Config *config) {
	if (config->circle_layout) {
		
		for (int32_t i = 0; i < config->circle_layout_count; i++) {
			if (config->circle_layout[i]) {
				free(config->circle_layout[i]);
				config->circle_layout[i] = NULL;
			}
		}
		
		free(config->circle_layout);
		config->circle_layout = NULL;
	}
	config->circle_layout_count = 0;
}

/* Release all precomputed bezier lookup tables used by the animation system. */
void free_baked_points(void) {
	if (baked_points_move) {
		free(baked_points_move);
		baked_points_move = NULL;
	}
	if (baked_points_open) {
		free(baked_points_open);
		baked_points_open = NULL;
	}
	if (baked_points_close) {
		free(baked_points_close);
		baked_points_close = NULL;
	}
	if (baked_points_tag) {
		free(baked_points_tag);
		baked_points_tag = NULL;
	}
	if (baked_points_focus) {
		free(baked_points_focus);
		baked_points_focus = NULL;
	}
	if (baked_points_opafadein) {
		free(baked_points_opafadein);
		baked_points_opafadein = NULL;
	}
	if (baked_points_opafadeout) {
		free(baked_points_opafadeout);
		baked_points_opafadeout = NULL;
	}
}

/* Free every heap allocation owned by the global config struct in preparation for reload or exit. */
void free_config(void) {
	
	int32_t i;

	if (config.window_rules) {
		for (int32_t i = 0; i < config.window_rules_count; i++) {
			ConfigWinRule *rule = &config.window_rules[i];
			if (rule->id)
				free((void *)rule->id);
			if (rule->title)
				free((void *)rule->title);
			if (rule->animation_type_open)
				free((void *)rule->animation_type_open);
			if (rule->animation_type_close)
				free((void *)rule->animation_type_close);
			if (rule->monitor)
				free((void *)rule->monitor);
			rule->id = NULL;
			rule->title = NULL;
			rule->animation_type_open = NULL;
			rule->animation_type_close = NULL;
			rule->monitor = NULL;
			
			if (rule->globalkeybinding.arg.v) {
				free((void *)rule->globalkeybinding.arg.v);
			}
		}
		free(config.window_rules);
		config.window_rules = NULL;
		config.window_rules_count = 0;
	}

	if (config.key_bindings) {
		for (i = 0; i < config.key_bindings_count; i++) {
			if (config.key_bindings[i].arg.v) {
				free((void *)config.key_bindings[i].arg.v);
				config.key_bindings[i].arg.v = NULL;
			}
			if (config.key_bindings[i].arg.v2) {
				free((void *)config.key_bindings[i].arg.v2);
				config.key_bindings[i].arg.v2 = NULL;
			}
			if (config.key_bindings[i].arg.v3) {
				free((void *)config.key_bindings[i].arg.v3);
				config.key_bindings[i].arg.v3 = NULL;
			}
		}
		free(config.key_bindings);
		config.key_bindings = NULL;
		config.key_bindings_count = 0;
	}

	if (config.mouse_bindings) {
		for (i = 0; i < config.mouse_bindings_count; i++) {
			if (config.mouse_bindings[i].arg.v) {
				free((void *)config.mouse_bindings[i].arg.v);
				config.mouse_bindings[i].arg.v = NULL;
			}
			if (config.mouse_bindings[i].arg.v2) {
				free((void *)config.mouse_bindings[i].arg.v2);
				config.mouse_bindings[i].arg.v2 = NULL;
			}
			if (config.mouse_bindings[i].arg.v3) {
				free((void *)config.mouse_bindings[i].arg.v3);
				config.mouse_bindings[i].arg.v3 = NULL;
			}
		}
		free(config.mouse_bindings);
		config.mouse_bindings = NULL;
		config.mouse_bindings_count = 0;
	}

	if (config.axis_bindings) {
		for (i = 0; i < config.axis_bindings_count; i++) {
			if (config.axis_bindings[i].arg.v) {
				free((void *)config.axis_bindings[i].arg.v);
				config.axis_bindings[i].arg.v = NULL;
			}
			if (config.axis_bindings[i].arg.v2) {
				free((void *)config.axis_bindings[i].arg.v2);
				config.axis_bindings[i].arg.v2 = NULL;
			}
			if (config.axis_bindings[i].arg.v3) {
				free((void *)config.axis_bindings[i].arg.v3);
				config.axis_bindings[i].arg.v3 = NULL;
			}
		}
		free(config.axis_bindings);
		config.axis_bindings = NULL;
		config.axis_bindings_count = 0;
	}

	if (config.switch_bindings) {
		for (i = 0; i < config.switch_bindings_count; i++) {
			if (config.switch_bindings[i].arg.v) {
				free((void *)config.switch_bindings[i].arg.v);
				config.switch_bindings[i].arg.v = NULL;
			}
			if (config.switch_bindings[i].arg.v2) {
				free((void *)config.switch_bindings[i].arg.v2);
				config.switch_bindings[i].arg.v2 = NULL;
			}
			if (config.switch_bindings[i].arg.v3) {
				free((void *)config.switch_bindings[i].arg.v3);
				config.switch_bindings[i].arg.v3 = NULL;
			}
		}
		free(config.switch_bindings);
		config.switch_bindings = NULL;
		config.switch_bindings_count = 0;
	}

	if (config.gesture_bindings) {
		for (i = 0; i < config.gesture_bindings_count; i++) {
			if (config.gesture_bindings[i].arg.v) {
				free((void *)config.gesture_bindings[i].arg.v);
				config.gesture_bindings[i].arg.v = NULL;
			}
			if (config.gesture_bindings[i].arg.v2) {
				free((void *)config.gesture_bindings[i].arg.v2);
				config.gesture_bindings[i].arg.v2 = NULL;
			}
			if (config.gesture_bindings[i].arg.v3) {
				free((void *)config.gesture_bindings[i].arg.v3);
				config.gesture_bindings[i].arg.v3 = NULL;
			}
		}
		free(config.gesture_bindings);
		config.gesture_bindings = NULL;
		config.gesture_bindings_count = 0;
	}

	if (config.idle_bindings) {
		for (i = 0; i < config.idle_bindings_count; i++) {
			if (config.idle_bindings[i].arg.v) {
				free((void *)config.idle_bindings[i].arg.v);
				config.idle_bindings[i].arg.v = NULL;
			}
			if (config.idle_bindings[i].arg.v2) {
				free((void *)config.idle_bindings[i].arg.v2);
				config.idle_bindings[i].arg.v2 = NULL;
			}
			if (config.idle_bindings[i].arg.v3) {
				free((void *)config.idle_bindings[i].arg.v3);
				config.idle_bindings[i].arg.v3 = NULL;
			}
		}
		free(config.idle_bindings);
		config.idle_bindings = NULL;
		config.idle_bindings_count = 0;
	}

	if (config.tag_rules) {
		for (int32_t i = 0; i < config.tag_rules_count; i++) {
			if (config.tag_rules[i].layout_name)
				free((void *)config.tag_rules[i].layout_name);
			if (config.tag_rules[i].monitor_name)
				free((void *)config.tag_rules[i].monitor_name);
			if (config.tag_rules[i].monitor_make)
				free((void *)config.tag_rules[i].monitor_make);
			if (config.tag_rules[i].monitor_model)
				free((void *)config.tag_rules[i].monitor_model);
			if (config.tag_rules[i].monitor_serial)
				free((void *)config.tag_rules[i].monitor_serial);
		}
		free(config.tag_rules);
		config.tag_rules = NULL;
		config.tag_rules_count = 0;
	}

	if (config.monitor_rules) {
		for (int32_t i = 0; i < config.monitor_rules_count; i++) {
			if (config.monitor_rules[i].name)
				free((void *)config.monitor_rules[i].name);
			if (config.monitor_rules[i].make)
				free((void *)config.monitor_rules[i].make);
			if (config.monitor_rules[i].model)
				free((void *)config.monitor_rules[i].model);
			if (config.monitor_rules[i].serial)
				free((void *)config.monitor_rules[i].serial);
		}
		free(config.monitor_rules);
		config.monitor_rules = NULL;
		config.monitor_rules_count = 0;
	}

	if (config.layer_rules) {
		for (int32_t i = 0; i < config.layer_rules_count; i++) {
			if (config.layer_rules[i].layer_name)
				free((void *)config.layer_rules[i].layer_name);
			if (config.layer_rules[i].animation_type_open)
				free((void *)config.layer_rules[i].animation_type_open);
			if (config.layer_rules[i].animation_type_close)
				free((void *)config.layer_rules[i].animation_type_close);
		}
		free(config.layer_rules);
		config.layer_rules = NULL;
		config.layer_rules_count = 0;
	}

	if (config.env) {
		for (int32_t i = 0; i < config.env_count; i++) {
			if (config.env[i]->type) {
				free((void *)config.env[i]->type);
			}
			if (config.env[i]->value) {
				free((void *)config.env[i]->value);
			}
			free(config.env[i]);
		}
		free(config.env);
		config.env = NULL;
		config.env_count = 0;
	}

	if (config.exec) {
		for (i = 0; i < config.exec_count; i++) {
			free(config.exec[i]);
		}
		free(config.exec);
		config.exec = NULL;
		config.exec_count = 0;
	}

	if (config.exec_once) {
		for (i = 0; i < config.exec_once_count; i++) {
			free(config.exec_once[i]);
		}
		free(config.exec_once);
		config.exec_once = NULL;
		config.exec_once_count = 0;
	}

	if (config.scroller_proportion_preset) {
		free(config.scroller_proportion_preset);
		config.scroller_proportion_preset = NULL;
		config.scroller_proportion_preset_count = 0;
	}

	if (config.cursor_theme) {
		free(config.cursor_theme);
		config.cursor_theme = NULL;
	}

	free_circle_layout(&config);

	free_baked_points();

	cleanup_config_keymap();
}

/* Clamp all numeric config fields into their allowed ranges after parsing. */
void override_config(void) {
	config.animations = CLAMP_INT(config.animations, 0, 1);
	config.layer_animations = CLAMP_INT(config.layer_animations, 0, 1);
	config.tag_animation_direction =
		CLAMP_INT(config.tag_animation_direction, 0, 1);
	config.animation_fade_in = CLAMP_INT(config.animation_fade_in, 0, 1);
	config.animation_fade_out = CLAMP_INT(config.animation_fade_out, 0, 1);
	config.zoom_initial_ratio =
		CLAMP_FLOAT(config.zoom_initial_ratio, 0.1f, 1.0f);
	config.zoom_end_ratio = CLAMP_FLOAT(config.zoom_end_ratio, 0.1f, 1.0f);
	config.fadein_begin_opacity =
		CLAMP_FLOAT(config.fadein_begin_opacity, 0.0f, 1.0f);
	config.fadeout_begin_opacity =
		CLAMP_FLOAT(config.fadeout_begin_opacity, 0.0f, 1.0f);
	config.animation_duration_move =
		CLAMP_INT(config.animation_duration_move, 1, 50000);
	config.animation_duration_open =
		CLAMP_INT(config.animation_duration_open, 1, 50000);
	config.animation_duration_tag =
		CLAMP_INT(config.animation_duration_tag, 1, 50000);
	config.animation_duration_close =
		CLAMP_INT(config.animation_duration_close, 1, 50000);
	config.animation_duration_focus =
		CLAMP_INT(config.animation_duration_focus, 1, 50000);
	config.scroller_default_proportion =
		CLAMP_FLOAT(config.scroller_default_proportion, 0.1f, 1.0f);
	config.scroller_default_proportion_single =
		CLAMP_FLOAT(config.scroller_default_proportion_single, 0.1f, 1.0f);
	config.scroller_ignore_proportion_single =
		CLAMP_INT(config.scroller_ignore_proportion_single, 0, 1);
	config.scroller_focus_center =
		CLAMP_INT(config.scroller_focus_center, 0, 1);
	config.scroller_prefer_center =
		CLAMP_INT(config.scroller_prefer_center, 0, 1);
	config.scroller_prefer_overspread =
		CLAMP_INT(config.scroller_prefer_overspread, 0, 1);
	config.edge_scroller_pointer_focus =
		CLAMP_INT(config.edge_scroller_pointer_focus, 0, 1);
	config.scroller_structs = CLAMP_INT(config.scroller_structs, 0, 1000);
	config.default_mfact = CLAMP_FLOAT(config.default_mfact, 0.1f, 0.9f);
	config.default_nmaster = CLAMP_INT(config.default_nmaster, 1, 1000);
	config.center_master_overspread =
		CLAMP_INT(config.center_master_overspread, 0, 1);
	config.center_when_single_stack =
		CLAMP_INT(config.center_when_single_stack, 0, 1);
	config.new_is_master = CLAMP_INT(config.new_is_master, 0, 1);
	config.dwindle_vsplit = CLAMP_INT(config.dwindle_vsplit, 0, 2);
	config.dwindle_hsplit = CLAMP_INT(config.dwindle_hsplit, 0, 2);
	config.dwindle_preserve_split =
		CLAMP_INT(config.dwindle_preserve_split, 0, 1);
	config.dwindle_smart_split = CLAMP_INT(config.dwindle_smart_split, 0, 1);
	config.dwindle_smart_resize = CLAMP_INT(config.dwindle_smart_resize, 0, 1);
	config.dwindle_drop_simple_split =
		CLAMP_INT(config.dwindle_drop_simple_split, 0, 1);
	config.dwindle_manual_split = CLAMP_INT(config.dwindle_manual_split, 0, 1);
	config.dwindle_split_ratio =
		CLAMP_FLOAT(config.dwindle_split_ratio, 0.05f, 0.95f);
	config.hotarea_size = CLAMP_INT(config.hotarea_size, 1, 1000);
	config.hotarea_corner = CLAMP_INT(config.hotarea_corner, 0, 3);
	config.enable_hotarea = CLAMP_INT(config.enable_hotarea, 0, 1);
	config.ov_tab_mode = CLAMP_INT(config.ov_tab_mode, 0, 1);
	config.overviewgappi = CLAMP_INT(config.overviewgappi, 0, 1000);
	config.overviewgappo = CLAMP_INT(config.overviewgappo, 0, 1000);
	config.overview_dim = CLAMP_INT(config.overview_dim, 0, 1);
	config.overview_dim_alpha = CLAMP_FLOAT(config.overview_dim_alpha, 0.0, 1.0);
	config.overview_borderpx = CLAMP_INT(config.overview_borderpx, 0, 100);
	config.xwayland_persistence = CLAMP_INT(config.xwayland_persistence, 0, 1);
	config.syncobj_enable = CLAMP_INT(config.syncobj_enable, 0, 1);
	if (config.idle_timeout < 0)
		config.idle_timeout = 0;
	if (config.idle_timeout_battery < 0)
		config.idle_timeout_battery = 0;
	config.battery_fps = CLAMP_INT(config.battery_fps, 1, 240);
	config.battery_timer_slack_ms =
		CLAMP_INT(config.battery_timer_slack_ms, 0, 1000);
	config.cpu_dma_latency_us =
		CLAMP_INT(config.cpu_dma_latency_us, -1, 1000000);
	config.late_latch = CLAMP_INT(config.late_latch, 0, 1);
	config.latency_margin_us = CLAMP_INT(config.latency_margin_us, 0, 16000);
	config.idle_action = CLAMP_INT(config.idle_action, IDLE_ACTION_OFF,
								   IDLE_ACTION_HIBERNATE);
	config.pre_idle_dim = CLAMP_INT(config.pre_idle_dim, 0, 1);
	config.pre_idle_dim_lead = CLAMP_INT(config.pre_idle_dim_lead, 1, 3600);
	config.pre_idle_dim_floor = CLAMP_INT(config.pre_idle_dim_floor, 1, 100);
	config.clipboard_history = CLAMP_INT(config.clipboard_history, 0, 1);
	config.clipboard_history_max_entries =
		CLAMP_INT(config.clipboard_history_max_entries, 1, 1000);
	config.clipboard_history_max_bytes =
		CLAMP_INT(config.clipboard_history_max_bytes, 1024, 64 * 1024 * 1024);
	config.focus_qos = CLAMP_INT(config.focus_qos, 0, 1);
	config.focus_qos_bg_nice = CLAMP_INT(config.focus_qos_bg_nice, 1, 19);
	config.tag_suspend_hidden = CLAMP_INT(config.tag_suspend_hidden, 0, 1);
	config.subpixel_rgb = CLAMP_INT(config.subpixel_rgb, 0, 1);
	config.debug_frametime = CLAMP_INT(config.debug_frametime, 0, 1);
	if (config.scroller_top_gap < -1)
		config.scroller_top_gap = -1;
	config.animation_spring = CLAMP_INT(config.animation_spring, 0, 1);
	config.animation_momentum = CLAMP_INT(config.animation_momentum, 0, 1);
	config.animation_momentum_scale =
		CLAMP_FLOAT(config.animation_momentum_scale, 0.0, 3.0);
	if (config.animation_spring_mass < 0.1)
		config.animation_spring_mass = 0.1;
	if (config.animation_spring_tension < 1.0)
		config.animation_spring_tension = 1.0;
	/* Friction must stay positive: zero damping never settles, so the
	   animation would never stop and the compositor could never sleep. */
	if (config.animation_spring_friction < 1.0)
		config.animation_spring_friction = 1.0;
	if (config.animation_spring_tag_tension < 1.0)
		config.animation_spring_tag_tension = 1.0;
	if (config.animation_spring_tag_friction < 1.0)
		config.animation_spring_tag_friction = 1.0;
	if (config.animation_spring_overview_tension < 1.0)
		config.animation_spring_overview_tension = 1.0;
	if (config.animation_spring_overview_friction < 1.0)
		config.animation_spring_overview_friction = 1.0;
	config.animation_motion_blur = CLAMP_INT(config.animation_motion_blur, 0, 1);
	if (config.animation_motion_blur_strength < 0.0)
		config.animation_motion_blur_strength = 0.0;
	if (config.animation_motion_blur_strength > 0.9)
		config.animation_motion_blur_strength = 0.9;
	config.drag_tile_refresh_interval =
		CLAMP_FLOAT(config.drag_tile_refresh_interval, 1.0f, 16.0f);
	config.drag_floating_refresh_interval =
		CLAMP_FLOAT(config.drag_floating_refresh_interval, 0.0f, 1000.0f);
	config.drag_tile_to_tile = CLAMP_INT(config.drag_tile_to_tile, 0, 1);
	config.drag_tile_small = CLAMP_INT(config.drag_tile_small, 0, 1);
	config.allow_tearing = CLAMP_INT(config.allow_tearing, 0, 2);
	config.allow_shortcuts_inhibit =
		CLAMP_INT(config.allow_shortcuts_inhibit, 0, 1);
	config.allow_lock_transparent =
		CLAMP_INT(config.allow_lock_transparent, 0, 1);
	config.axis_bind_apply_timeout =
		CLAMP_INT(config.axis_bind_apply_timeout, 0, 1000);
	config.focus_on_activate = CLAMP_INT(config.focus_on_activate, 0, 1);
	config.idleinhibit_ignore_visible =
		CLAMP_INT(config.idleinhibit_ignore_visible, 0, 1);
	config.sloppyfocus = CLAMP_INT(config.sloppyfocus, 0, 1);
	config.warpcursor = CLAMP_INT(config.warpcursor, 0, 1);
	config.drag_corner = CLAMP_INT(config.drag_corner, 0, 4);
	config.drag_warp_cursor = CLAMP_INT(config.drag_warp_cursor, 0, 1);
	config.focus_cross_monitor = CLAMP_INT(config.focus_cross_monitor, 0, 1);
	config.exchange_cross_monitor =
		CLAMP_INT(config.exchange_cross_monitor, 0, 1);
	config.scratchpad_cross_monitor =
		CLAMP_INT(config.scratchpad_cross_monitor, 0, 1);
	config.focus_cross_tag = CLAMP_INT(config.focus_cross_tag, 0, 1);
	config.view_current_to_back = CLAMP_INT(config.view_current_to_back, 0, 1);
	config.enable_floating_snap = CLAMP_INT(config.enable_floating_snap, 0, 1);
	config.snap_distance = CLAMP_INT(config.snap_distance, 0, 99999);
	config.cursor_size = CLAMP_INT(config.cursor_size, 4, 512);
	config.no_border_when_single =
		CLAMP_INT(config.no_border_when_single, 0, 1);
	config.no_radius_when_single =
		CLAMP_INT(config.no_radius_when_single, 0, 1);
	config.cursor_hide_timeout =
		CLAMP_INT(config.cursor_hide_timeout, 0, 36000);
	config.single_scratchpad = CLAMP_INT(config.single_scratchpad, 0, 1);
	config.repeat_rate = CLAMP_INT(config.repeat_rate, 1, 1000);
	config.repeat_delay = CLAMP_INT(config.repeat_delay, 1, 20000);
	config.numlockon = CLAMP_INT(config.numlockon, 0, 1);
	config.disable_trackpad = CLAMP_INT(config.disable_trackpad, 0, 1);
	config.tap_to_click = CLAMP_INT(config.tap_to_click, 0, 1);
	config.tap_and_drag = CLAMP_INT(config.tap_and_drag, 0, 1);
	config.drag_lock = CLAMP_INT(config.drag_lock, 0, 1);
	config.trackpad_natural_scrolling =
		CLAMP_INT(config.trackpad_natural_scrolling, 0, 1);
	config.disable_while_typing = CLAMP_INT(config.disable_while_typing, 0, 1);
	config.left_handed = CLAMP_INT(config.left_handed, 0, 1);
	config.middle_button_emulation =
		CLAMP_INT(config.middle_button_emulation, 0, 1);
	config.swipe_min_threshold = CLAMP_INT(config.swipe_min_threshold, 1, 1000);
	config.mouse_natural_scrolling =
		CLAMP_INT(config.mouse_natural_scrolling, 0, 1);
	config.mouse_accel_profile = CLAMP_INT(config.mouse_accel_profile, 0, 2);
	config.mouse_accel_speed =
		CLAMP_FLOAT(config.mouse_accel_speed, -1.0f, 1.0f);
	config.trackpad_accel_profile =
		CLAMP_INT(config.trackpad_accel_profile, 0, 2);
	config.trackpad_accel_speed =
		CLAMP_FLOAT(config.trackpad_accel_speed, -1.0f, 1.0f);
	config.scroll_method = CLAMP_INT(config.scroll_method, 0, 4);
	config.scroll_button = CLAMP_INT(config.scroll_button, 272, 279);
	config.click_method = CLAMP_INT(config.click_method, 0, 2);
	config.send_events_mode = CLAMP_INT(config.send_events_mode, 0, 2);
	config.button_map = CLAMP_INT(config.button_map, 0, 1);
	config.axis_scroll_factor =
		CLAMP_FLOAT(config.axis_scroll_factor, 0.1f, 10.0f);
	config.trackpad_scroll_factor =
		CLAMP_FLOAT(config.trackpad_scroll_factor, 0.1f, 10.0f);
	config.gappih = CLAMP_INT(config.gappih, 0, 1000);
	config.gappiv = CLAMP_INT(config.gappiv, 0, 1000);
	config.gappoh = CLAMP_INT(config.gappoh, 0, 1000);
	config.gappov = CLAMP_INT(config.gappov, 0, 1000);
	config.scratchpad_width_ratio =
		CLAMP_FLOAT(config.scratchpad_width_ratio, 0.1f, 1.0f);
	config.scratchpad_height_ratio =
		CLAMP_FLOAT(config.scratchpad_height_ratio, 0.1f, 1.0f);
	config.borderpx = CLAMP_INT(config.borderpx, 0, 200);
	config.smartgaps = CLAMP_INT(config.smartgaps, 0, 1);
	config.blur = CLAMP_INT(config.blur, 0, 1);
	config.blur_layer = CLAMP_INT(config.blur_layer, 0, 1);
	config.blur_optimized = CLAMP_INT(config.blur_optimized, 0, 1);
	config.border_radius = CLAMP_INT(config.border_radius, 0, 100);
	config.blur_params.num_passes =
		CLAMP_INT(config.blur_params.num_passes, 0, 10);
	config.blur_params.radius = CLAMP_INT(config.blur_params.radius, 0, 100);
	config.blur_params.noise = CLAMP_FLOAT(config.blur_params.noise, 0, 1);
	config.blur_params.brightness =
		CLAMP_FLOAT(config.blur_params.brightness, 0, 1);
	config.blur_params.contrast =
		CLAMP_FLOAT(config.blur_params.contrast, 0, 1);
	config.blur_params.saturation =
		CLAMP_FLOAT(config.blur_params.saturation, 0, 1);
	config.shadows = CLAMP_INT(config.shadows, 0, 1);
	config.shadow_only_floating = CLAMP_INT(config.shadow_only_floating, 0, 1);
	config.layer_shadows = CLAMP_INT(config.layer_shadows, 0, 1);
	config.shadows_size = CLAMP_INT(config.shadows_size, 0, 100);
	config.shadows_blur = CLAMP_INT(config.shadows_blur, 0, 100);
	config.shadows_position_x =
		CLAMP_INT(config.shadows_position_x, -1000, 1000);
	config.shadows_position_y =
		CLAMP_INT(config.shadows_position_y, -1000, 1000);
	config.focused_opacity = CLAMP_FLOAT(config.focused_opacity, 0.0f, 1.0f);
	config.unfocused_opacity =
		CLAMP_FLOAT(config.unfocused_opacity, 0.0f, 1.0f);
}

/* Initialize the global config with built-in default values for every setting. */
void set_value_default() {
	config.animations = 1;
	config.layer_animations = 0;
	config.animation_fade_in = 1;
	config.animation_fade_out = 1;
	config.tag_animation_direction = HORIZONTAL;
	config.zoom_initial_ratio = 0.4f;
	config.zoom_end_ratio = 0.8f;
	config.fadein_begin_opacity = 0.5f;
	config.fadeout_begin_opacity = 0.5f;
	/* Snappier defaults: legacy bezier paths (open/close/move/tag) all
	   halved so the unsprung animations finish in well under a frame
	   budget on a 60 Hz display. focus stays instant; spring-driven
	   geometry transitions use animation_spring_* below. */
	config.animation_duration_move = 240;
	config.animation_duration_open = 200;
	config.animation_duration_tag = 180;
	config.animation_duration_close = 150;
	config.animation_duration_focus = 0;

	config.axis_bind_apply_timeout = 100;
	config.focus_on_activate = 1;
	config.new_is_master = 1;
	config.default_mfact = 0.55f;
	config.default_nmaster = 1;
	config.center_master_overspread = 0;
	config.center_when_single_stack = 1;

	config.dwindle_vsplit = 1;
	config.dwindle_hsplit = 1;
	config.dwindle_preserve_split = 0;
	config.dwindle_smart_split = 0;
	config.dwindle_smart_resize = 0;
	config.dwindle_drop_simple_split = 1;
	config.dwindle_manual_split = 0;
	config.dwindle_split_ratio = 0.5f;

	config.log_level = WLR_ERROR;
	config.numlockon = 0;
	config.capslock = 0;

	config.ov_tab_mode = 0;
	config.hotarea_size = 10;
	config.hotarea_corner = BOTTOM_LEFT;
	config.enable_hotarea = 1;
	config.smartgaps = 0;
	config.sloppyfocus = 1;
	config.gappih = 5;
	config.gappiv = 5;
	config.gappoh = 10;
	config.gappov = 10;
	config.scratchpad_width_ratio = 0.8f;
	config.scratchpad_height_ratio = 0.9f;

	config.scroller_structs = 20;
	config.scroller_default_proportion = 0.9f;
	config.scroller_default_proportion_single = 1.0f;
	config.scroller_ignore_proportion_single = 1;
	config.scroller_focus_center = 0;
	config.scroller_prefer_center = 0;
	config.scroller_prefer_overspread = 1;
	config.edge_scroller_pointer_focus = 1;
	config.focus_cross_monitor = 0;
	config.exchange_cross_monitor = 0;
	config.scratchpad_cross_monitor = 0;
	config.focus_cross_tag = 0;
	config.axis_scroll_factor = 1.0;
	config.trackpad_scroll_factor = 1.0;
	config.view_current_to_back = 0;
	config.single_scratchpad = 1;
	config.xwayland_persistence = 1;
	config.syncobj_enable = 1;
	config.idle_timeout = 300;
	config.idle_timeout_battery = 0;
	config.battery_fps = 60;
	config.battery_timer_slack_ms = 50;
	config.cpu_dma_latency_us = -1;
	config.late_latch = 0;
	config.latency_margin_us = 2000;
	config.idle_action = IDLE_ACTION_OFF;
	config.pre_idle_dim = 0;
	config.pre_idle_dim_lead = 30;
	config.pre_idle_dim_floor = 10;
	config.focus_qos = 0;
	config.focus_qos_bg_nice = 10;
	config.tag_suspend_hidden = 0;
	config.clipboard_history = 1;
	config.clipboard_history_max_entries = 100;
	config.clipboard_history_max_bytes = 1 * 1024 * 1024;
	config.subpixel_rgb = 0;
	config.debug_frametime = 0;
	config.scroller_top_gap = -1;
	config.animation_spring = 1;
	config.animation_spring_mass = 1.0;
	/* Snappy near-critical damping (no bounce). c_crit for k,m=1 is
	   2*sqrt(k); the friction matches so the spring settles cleanly. */
	config.animation_spring_tension = 180.0;
	config.animation_spring_friction = 27.0;
	/* Workspace slides: faster spring -- critical damping at k=260. */
	config.animation_spring_tag_tension = 260.0;
	config.animation_spring_tag_friction = 32.0;
	config.animation_momentum = 1;
	config.animation_momentum_scale = 1.0;
	/* Overview enter/exit: snappiest spring -- critical damping at k=340. */
	config.animation_spring_overview_tension = 340.0;
	config.animation_spring_overview_friction = 37.0;
	config.animation_motion_blur = 0;
	config.animation_motion_blur_strength = 0.3;
	config.drag_tile_refresh_interval = 8.0f;
	config.drag_floating_refresh_interval = 8.0f;
	config.allow_tearing = TEARING_DISABLED;
	config.allow_shortcuts_inhibit = SHORTCUTS_INHIBIT_ENABLE;
	config.allow_lock_transparent = 0;
	config.no_border_when_single = 0;
	config.no_radius_when_single = 0;
	config.snap_distance = 30;
	config.drag_tile_to_tile = 0;
	config.drag_tile_small = 1;
	config.enable_floating_snap = 0;
	config.swipe_min_threshold = 1;

	config.idleinhibit_ignore_visible = 0;

	config.borderpx = 4;
	config.overviewgappi = 5;
	config.overviewgappo = 30;
	config.overview_dim = 1;
	config.overview_dim_alpha = 0.55;
	config.overview_borderpx = 6;
	config.cursor_hide_timeout = 0;

	config.warpcursor = 1;
	config.drag_corner = 3;
	config.drag_warp_cursor = 1;

	config.repeat_rate = 25;
	config.repeat_delay = 600;

	config.disable_trackpad = 0;
	config.tap_to_click = 1;
	config.tap_and_drag = 1;
	config.drag_lock = 1;
	config.mouse_natural_scrolling = 0;
	config.cursor_size = 24;
	config.trackpad_natural_scrolling = 0;
	config.disable_while_typing = 1;
	config.left_handed = 0;
	config.middle_button_emulation = 0;
	config.mouse_accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
	config.mouse_accel_speed = 0.0;
	config.trackpad_accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
	config.trackpad_accel_speed = 0.0;
	config.scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
	config.scroll_button = 274;
	config.click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
	config.send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
	config.button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;

	config.blur = 0;
	config.blur_layer = 0;
	config.blur_optimized = 1;
	config.border_radius = 0;
	config.border_radius_location_default = CORNER_LOCATION_ALL;
	config.blur_params.num_passes = 1;
	config.blur_params.radius = 5;
	config.blur_params.noise = 0.02f;
	config.blur_params.brightness = 0.9f;
	config.blur_params.contrast = 0.9f;
	config.blur_params.saturation = 1.2f;
	config.shadows = 0;
	config.shadow_only_floating = 1;
	config.layer_shadows = 0;
	config.shadows_size = 10;
	config.shadows_blur = 15.0f;
	config.shadows_position_x = 0;
	config.shadows_position_y = 0;
	config.focused_opacity = 1.0f;
	config.unfocused_opacity = 1.0f;

	config.shadowscolor[0] = 0.0f;
	config.shadowscolor[1] = 0.0f;
	config.shadowscolor[2] = 0.0f;
	config.shadowscolor[3] = 1.0f;

	config.animation_curve_move[0] = 0.46;
	config.animation_curve_move[1] = 1.0;
	config.animation_curve_move[2] = 0.29;
	config.animation_curve_move[3] = 0.99;
	config.animation_curve_open[0] = 0.46;
	config.animation_curve_open[1] = 1.0;
	config.animation_curve_open[2] = 0.29;
	config.animation_curve_open[3] = 0.99;
	config.animation_curve_tag[0] = 0.46;
	config.animation_curve_tag[1] = 1.0;
	config.animation_curve_tag[2] = 0.29;
	config.animation_curve_tag[3] = 0.99;
	config.animation_curve_close[0] = 0.46;
	config.animation_curve_close[1] = 1.0;
	config.animation_curve_close[2] = 0.29;
	config.animation_curve_close[3] = 0.99;
	config.animation_curve_focus[0] = 0.46;
	config.animation_curve_focus[1] = 1.0;
	config.animation_curve_focus[2] = 0.29;
	config.animation_curve_focus[3] = 0.99;
	config.animation_curve_opafadein[0] = 0.46;
	config.animation_curve_opafadein[1] = 1.0;
	config.animation_curve_opafadein[2] = 0.29;
	config.animation_curve_opafadein[3] = 0.99;
	config.animation_curve_opafadeout[0] = 0.5;
	config.animation_curve_opafadeout[1] = 0.5;
	config.animation_curve_opafadeout[2] = 0.5;
	config.animation_curve_opafadeout[3] = 0.5;

	config.rootcolor[0] = 0x32 / 255.0f;
	config.rootcolor[1] = 0x32 / 255.0f;
	config.rootcolor[2] = 0x32 / 255.0f;
	config.rootcolor[3] = 1.0f;
	config.bordercolor[0] = 0x44 / 255.0f;
	config.bordercolor[1] = 0x44 / 255.0f;
	config.bordercolor[2] = 0x44 / 255.0f;
	config.bordercolor[3] = 1.0f;
	config.dropcolor[0] = 0xd5 / 255.0f;
	config.dropcolor[1] = 0x89 / 255.0f;
	config.dropcolor[2] = 0x9d / 255.0f;
	config.dropcolor[3] = 0.5f;
	config.splitcolor[0] = 0xeb / 255.0f;
	config.splitcolor[1] = 0x44 / 255.0f;
	config.splitcolor[2] = 0x1e / 255.0f;
	config.splitcolor[3] = 1.0f;
	config.focuscolor[0] = 0xc6 / 255.0f;
	config.focuscolor[1] = 0x6b / 255.0f;
	config.focuscolor[2] = 0x25 / 255.0f;
	config.focuscolor[3] = 1.0f;
	config.maximizescreencolor[0] = 0x89 / 255.0f;
	config.maximizescreencolor[1] = 0xaa / 255.0f;
	config.maximizescreencolor[2] = 0x61 / 255.0f;
	config.maximizescreencolor[3] = 1.0f;
	config.urgentcolor[0] = 0xad / 255.0f;
	config.urgentcolor[1] = 0x40 / 255.0f;
	config.urgentcolor[2] = 0x1f / 255.0f;
	config.urgentcolor[3] = 1.0f;
	config.scratchpadcolor[0] = 0x51 / 255.0f;
	config.scratchpadcolor[1] = 0x6c / 255.0f;
	config.scratchpadcolor[2] = 0x93 / 255.0f;
	config.scratchpadcolor[3] = 1.0f;
	config.globalcolor[0] = 0xb1 / 255.0f;
	config.globalcolor[1] = 0x53 / 255.0f;
	config.globalcolor[2] = 0xa7 / 255.0f;
	config.globalcolor[3] = 1.0f;
	config.overlaycolor[0] = 0x14 / 255.0f;
	config.overlaycolor[1] = 0xa5 / 255.0f;
	config.overlaycolor[2] = 0x7c / 255.0f;
	config.overlaycolor[3] = 1.0f;
}

/* Append the hard-coded CHVT key bindings to config->key_bindings. */
void set_default_key_bindings(Config *config) {
	
	size_t default_key_bindings_count =
		sizeof(default_key_bindings) / sizeof(KeyBinding);

	config->key_bindings =
		realloc(config->key_bindings,
				(config->key_bindings_count + default_key_bindings_count) *
					sizeof(KeyBinding));
	if (!config->key_bindings) {
		return;
	}

	for (size_t i = 0; i < default_key_bindings_count; i++) {
		config->key_bindings[config->key_bindings_count + i] =
			default_key_bindings[i];
		config->key_bindings[config->key_bindings_count + i].iscommonmode =
			true;
		config->key_bindings[config->key_bindings_count + i].islockapply = true;
	}

	config->key_bindings_count += default_key_bindings_count;
}

/* Entry point that resets config, loads defaults, reads lemon.conf, and clamps results. */
bool parse_config(void) {

	char filename[1024];

	free_config();

	memset(&config, 0, sizeof(config));

	config.xkb_rules.layout = config.xkb_rules_layout;
	config.xkb_rules.variant = config.xkb_rules_variant;
	config.xkb_rules.options = config.xkb_rules_options;
	config.xkb_rules.rules = config.xkb_rules_rules;
	config.xkb_rules.model = config.xkb_rules_model;

	config.window_rules = NULL;
	config.window_rules_count = 0;
	config.monitor_rules = NULL;
	config.monitor_rules_count = 0;
	config.key_bindings = NULL;
	config.key_bindings_count = 0;
	config.mouse_bindings = NULL;
	config.mouse_bindings_count = 0;
	config.axis_bindings = NULL;
	config.axis_bindings_count = 0;
	config.switch_bindings = NULL;
	config.switch_bindings_count = 0;
	config.gesture_bindings = NULL;
	config.gesture_bindings_count = 0;
	config.idle_bindings = NULL;
	config.idle_bindings_count = 0;
	config.env = NULL;
	config.env_count = 0;
	config.exec = NULL;
	config.exec_count = 0;
	config.exec_once = NULL;
	config.exec_once_count = 0;
	config.scroller_proportion_preset = NULL;
	config.scroller_proportion_preset_count = 0;
	config.circle_layout = NULL;
	config.circle_layout_count = 0;
	config.tag_rules = NULL;
	config.tag_rules_count = 0;
	config.cursor_theme = NULL;
	strcpy(config.keymode, "default");

	create_config_keymap();

	if (cli_config_path) {
		snprintf(filename, sizeof(filename), "%s", cli_config_path);
	} else {
		
		const char *homedir = getenv("HOME");
		if (!homedir) {
			
			return false;
		}
		
		snprintf(filename, sizeof(filename), "%s/.config/lemon/lemon.conf",
				 homedir);
	}

	bool parse_correct = true;
	set_value_default();
	parse_correct = parse_config_file(&config, filename, true);
	set_default_key_bindings(&config);
	override_config();
	return parse_correct;
}

/* Recreate or destroy the per-monitor optimized blur scene nodes to match the current config. */
void reset_blur_params(void) {
	
	{
		Monitor *m_inv;
		LayerSurface *l_inv;
		wl_list_for_each(m_inv, &mons, link) {
			for (size_t li = 0; li < LENGTH(m_inv->layers); li++) {
				wl_list_for_each(l_inv, &m_inv->layers[li], link) {
					l_inv->blur_state_applied = false;
				}
			}
		}
	}

	if (config.blur) {
		Monitor *m = NULL;
		wl_list_for_each(m, &mons, link) {
			if (m->blur != NULL) {
				wlr_scene_node_destroy(&m->blur->node);
			}
			m->blur = wlr_scene_optimized_blur_create(&scene->tree, 0, 0);
			wlr_scene_node_reparent(&m->blur->node, layers[LyrBlur]);
			wlr_scene_optimized_blur_set_size(m->blur, m->m.width, m->m.height);
			wlr_scene_set_blur_data(
				scene, config.blur_params.num_passes, config.blur_params.radius,
				config.blur_params.noise, config.blur_params.brightness,
				config.blur_params.contrast, config.blur_params.saturation);
		}
	} else {
		Monitor *m = NULL;
		wl_list_for_each(m, &mons, link) {

			if (m->blur) {
				wlr_scene_node_destroy(&m->blur->node);
				m->blur = NULL;
			}
		}
	}
}

/* Walk all enabled monitors and re-commit the first matching ConfigMonitorRule to each. */
void reapply_monitor_rules(void) {
	ConfigMonitorRule *mr;
	Monitor *m = NULL;
	int32_t ji, vrr, custom;
	int32_t mx, my;
	struct wlr_output_state state;

	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled)
			continue;

		wlr_output_state_init(&state);

		for (ji = 0; ji < config.monitor_rules_count; ji++) {
			if (config.monitor_rules_count < 1)
				break;

			mr = &config.monitor_rules[ji];

			if (monitor_matches_rule(m, mr)) {
				mx = mr->x == INT32_MAX ? m->m.x : mr->x;
				my = mr->y == INT32_MAX ? m->m.y : mr->y;
				vrr = mr->vrr >= 0 ? mr->vrr : 0;
				custom = mr->custom >= 0 ? mr->custom : 0;

				(void)apply_rule_to_state(m, mr, &state, vrr, custom);
				wlr_output_layout_add(output_layout, m->wlr_output, mx, my);
				wlr_output_commit_state(m->wlr_output, &state);
				break;
			}
		}

		wlr_output_state_finish(&state);
	}
	updatemons(NULL, NULL);
}

/* Export XCURSOR_SIZE and XCURSOR_THEME env vars based on the current cursor config. */
void set_xcursor_env() {
	if (config.cursor_size > 0) {
		char size_str[16];
		snprintf(size_str, sizeof(size_str), "%d", config.cursor_size);
		setenv("XCURSOR_SIZE", size_str, 1);
	} else {
		setenv("XCURSOR_SIZE", "24", 1);
	}

	if (config.cursor_theme) {
		setenv("XCURSOR_THEME", config.cursor_theme, 1);
	}
}

/* Rebuild the xcursor manager and hide-timer using the latest cursor theme/size settings. */
void reapply_cursor_style(void) {
	if (hide_cursor_source) {
		wl_event_source_timer_update(hide_cursor_source, 0);
		wl_event_source_remove(hide_cursor_source);
		hide_cursor_source = NULL;
	}

	wlr_cursor_unset_image(cursor);

	wlr_cursor_set_surface(cursor, NULL, 0, 0);

	if (cursor_mgr) {
		wlr_xcursor_manager_destroy(cursor_mgr);
		cursor_mgr = NULL;
	}

	set_xcursor_env();

	cursor_mgr =
		wlr_xcursor_manager_create(config.cursor_theme, config.cursor_size);

	Monitor *m = NULL;
	wl_list_for_each(m, &mons, link) {
		wlr_xcursor_manager_load(cursor_mgr, m->wlr_output->scale);
	}

	wlr_cursor_set_xcursor(cursor, cursor_mgr, "left_ptr");

	hide_cursor_source = wl_event_loop_add_timer(wl_display_get_event_loop(dpy),
												 hidecursor, cursor);
	if (cursor_hidden) {
		wlr_cursor_unset_image(cursor);
	} else {
		wl_event_source_timer_update(hide_cursor_source,
									 config.cursor_hide_timeout * 1000);
	}
}

/* Re-apply the configured root background color to the scene's root rectangle. */
void reapply_rootbg(void) {
	wlr_scene_rect_set_color(root_bg, config.rootcolor);
}

/* Refresh border width and drop/split indicator colors on every live client after reload. */
void reapply_property(void) {
	Client *c = NULL;

	wl_list_for_each(c, &clients, link) {
		if (c && !c->iskilling) {
			if (!c->isnoborder && !c->isfullscreen) {
				c->bw = config.borderpx;
			}

			wlr_scene_rect_set_color(c->droparea, config.dropcolor);
			wlr_scene_rect_set_color(c->splitindicator[0], config.splitcolor);
			wlr_scene_rect_set_color(c->splitindicator[1], config.splitcolor);
		}
	}
}

/* Push the new repeat rate and delay onto every connected keyboard. */
void reapply_keyboard(void) {
	InputDevice *id;
	wl_list_for_each(id, &inputdevices, link) {
		if (id->wlr_device->type != WLR_INPUT_DEVICE_KEYBOARD) {
			continue;
		}
		wlr_keyboard_set_repeat_info((struct wlr_keyboard *)id->device_data,
									 config.repeat_rate, config.repeat_delay);
	}
}

/* Re-run libinput pointer configuration on every connected pointer device. */
void reapply_pointer(void) {
	InputDevice *id;
	struct libinput_device *device;
	wl_list_for_each(id, &inputdevices, link) {

		if (id->wlr_device->type != WLR_INPUT_DEVICE_POINTER) {
			continue;
		}

		device = id->libinput_device;
		if (wlr_input_device_is_libinput(id->wlr_device) && device) {
			configure_pointer(device);
		}
	}
}

/* Reset per-tag master/mfact and gap fields on every monitor to the configured defaults. */
void reapply_master(void) {

	int32_t i;
	Monitor *m = NULL;
	for (i = 0; i <= LENGTH(tags); i++) {
		wl_list_for_each(m, &mons, link) {
			if (!m->wlr_output->enabled) {
				continue;
			}
			m->pertag->nmasters[i] = config.default_nmaster;
			m->pertag->mfacts[i] = config.default_mfact;
			m->gappih = config.gappih;
			m->gappiv = config.gappiv;
			m->gappoh = config.gappoh;
			m->gappov = config.gappov;
		}
	}
}

/* Apply every matching ConfigTagRule (layout, mfact, nmaster, flags) to monitor m's pertag state. */
void parse_tagrule(Monitor *m) {
	int32_t i, jk;
	ConfigTagRule tr;
	Client *c = NULL;
	bool match_rule = false;

	for (i = 0; i <= LENGTH(tags); i++) {
		m->pertag->nmasters[i] = config.default_nmaster;
		m->pertag->mfacts[i] = config.default_mfact;
	}

	for (i = 0; i < config.tag_rules_count; i++) {

		tr = config.tag_rules[i];

		match_rule = true;

		if (tr.monitor_name != NULL) {
			if (!regex_match(tr.monitor_name, m->wlr_output->name)) {
				match_rule = false;
			}
		}

		if (tr.monitor_make != NULL) {
			if (m->wlr_output->make == NULL ||
				strcmp(tr.monitor_make, m->wlr_output->make) != 0) {
				match_rule = false;
			}
		}

		if (tr.monitor_model != NULL) {
			if (m->wlr_output->model == NULL ||
				strcmp(tr.monitor_model, m->wlr_output->model) != 0) {
				match_rule = false;
			}
		}

		if (tr.monitor_serial != NULL) {
			if (m->wlr_output->serial == NULL ||
				strcmp(tr.monitor_serial, m->wlr_output->serial) != 0) {
				match_rule = false;
			}
		}

		if (config.tag_rules_count > 0 && match_rule) {

			for (jk = 0; jk < LENGTH(layouts); jk++) {
				if (tr.layout_name &&
					strcmp(layouts[jk].name, tr.layout_name) == 0) {
					m->pertag->ltidxs[tr.id] = &layouts[jk];
				}
			}

			if (tr.no_hide >= 0)
				m->pertag->no_hide[tr.id] = tr.no_hide;
			if (tr.nmaster >= 1)
				m->pertag->nmasters[tr.id] = tr.nmaster;
			if (tr.mfact > 0.0f)
				m->pertag->mfacts[tr.id] = tr.mfact;
			if (tr.no_render_border >= 0)
				m->pertag->no_render_border[tr.id] = tr.no_render_border;
			if (tr.open_as_floating >= 0)
				m->pertag->open_as_floating[tr.id] = tr.open_as_floating;
		}
	}

	for (i = 1; i <= LENGTH(tags); i++) {
		wl_list_for_each(c, &clients, link) {
			if ((c->tags & (1 << (i - 1)) & TAGMASK) && ISTILED(c)) {
				if (m->pertag->mfacts[i] > 0.0f)
					c->master_mfact_per = m->pertag->mfacts[i];
			}
		}
	}
}

/* Run parse_tagrule on every enabled monitor to refresh tag-specific settings. */
void reapply_tagrule(void) {
	Monitor *m = NULL;
	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled) {
			continue;
		}
		parse_tagrule(m);
	}
}

/* Re-apply every hot-reloadable subsystem (cursor, keyboard, blur, rules, layout) after a reload. */
void reset_option(void) {
	init_baked_points();
	handlecursoractivity();
	reset_keyboard_layout();
	reset_blur_params();
	set_env();
	run_exec();

	reapply_cursor_style();
	reapply_property();
	reapply_rootbg();
	reapply_keyboard();
	reapply_pointer();
	reapply_master();

	reapply_tagrule();
	reapply_monitor_rules();

	arrange(selmon, false, false);
}

/* Keybind handler that re-parses the config file and reapplies all live settings. */
int32_t reload_config(const Arg *arg) {
	parse_config();
	reset_option();
	setup_idle_timers();
	printstatus();
	return 1;
}
