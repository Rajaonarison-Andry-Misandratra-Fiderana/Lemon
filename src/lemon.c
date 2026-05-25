
#include "wlr-layer-shell-unstable-v1-protocol.h"
#include "wlr/util/box.h"
#include "wlr/util/edges.h"
#include <dirent.h>
#include <getopt.h>
#include <libinput.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <scenefx/render/fx_renderer/fx_renderer.h>
#include <scenefx/types/fx/blur_data.h>
#include <scenefx/types/fx/clipped_region.h>
#include <scenefx/types/fx/corner_location.h>
#include <scenefx/types/wlr_scene.h>
#include <signal.h>
#include <spawn.h>
#include <sched.h>
#include <malloc.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/signalfd.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/libinput.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/wayland.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_drm_lease_v1.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_ext_data_control_v1.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_keyboard_shortcuts_inhibit_v1.h>
#include <wlr/types/wlr_content_type_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_switch.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_foreign_registry.h>
#include <wlr/types/wlr_xdg_foreign_v1.h>
#include <wlr/types/wlr_xdg_foreign_v2.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <wlr/util/region.h>
#include <wordexp.h>
#include <xkbcommon/xkbcommon.h>
#ifdef XWAYLAND
#include <X11/Xlib.h>
#include <wlr/xwayland.h>
#include <xcb/xcb_icccm.h>
#endif
#include "common/util.h"
#include "common/surface_cache.h"

#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define GEZERO(A) ((A) >= 0 ? (A) : 0)
#define CLEANMASK(mask) (mask & ~WLR_MODIFIER_CAPS)
#define INSIDEMON(A)                                                           \
	(A->geom.x >= A->mon->m.x && A->geom.y >= A->mon->m.y &&                   \
	 A->geom.x + A->geom.width <= A->mon->m.x + A->mon->m.width &&             \
	 A->geom.y + A->geom.height <= A->mon->m.y + A->mon->m.height)
#define GEOMINSIDEMON(A, M)                                                    \
	(A->x >= M->m.x && A->y >= M->m.y &&                                       \
	 A->x + A->width <= M->m.x + M->m.width &&                                 \
	 A->y + A->height <= M->m.y + M->m.height)
#define ISTILED(A)                                                             \
	(A && !(A)->isfloating && !(A)->isminimized && !(A)->iskilling &&          \
	 !(A)->ismaximizescreen && !(A)->isfullscreen && !(A)->isunglobal)
#define ISSCROLLTILED(A)                                                       \
	(A && !(A)->isfloating && !(A)->isminimized && !(A)->iskilling &&          \
	 !(A)->isunglobal)
#define VISIBLEON(C, M)                                                        \
	((C) && (M) && (C)->mon == (M) &&                                          \
	 (((C)->tags & (M)->tagset[(M)->seltags] || (C)->isglobal ||               \
	   (C)->isunglobal)))
#define LENGTH(X) (sizeof X / sizeof X[0])
#define END(A) ((A) + LENGTH(A))
#define TAGMASK ((1 << LENGTH(tags)) - 1)
#define LISTEN(E, L, H) wl_signal_add((E), ((L)->notify = (H), (L)))
#define ISFULLSCREEN(A)                                                        \
	((A)->isfullscreen || (A)->ismaximizescreen ||                             \
	 (A)->overview_ismaximizescreenbak || (A)->overview_isfullscreenbak)
#define LISTEN_STATIC(E, H)                                                    \
	do {                                                                       \
		struct wl_listener *_l = ecalloc(1, sizeof(*_l));                      \
		_l->notify = (H);                                                      \
		wl_signal_add((E), _l);                                                \
	} while (0)

#define APPLY_INT_PROP(obj, rule, prop)                                        \
	if (rule->prop >= 0)                                                       \
	obj->prop = rule->prop

#define APPLY_FLOAT_PROP(obj, rule, prop)                                      \
	if (rule->prop > 0.0f)                                                     \
	obj->prop = rule->prop

#define APPLY_STRING_PROP(obj, rule, prop)                                     \
	if (rule->prop != NULL)                                                    \
	obj->prop = rule->prop

#define BAKED_POINTS_COUNT 256

enum { TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };

enum { VERTICAL, HORIZONTAL };
enum { TIER_FOCUS = 0, TIER_VISIBLE = 1, TIER_OCCLUDED = 2, TIER_HIDDEN = 3 };
#define TIER_OCCLUDED_INTERVAL_MS 33u
#define TIER_HIDDEN_INTERVAL_MS 200u

enum { SWIPE_UP, SWIPE_DOWN, SWIPE_LEFT, SWIPE_RIGHT };
enum { CurNormal, CurPressed, CurMove, CurResize };
enum { XDGShell, LayerShell, X11 };
enum { AxisUp, AxisDown, AxisLeft, AxisRight };
enum {
	LyrBg,
	LyrBlur,
	LyrBottom,
	LyrTile,
	LyrTop,
	LyrFadeOut,
	LyrOverlay,
	LyrIMPopup,
	LyrBlock,
	NUM_LAYERS
};
#ifdef XWAYLAND
enum {
	NetWMWindowTypeDialog,
	NetWMWindowTypeSplash,
	NetWMWindowTypeToolbar,
	NetWMWindowTypeUtility,
	NetLast
};
#endif
enum { UP, DOWN, LEFT, RIGHT, UNDIR };
enum { NONE, OPEN, MOVE, CLOSE, TAG, FOCUS, OPAFADEIN, OPAFADEOUT };
enum { ANIM_TYPE_NONE = 0, ANIM_TYPE_FADE, ANIM_TYPE_ZOOM, ANIM_TYPE_SLIDE };

#define CLIENT_SET_FOREIGN_ACTIVATED(c, val) do { \
	if ((c) && (c)->foreign_toplevel && \
	    (c)->foreign_activated_state != (val)) { \
		wlr_foreign_toplevel_handle_v1_set_activated( \
			(c)->foreign_toplevel, (val)); \
		(c)->foreign_activated_state = (val); \
	} \
} while (0)

/* Parse an animation-type keyword string into the matching ANIM_TYPE_* enum value. */
static inline int32_t anim_type_parse(const char *s) {
	if (!s || !s[0]) return ANIM_TYPE_NONE;
	switch (s[0]) {
	case 'z': if (strcmp(s, "zoom") == 0)  return ANIM_TYPE_ZOOM;  break;
	case 'f': if (strcmp(s, "fade") == 0)  return ANIM_TYPE_FADE;  break;
	case 's': if (strcmp(s, "slide") == 0) return ANIM_TYPE_SLIDE; break;
	case 'n': if (strcmp(s, "none") == 0)  return ANIM_TYPE_NONE;  break;
	}
	return ANIM_TYPE_NONE;
}

/* Return effective animation type: per-target string when set, otherwise config default. */
static inline int32_t anim_type_effective(const char *per_target,
                                          const char *config_default) {
	return anim_type_parse(per_target ? per_target : config_default);
}
enum { UNFOLD, FOLD, INVALIDFOLD };
enum { PREV, NEXT };
enum { STATE_UNSPECIFIED = 0, STATE_ENABLED, STATE_DISABLED };

enum tearing_mode {
	TEARING_DISABLED = 0,
	TEARING_ENABLED,
	TEARING_FULLSCREEN_ONLY,
};

enum seat_config_shortcuts_inhibit {
	SHORTCUTS_INHIBIT_DISABLE,
	SHORTCUTS_INHIBIT_ENABLE,
};

enum print_event_type {
	PRINT_ACTIVE = 1 << 0,
	PRINT_TAG = 1 << 1,
	PRINT_LAYOUT = 1 << 2,
	PRINT_TITLE = 1 << 3,
	PRINT_APPID = 1 << 4,
	PRINT_LAYOUT_SYMBOL = 1 << 5,
	PRINT_FULLSCREEN = 1 << 6,
	PRINT_FLOATING = 1 << 7,
	PRINT_X = 1 << 8,
	PRINT_Y = 1 << 9,
	PRINT_WIDTH = 1 << 10,
	PRINT_HEIGHT = 1 << 11,
	PRINT_LAST_LAYER = 1 << 12,
	PRINT_KB_LAYOUT = 1 << 13,
	PRINT_KEYMODE = 1 << 14,
	PRINT_SCALEFACTOR = 1 << 15,
	PRINT_FRAME = 1 << 16,
	PRINT_ALL = (1 << 17) - 1
};

typedef struct Pertag Pertag;
typedef struct Monitor Monitor;
typedef struct Client Client;

struct dvec2 {
	double x, y;
};

struct ivec2 {
	int32_t x, y, width, height;
};

typedef struct {
	int32_t i;
	int32_t i2;
	float f;
	float f2;
	char *v;
	char *v2;
	char *v3;
	uint32_t ui;
	uint32_t ui2;
} Arg;

typedef struct {
	uint32_t mod;
	uint32_t button;
	int32_t (*func)(const Arg *);
	const Arg arg;
} Button;

typedef struct {
	char mode[28];
	bool isdefault;
} KeyMode;

typedef struct {
	uint32_t mod;
	uint32_t dir;
	int32_t (*func)(const Arg *);
	const Arg arg;
} Axis;

typedef struct {
	struct wl_list link;
	struct wlr_input_device *wlr_device;
	struct libinput_device *libinput_device;
	struct wl_listener destroy_listener;
	void *device_data;
} InputDevice;

typedef struct {
	struct wl_list link;
	struct wlr_switch *wlr_switch;
	struct wl_listener toggle;
	InputDevice *input_dev;
} Switch;

struct dwl_animation {
	bool should_animate;
	bool running;
	bool tagining;
	bool tagouted;
	bool tagouting;
	bool begin_fade_in;
	bool tag_from_rule;
	uint32_t time_started;
	uint32_t duration;
	struct wlr_box initial;
	struct wlr_box current;
	int32_t action;

	/* Spring-physics geometry state (move/resize/tile). vis/vel are sub-pixel
	   x,y,width,height. Preserved across retargets so the animation is fully
	   interruptible. spring_init seeds vis from initial on a fresh start;
	   last_tick_ms gives the integrator its dt. */
	double vis[4];
	double vel[4];
	bool spring_init;
	uint32_t last_tick_ms;
	/* Momentum hand-off: input velocity (px/s) injected into vel[] on the first
	   spring tick, so a thrown/flung window keeps the gesture's momentum. */
	double vel_seed[4];
};

struct dwl_opacity_animation {
	bool running;
	float current_opacity;
	float target_opacity;
	float initial_opacity;
	uint32_t time_started;
	uint32_t duration;
	float current_border_color[4];
	float target_border_color[4];
	float initial_border_color[4];
};

typedef struct {
	float width_scale;
	float height_scale;
	int32_t width;
	int32_t height;
	enum corner_location corner_location;
	bool should_scale;
} BufferData;

struct Client {
	
	uint32_t type;
	struct wlr_box geom, pending, float_geom, animainit_geom,
		overview_backup_geom, current,
		drag_begin_geom;
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_rect *border;
	struct wlr_scene_rect *droparea;
	struct wlr_scene_rect *splitindicator[2];
	struct wlr_scene_shadow *shadow;
	struct wlr_scene_tree *scene_surface;
	struct wl_list link;
	struct wl_list flink;
	struct wl_list fadeout_link;
	union {
		struct wlr_xdg_surface *xdg;
		struct wlr_xwayland_surface *xwayland;
	} surface;
	struct wl_listener commit;
	struct wl_listener map;
	struct wl_listener maximize;
	struct wl_listener minimize;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener set_title;
	struct wl_listener fullscreen;
#ifdef XWAYLAND
	struct wl_listener activate;
	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener configure;
	struct wl_listener set_hints;
	struct wl_listener set_geometry;
	struct wl_listener commmitx11;
#endif
	uint32_t bw;
	uint32_t tags, oldtags, mini_restore_tag;
	bool dirty;
	uint32_t configure_serial;
	struct wlr_foreign_toplevel_handle_v1 *foreign_toplevel;
	int32_t isfloating, isurgent, isfullscreen, isfakefullscreen,
		need_float_size_reduce, isminimized, isoverlay, isnosizehint,
		ignore_maximize, ignore_minimize, indleinhibit_when_focus;
	int32_t ismaximizescreen;
	int32_t overview_backup_bw;
	int32_t fullscreen_backup_x, fullscreen_backup_y, fullscreen_backup_w,
		fullscreen_backup_h;
	int32_t overview_isfullscreenbak, overview_ismaximizescreenbak,
		overview_isfloatingbak;

	struct wlr_xdg_toplevel_decoration_v1 *decoration;
	struct wl_listener foreign_activate_request;
	struct wl_listener foreign_fullscreen_request;
	struct wl_listener foreign_close_request;
	struct wl_listener foreign_destroy;
	struct wl_listener foreign_minimize_request;
	struct wl_listener foreign_maximize_request;
	struct wl_listener set_decoration_mode;
	struct wl_listener destroy_decoration;

	const char *animation_type_open;
	const char *animation_type_close;
	int32_t is_in_scratchpad;
	int32_t iscustomsize;
	int32_t iscustompos;
	int32_t is_scratchpad_show;
	int32_t isglobal;
	int32_t isnoborder;
	int32_t isnoshadow;
	int32_t isnoradius;
	int32_t isnoanimation;
	int32_t isopensilent;
	int32_t istagsilent;
	int32_t iskilling;
	int32_t istagswitching;
	int32_t isnamedscratchpad;
	bool is_pending_open_animation;
	bool is_restoring_from_ov;
	bool ov_anim;
	/* True while this client is temporarily unminimized for overview display.
	   Re-minimized on overview exit unless the user clicked it (selmon->sel). */
	bool ov_was_minimized;
	/* Monotonic ms timestamp of last focus activity. Used by the idle
	   hibernation scanner to decide when to send xdg suspended. */
	uint32_t last_active_ms;
	float scroller_proportion;
	float stack_proportion;
	float old_stack_proportion;
	bool need_output_flush;
	bool focus_opacity_dirty;
	bool foreign_activated_state;
	int8_t fullscreen_sent;
	int8_t maximized_sent;
	int8_t suspended_sent;
	uint32_t tiled_edges_sent;
	struct dwl_animation animation;
	struct dwl_opacity_animation opacity_animation;
	int32_t isterm, noswallow;
	int32_t allow_csd;
	int32_t force_fakemaximize;
	int32_t force_tiled_state;
	pid_t pid;
	Client *swallowing, *swallowedby;
	bool is_clip_to_hide;
	bool drag_to_tile;
	bool scratchpad_switching_mon;
	bool fake_no_border;
	int32_t nofocus;
	int32_t nofadein;
	int32_t nofadeout;
	int32_t no_force_center;
	int32_t isunglobal;
	float focused_opacity;
	float unfocused_opacity;
	/* Architecture brief §9: render tier (0=FOCUS, 1=VISIBLE, 2=OCCLUDED, 3=HIDDEN).
	   Recomputed on focus/layout change; checked in animation tick. */
	uint8_t render_tier;
	uint32_t tier_last_anim_ms;
	char oldmonname[128];
	int32_t noblur;
	double master_mfact_per, master_inner_per, stack_inner_per;
	double old_master_mfact_per, old_master_inner_per, old_stack_inner_per;
	double old_scroller_pproportion;
	bool ismaster;
	bool old_ismaster;
	bool cursor_in_upper_half, cursor_in_left_half;
	bool isleftstack;
	int32_t tearing_hint;
	int32_t force_tearing;
	/* content-type-v1 hint: WP_CONTENT_TYPE_V1_TYPE_{NONE,PHOTO,VIDEO,GAME}.
	   Refreshed on surface commit; drives auto-tearing and animation skip. */
	int32_t content_type;
	int32_t allow_shortcuts_inhibit;
	float scroller_proportion_single;
	bool isfocusing;
	bool enable_drop_area_draw;
	int32_t drop_direction;
	struct wlr_box drag_tile_float_backup_geom;
	float grid_col_per;
	float grid_row_per;
	float old_grid_col_per;
	float old_grid_row_per;
	int32_t grid_col_idx;
	int32_t grid_row_idx;
};

typedef struct {
	struct wl_list link;
	struct wl_resource *resource;
	Monitor *mon;
} DwlIpcOutput;

typedef struct {
	uint32_t mod;
	xkb_keysym_t keysym;
	int32_t (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	struct wlr_keyboard_group *wlr_group;

	int32_t nsyms;
	const xkb_keysym_t *keysyms;
	uint32_t mods;
	uint32_t keycode;
	struct wl_event_source *key_repeat_source;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
} KeyboardGroup;

typedef struct {
	struct wlr_keyboard_shortcuts_inhibitor_v1 *inhibitor;
	struct wl_listener destroy;
	struct wl_list link;
} KeyboardShortcutsInhibitor;

typedef struct {
	
	uint32_t type;
	struct wlr_box geom, current, pending, animainit_geom;
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_tree *popups;
	struct wlr_scene_shadow *shadow;
	struct wlr_scene_layer_surface_v1 *scene_layer;
	struct wl_list link;
	struct wl_list fadeout_link;
	int32_t mapped;
	struct wlr_layer_surface_v1 *layer_surface;

	struct wl_listener destroy;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;

	struct dwl_animation animation;
	bool dirty;
	bool blur_state_applied;
	int32_t noblur;
	int32_t noanim;
	int32_t noshadow;
	char *animation_type_open;
	char *animation_type_close;
	bool need_output_flush;
	bool being_unmapped;
} LayerSurface;

typedef struct {
	struct wlr_xdg_popup *wlr_popup;
	struct wl_listener destroy;
	struct wl_listener commit;
	struct wl_listener reposition;
} Popup;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
	const char *name;
	uint32_t id;
} Layout;

struct Monitor {
	struct wl_list link;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wlr_output_state pending;
	struct wl_listener frame;
	struct wl_listener destroy;
	struct wl_listener request_state;
	struct wl_listener destroy_lock_surface;
	struct wlr_session_lock_surface_v1 *lock_surface;
	struct wl_event_source *skip_frame_timeout;
	struct wl_event_source *battery_frame_throttle;
	uint32_t last_anim_schedule_ms;
	/* Late input latching: deferred render timer + EMA of render duration (us)
	   used to start the render just before vblank. render_pending guards it. */
	struct wl_event_source *render_deadline;
	uint32_t render_ema_us;
	bool render_pending;
	struct wlr_box m;
	struct wlr_box w;
	struct wl_list layers[4];
	uint32_t seltags;
	uint32_t tagset[2];
	bool skiping_frame;
	uint32_t resizing_count_pending;
	uint32_t resizing_count_current;

	struct wl_list dwl_ipc_outputs;
	int32_t gappih;
	int32_t gappiv;
	int32_t gappoh;
	int32_t gappov;
	Pertag *pertag;
	uint32_t ovbk_current_tagset;
	uint32_t ovbk_prev_tagset;
	Client *sel, *prevsel;
	int32_t isoverview;
	struct wlr_scene_rect *ov_dim;
	float ov_dim_cur;
	uint32_t ov_dim_last_ms;
	int32_t is_in_hotarea;
	int32_t asleep;
	uint32_t visible_clients;
	uint32_t visible_tiling_clients;
	uint32_t visible_scroll_tiling_clients;
	struct wlr_scene_optimized_blur *blur;
	char last_surface_ws_name[256];
	struct wlr_ext_workspace_group_handle_v1 *ext_group;
	bool iscleanuping;
};

typedef struct {
	struct wlr_pointer_constraint_v1 *constraint;
	struct wl_listener destroy;
} PointerConstraint;

typedef struct {
	struct wlr_scene_tree *scene;

	struct wlr_session_lock_v1 *lock;
	struct wl_listener new_surface;
	struct wl_listener unlock;
	struct wl_listener destroy;
} SessionLock;

typedef struct DwindleNode DwindleNode;
struct DwindleNode {
	bool is_split;
	bool split_h;
	bool split_locked;
	bool custom_leaf_split_h;
	float ratio;
	float drag_init_ratio;
	int32_t container_x;
	int32_t container_y;
	int32_t container_w;
	int32_t container_h;
	DwindleNode *parent;
	DwindleNode *first;
	DwindleNode *second;
	Client *client;
};

struct ScrollerStackNode {
	Client *client;
	float scroller_proportion;
	float stack_proportion;
	float scroller_proportion_single;

	struct ScrollerStackNode *next_in_stack;
	struct ScrollerStackNode *prev_in_stack;
	struct ScrollerStackNode *all_next;
};

struct TagScrollerState {
	struct ScrollerStackNode *all_first;
	int count;
};

static void applybounds(
	Client *c,
	struct wlr_box *bbox);
static void applyrules(Client *c);
static void arrange(Monitor *m, bool want_animation,
					bool from_view);
static void arrangelayer(Monitor *m, struct wl_list *list,
						 struct wlr_box *usable_area, int32_t exclusive);
static void arrangelayers(Monitor *m);
static void handle_print_status(struct wl_listener *listener, void *data);
static void axisnotify(struct wl_listener *listener,
					   void *data);
static void buttonpress(struct wl_listener *listener,
						void *data);
static int32_t ongesture(struct wlr_pointer_swipe_end_event *event);
static void swipe_begin(struct wl_listener *listener, void *data);
static void swipe_update(struct wl_listener *listener, void *data);
static void swipe_end(struct wl_listener *listener, void *data);
static void pinch_begin(struct wl_listener *listener, void *data);
static void pinch_update(struct wl_listener *listener, void *data);
static void pinch_end(struct wl_listener *listener, void *data);
static void hold_begin(struct wl_listener *listener, void *data);
static void hold_end(struct wl_listener *listener, void *data);
static void checkidleinhibitor(struct wlr_surface *exclude);
static void cleanup(void);
static void cleanupmon(struct wl_listener *listener, void *data);
static void closemon(Monitor *m);
static void cleanuplisteners(void);
static void toggle_hotarea(int32_t x_root, int32_t y_root);
static void maplayersurfacenotify(struct wl_listener *listener, void *data);
static void commitlayersurfacenotify(struct wl_listener *listener, void *data);
static void commitnotify(struct wl_listener *listener, void *data);
static void createdecoration(struct wl_listener *listener, void *data);
static void createidleinhibitor(struct wl_listener *listener, void *data);
static void createkeyboard(struct wlr_keyboard *keyboard);
static void requestmonstate(struct wl_listener *listener, void *data);
static void createlayersurface(struct wl_listener *listener, void *data);
static void createlocksurface(struct wl_listener *listener, void *data);
static void createmon(struct wl_listener *listener, void *data);
static void createnotify(struct wl_listener *listener, void *data);
static void createpointer(struct wlr_pointer *pointer);
static void configure_pointer(struct libinput_device *device);
static void destroyinputdevice(struct wl_listener *listener, void *data);
static void createswitch(struct wlr_switch *switch_device);
static void switch_toggle(struct wl_listener *listener, void *data);
static void createpointerconstraint(struct wl_listener *listener, void *data);
static void cursorconstrain(struct wlr_pointer_constraint_v1 *constraint);
static void commitpopup(struct wl_listener *listener, void *data);
static void createpopup(struct wl_listener *listener, void *data);
static void cursorframe(struct wl_listener *listener, void *data);
static void cursorwarptohint(void);
static void destroydecoration(struct wl_listener *listener, void *data);
static void destroydragicon(struct wl_listener *listener, void *data);
static void destroyidleinhibitor(struct wl_listener *listener, void *data);
static void destroylayernodenotify(struct wl_listener *listener, void *data);
static void destroylock(SessionLock *lock, int32_t unlocked);
static void destroylocksurface(struct wl_listener *listener, void *data);
static void destroynotify(struct wl_listener *listener, void *data);
static void destroypointerconstraint(struct wl_listener *listener, void *data);
static void destroysessionlock(struct wl_listener *listener, void *data);
static void destroykeyboardgroup(struct wl_listener *listener, void *data);
static Monitor *dirtomon(enum wlr_direction dir);
static void setcursorshape(struct wl_listener *listener, void *data);

static void focusclient(Client *c, int32_t lift);

static void setborder_color(Client *c);
static Client *focustop(Monitor *m);
static void fullscreennotify(struct wl_listener *listener, void *data);
static void gpureset(struct wl_listener *listener, void *data);

static int32_t keyrepeat(void *data);

static void inputdevice(struct wl_listener *listener, void *data);
static int32_t keybinding(uint32_t state, bool locked, uint32_t mods,
						  xkb_keysym_t sym, uint32_t keycode);
static void keypress(struct wl_listener *listener, void *data);
static void keypressmod(struct wl_listener *listener, void *data);
static bool keypressglobal(struct wlr_surface *last_surface,
						   struct wlr_keyboard *keyboard,
						   struct wlr_keyboard_key_event *event, uint32_t mods,
						   xkb_keysym_t keysym, uint32_t keycode);
static void locksession(struct wl_listener *listener, void *data);
static void mapnotify(struct wl_listener *listener, void *data);
static void maximizenotify(struct wl_listener *listener, void *data);
static void minimizenotify(struct wl_listener *listener, void *data);
static void motionabsolute(struct wl_listener *listener, void *data);
static void motionnotify(uint32_t time, struct wlr_input_device *device,
						 double sx, double sy, double sx_unaccel,
						 double sy_unaccel);
static void motionrelative(struct wl_listener *listener, void *data);

static void reset_foreign_tolevel(Client *c, Monitor *oldmon, Monitor *newmon);
static void exchange_two_client(Client *c1, Client *c2);
static void outputmgrapply(struct wl_listener *listener, void *data);
static void outputmgrapplyortest(struct wlr_output_configuration_v1 *config,
								 int32_t test);
static void outputmgrtest(struct wl_listener *listener, void *data);
static void pointerfocus(Client *c, struct wlr_surface *surface, double sx,
						 double sy, uint32_t time);
static void printstatus(void);
static void quitsignal(int32_t signo);
static void powermgrsetmode(struct wl_listener *listener, void *data);
static void rendermon(struct wl_listener *listener, void *data);
static void requestdecorationmode(struct wl_listener *listener, void *data);
static void requestdrmlease(struct wl_listener *listener, void *data);
static void requeststartdrag(struct wl_listener *listener, void *data);
static void resize(Client *c, struct wlr_box geo, int32_t interact);
static void run(char *startup_cmd);
static void setcursor(struct wl_listener *listener, void *data);
static void setfloating(Client *c, int32_t floating);
static void setfakefullscreen(Client *c, int32_t fakefullscreen);
static void setfullscreen(Client *c, int32_t fullscreen);
static void setmaximizescreen(Client *c, int32_t maximizescreen);
static void reset_maximizescreen_size(Client *c);
static void setgaps(int32_t oh, int32_t ov, int32_t ih, int32_t iv);

static void setmon(Client *c, Monitor *m, uint32_t newtags, bool focus);
static void setpsel(struct wl_listener *listener, void *data);
static void setsel(struct wl_listener *listener, void *data);
static void setup(void);
static void startdrag(struct wl_listener *listener, void *data);

static void unlocksession(struct wl_listener *listener, void *data);
static void unmaplayersurfacenotify(struct wl_listener *listener, void *data);
static void unmapnotify(struct wl_listener *listener, void *data);
static void updatemons(struct wl_listener *listener, void *data);
static void updatetitle(struct wl_listener *listener, void *data);
static void urgent(struct wl_listener *listener, void *data);
static void view(const Arg *arg, bool want_animation);

static void handlesig(int32_t signo);
static int32_t setup_sigchld_signalfd(void);
static int32_t setup_sigusr1_signalfd(void);

/* Set to true once setup() has finished wiring monitors, scene tree and
   listeners. Until then SIGUSR1-triggered reload_config would deref a
   half-initialised compositor (e.g. arrange() through selmon=NULL),
   which crashes and ends up in an exec-once -> matugen -> SIGUSR1
   reboot loop when a theme generator races startup. */
static volatile bool reload_ready = false;
static void
handle_keyboard_shortcuts_inhibit_new_inhibitor(struct wl_listener *listener,
												void *data);
static void virtualkeyboard(struct wl_listener *listener, void *data);
static void virtualpointer(struct wl_listener *listener, void *data);
static void warp_cursor(const Client *c);
static Monitor *xytomon(double x, double y);
static void xytonode(double x, double y, struct wlr_surface **psurface,
					 Client **pc, LayerSurface **pl, double *nx, double *ny);
static void clear_fullscreen_flag(Client *c);
static pid_t getparentprocess(pid_t p);
static int32_t isdescprocess(pid_t p, pid_t c);
static Client *termforwin(Client *w);
static void swallow(Client *c, Client *w);

static void warp_cursor_to_selmon(Monitor *m);
uint32_t want_restore_fullscreen(Client *target_client);
static void overview_restore(Client *c, const Arg *arg);
static void overview_backup(Client *c);
static void set_minimized(Client *c);

static void show_scratchpad(Client *c);
static void show_hide_client(Client *c);
static void tag_client(const Arg *arg, Client *target_client);

static struct wlr_box setclient_coordinate_center(Client *c, Monitor *m,
												  struct wlr_box geom,
												  int32_t offsetx,
												  int32_t offsety);
static uint32_t get_tags_first_tag(uint32_t tags);

static struct wlr_output_mode *
get_nearest_output_mode(struct wlr_output *output, int32_t width,
						int32_t height, float refresh);

static void client_commit(Client *c);
static void layer_commit(LayerSurface *l);
static void apply_border(Client *c);
static void client_set_opacity(Client *c, double opacity);
static void init_baked_points(void);
static void scene_buffer_apply_opacity(struct wlr_scene_buffer *buffer,
									   int32_t sx, int32_t sy, void *data);

static Client *direction_select(const Arg *arg);
static void view_in_mon(const Arg *arg, bool want_animation, Monitor *m,
						bool changefocus);

static void buffer_set_effect(Client *c, BufferData buffer_data);
static void snap_scene_buffer_apply_effect(struct wlr_scene_buffer *buffer,
										   int32_t sx, int32_t sy, void *data);
static void client_set_pending_state(Client *c);
static void layer_set_pending_state(LayerSurface *l);
static void set_rect_size(struct wlr_scene_rect *rect, int32_t width,
						  int32_t height);
static Client *center_tiled_select(Monitor *m);
static void handlecursoractivity(void);
static int32_t hidecursor(void *data);
static bool check_hit_no_border(Client *c);
static void reset_keyboard_layout(void);
static void client_update_oldmonname_record(Client *c, Monitor *m);
static void pending_kill_client(Client *c);
static uint32_t get_tags_first_tag_num(uint32_t source_tags);
static void init_fadeout_layers(LayerSurface *l);
static void layer_actual_size(LayerSurface *l, int32_t *width, int32_t *height);
static void get_layer_target_geometry(LayerSurface *l,
									  struct wlr_box *target_box);
static void scene_buffer_apply_effect(struct wlr_scene_buffer *buffer,
									  int32_t sx, int32_t sy, void *data);
static double find_animation_curve_at(double t, int32_t type);

static enum corner_location set_client_corner_location(Client *c);
static struct wlr_scene_tree *
wlr_scene_tree_snapshot(struct wlr_scene_node *node,
						struct wlr_scene_tree *parent);
static bool is_scroller_layout(Monitor *m);
static bool is_centertile_layout(Monitor *m);
static void create_output(struct wlr_backend *backend, void *data);
static void get_layout_abbr(char *abbr, const char *full_name);
static void apply_named_scratchpad(Client *target_client);
static Client *get_client_by_id_or_title(const char *arg_id,
										 const char *arg_title);
static bool switch_scratchpad_client_state(Client *c);
static bool check_trackpad_disabled(struct wlr_pointer *pointer);
static uint32_t get_tag_status(uint32_t tag, Monitor *m);
static void enable_adaptive_sync(Monitor *m, struct wlr_output_state *state);
static Client *get_next_stack_client(Client *c, bool reverse);
static void set_float_malposition(Client *tc);
static void set_size_per(Monitor *m, Client *c);
static void resize_tile_client(Client *grabc, bool isdrag, int32_t offsetx,
							   int32_t offsety, uint32_t time);
static void refresh_monitors_workspaces_status(Monitor *m);
static void init_client_properties(Client *c);
static float *get_border_color(Client *c);
static void clear_fullscreen_and_maximized_state(Monitor *m);
static void request_fresh_all_monitors(void);
static bool detect_on_battery(void);
static int battery_poll_callback(void *data);
static int battery_frame_throttle_callback(void *data);
static void do_rendermon(Monitor *m);
static int render_deadline_callback(void *data);
static Client *find_client_by_direction(Client *tc, const Arg *arg,
										bool findfloating, bool ignore_align);
static void exit_scroller_stack(Client *c);
static Client *scroll_get_stack_head_client(Client *c);
static bool client_only_in_one_tag(Client *c);
static Client *get_focused_stack_client(Client *sc);
static bool client_is_in_same_stack(Client *sc, Client *tc, Client *fc);
static void monitor_stop_skip_frame_timer(Monitor *m);
static int monitor_skip_frame_timeout_callback(void *data);
static Monitor *get_monitor_nearest_to(int32_t lx, int32_t ly);
static bool match_monitor_spec(char *spec, Monitor *m);
static void last_cursor_surface_destroy(struct wl_listener *listener,
										void *data);
static int32_t keep_idle_inhibit(void *data);
static void check_keep_idle_inhibit(Client *c);
static void setup_idle_timers(void);
static void reset_idle_timers(void);
static void destroy_idle_timers(void);
static int32_t idle_timer_callback(void *data);
static void pre_caculate_before_arrange(Monitor *m, bool want_animation,
										bool from_view, bool only_caculate);
static void client_pending_fullscreen_state(Client *c, int32_t isfullscreen);
static void client_pending_maximized_state(Client *c, int32_t ismaximized);
static void client_pending_minimized_state(Client *c, int32_t isminimized);
static void scroller_insert_stack(Client *c, Client *target_client,
								  bool insert_before);
static void dwindle_move_client(DwindleNode **root, Client *c, Client *target,
								float ratio, int32_t dir);
static void dwindle_resize_client_step(Monitor *m, Client *c, int32_t dx,
									   int32_t dy);
static void dwindle_resize_client(Monitor *m, Client *c);

static struct TagScrollerState *ensure_scroller_state(Monitor *m, uint32_t tag);
static struct ScrollerStackNode *find_scroller_node(struct TagScrollerState *st,
													Client *c);
static void sync_scroller_state_to_clients(Monitor *m, uint32_t tag);
static void scroller_node_remove(struct TagScrollerState *st,
								 struct ScrollerStackNode *target);
static struct ScrollerStackNode *
scroller_node_create(struct TagScrollerState *st, Client *c);
static void update_scroller_state(Monitor *m);
Client *scroll_get_stack_tail_client(Client *c);
static DwindleNode *dwindle_find_leaf(DwindleNode *node, Client *c);

#include "data/static_keymap.h"
#include "dispatch/bind_declare.h"
#include "layout/layout.h"

static const char broken[] = "broken";
static pid_t child_pid = -1;
static int32_t locked;
static uint32_t locked_mods = 0;
static void *exclusive_focus;
static struct wl_display *dpy;
static struct wl_event_loop *event_loop;
static struct wlr_backend *backend;
static struct wlr_backend *headless_backend;
static struct wlr_scene *scene;
static struct wlr_scene_tree *layers[NUM_LAYERS];
static struct wlr_renderer *drw;
static struct wlr_allocator *alloc;
static struct wlr_compositor *compositor;

static struct wlr_xdg_shell *xdg_shell;
static struct wlr_xdg_activation_v1 *activation;
static struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
static struct wl_list clients;
static struct wl_list fstack;
static struct wl_list fadeout_clients;
static struct wl_list fadeout_layers;
static struct wlr_idle_notifier_v1 *idle_notifier;
static struct wlr_idle_inhibit_manager_v1 *idle_inhibit_mgr;
static bool idle_inhibited = false;
static struct wlr_layer_shell_v1 *layer_shell;
static struct wlr_output_manager_v1 *output_mgr;
static struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_mgr;
static struct wlr_screencopy_manager_v1 *screencopy_mgr;
static struct wlr_keyboard_shortcuts_inhibit_manager_v1
	*keyboard_shortcuts_inhibit;
static struct wlr_virtual_pointer_manager_v1 *virtual_pointer_mgr;
static struct wlr_output_power_manager_v1 *power_mgr;
static struct wlr_pointer_gestures_v1 *pointer_gestures;
static struct wlr_drm_lease_v1_manager *drm_lease_manager;
struct lemon_print_status_manager *print_status_manager;

static struct wlr_cursor *cursor;
static struct wlr_xcursor_manager *cursor_mgr;
static struct wlr_session *session;

static struct wlr_scene_rect *root_bg;
static struct wlr_session_lock_manager_v1 *session_lock_mgr;
static struct wlr_scene_rect *locked_bg;
static struct wlr_session_lock_v1 *cur_lock;
static const int32_t layermap[] = {LyrBg, LyrBottom, LyrTop, LyrOverlay};
static struct wlr_scene_tree *drag_icon;
static struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr;
static struct wlr_pointer_constraints_v1 *pointer_constraints;
static struct wlr_relative_pointer_manager_v1 *relative_pointer_mgr;
static struct wlr_pointer_constraint_v1 *active_constraint;

static struct wlr_seat *seat;
static KeyboardGroup *kb_group;
static struct wl_list inputdevices;
static struct wl_list keyboard_shortcut_inhibitors;
static uint32_t cursor_mode;
static Client *grabc, *dropc;
/* Pointer velocity (px/s, EMA) tracked while dragging, handed off to the
   window's spring on release for the momentum "throw" feel (animation_momentum). */
static double drag_vel_x, drag_vel_y, drag_last_x, drag_last_y;
static uint32_t drag_last_ms;
static int32_t rzcorner;
static int32_t grabcx, grabcy;
static int32_t drag_begin_cursorx, drag_begin_cursory;
static bool start_drag_window = false;
static int32_t last_apply_drap_time = 0;

static struct wlr_output_layout *output_layout;
static struct wlr_box sgeom;
static struct wl_list mons;
static Monitor *selmon;

static int32_t enablegaps = 1;
static int32_t axis_apply_time = 0;
static int32_t axis_apply_dir = 0;
static int32_t scroller_focus_lock = 0;

static uint32_t swipe_fingers = 0;
static double swipe_dx = 0;
static double swipe_dy = 0;

bool render_border = true;

uint32_t chvt_backup_tag = 0;
bool allow_frame_scheduling = true;
bool on_battery = false;
static struct wl_event_source *battery_poll_source;
#define BATTERY_ANIM_INTERVAL_MS 16u
char chvt_backup_selmon[32] = {0};

struct dvec2 *baked_points_move;
struct dvec2 *baked_points_open;
struct dvec2 *baked_points_tag;
struct dvec2 *baked_points_close;
struct dvec2 *baked_points_focus;
struct dvec2 *baked_points_opafadein;
struct dvec2 *baked_points_opafadeout;

static struct wl_event_source *hide_cursor_source;
static struct wl_event_source *keep_idle_inhibit_source;
static bool cursor_hidden = false;
static bool tag_combo = false;
static const char *cli_config_path = NULL;
static bool cli_debug_log = false;
static KeyMode keymode = {
	.mode = {'d', 'e', 'f', 'a', 'u', 'l', 't', '\0'},
	.isdefault = true,
};

static char *env_vars[] = {"DISPLAY",
						   "WAYLAND_DISPLAY",
						   "XDG_CURRENT_DESKTOP",
						   "XDG_SESSION_TYPE",
						   "XCURSOR_THEME",
						   "XCURSOR_SIZE",
						   NULL};
static struct {
	enum wp_cursor_shape_device_v1_shape shape;
	struct wlr_surface *surface;
	int32_t hotspot_x;
	int32_t hotspot_y;
} last_cursor;

#include "client/client.h"
#include "config/preset.h"
struct Pertag {
	uint32_t curtag, prevtag;
	int32_t nmasters[LENGTH(tags) + 1];
	float mfacts[LENGTH(tags) + 1];
	int32_t no_hide[LENGTH(tags) + 1];
	int32_t no_render_border[LENGTH(tags) + 1];
	int32_t open_as_floating[LENGTH(tags) + 1];
	struct DwindleNode *dwindle_root[LENGTH(tags) + 1];
	const Layout *ltidxs[LENGTH(tags) + 1];
	struct TagScrollerState *scroller_state[LENGTH(tags) + 1];
};
#include "config/parse_config.h"

static struct wl_signal lemon_print_status;

static struct wl_listener print_status_listener = {.notify =
													   handle_print_status};
static struct wl_listener cursor_axis = {.notify = axisnotify};
static struct wl_listener cursor_button = {.notify = buttonpress};
static struct wl_listener cursor_frame = {.notify = cursorframe};
static struct wl_listener cursor_motion = {.notify = motionrelative};
static struct wl_listener cursor_motion_absolute = {.notify = motionabsolute};
static struct wl_listener gpu_reset = {.notify = gpureset};
static struct wl_listener layout_change = {.notify = updatemons};
static struct wl_listener new_idle_inhibitor = {.notify = createidleinhibitor};
static struct wl_listener new_input_device = {.notify = inputdevice};
static struct wl_listener new_virtual_keyboard = {.notify = virtualkeyboard};
static struct wl_listener new_virtual_pointer = {.notify = virtualpointer};
static struct wl_listener new_pointer_constraint = {
	.notify = createpointerconstraint};
static struct wl_listener new_output = {.notify = createmon};
static struct wl_listener new_xdg_toplevel = {.notify = createnotify};
static struct wl_listener new_xdg_popup = {.notify = createpopup};
static struct wl_listener new_xdg_decoration = {.notify = createdecoration};
static struct wl_listener new_layer_surface = {.notify = createlayersurface};
static struct wl_listener output_mgr_apply = {.notify = outputmgrapply};
static struct wl_listener output_mgr_test = {.notify = outputmgrtest};
static struct wl_listener output_power_mgr_set_mode = {.notify =
														   powermgrsetmode};
static struct wl_listener request_activate = {.notify = urgent};
static struct wl_listener request_cursor = {.notify = setcursor};
static struct wl_listener request_set_psel = {.notify = setpsel};
static struct wl_listener request_set_sel = {.notify = setsel};
static struct wl_listener request_set_cursor_shape = {.notify = setcursorshape};
static struct wl_listener request_start_drag = {.notify = requeststartdrag};
static struct wl_listener start_drag = {.notify = startdrag};
static struct wl_listener new_session_lock = {.notify = locksession};
static struct wl_listener drm_lease_request = {.notify = requestdrmlease};
static struct wl_listener keyboard_shortcuts_inhibit_new_inhibitor = {
	.notify = handle_keyboard_shortcuts_inhibit_new_inhibitor};
static struct wl_listener last_cursor_surface_destroy_listener = {
	.notify = last_cursor_surface_destroy};

#ifdef XWAYLAND
static void fix_xwayland_unmanaged_coordinate(Client *c);
static int32_t synckeymap(void *data);
static void activatex11(struct wl_listener *listener, void *data);
static void configurex11(struct wl_listener *listener, void *data);
static void createnotifyx11(struct wl_listener *listener, void *data);
static void dissociatex11(struct wl_listener *listener, void *data);
static void commitx11(struct wl_listener *listener, void *data);
static void associatex11(struct wl_listener *listener, void *data);
static void sethints(struct wl_listener *listener, void *data);
static void xwaylandready(struct wl_listener *listener, void *data);
static void setgeometrynotify(struct wl_listener *listener, void *data);
static struct wl_listener new_xwayland_surface = {.notify = createnotifyx11};
static struct wl_listener xwayland_ready = {.notify = xwaylandready};
static struct wlr_xwayland *xwayland;
static struct wl_event_source *sync_keymap;
#endif

void request_fresh_all_monitors(void);
void request_fresh_for_client(Client *c);
static void recompute_render_tiers(void);

#include "animation/spring.h"
#include "animation/client.h"
#include "animation/common.h"
#include "animation/layer.h"
#include "animation/tag.h"
#include "clipboard/clipboard.h"
#include "dispatch/cycler.h"
#include "dispatch/bind_define.h"
#include "ext-protocol/all.h"
#include "fetch/fetch.h"
#include "layout/arrange.h"
#include "layout/dwindle.h"
#include "layout/horizontal.h"
#include "layout/scroll.h"
#include "layout/vertical.h"

/* Move a client to monitor m, preserving tags and recentering floating clients. */
void client_change_mon(Client *c, Monitor *m) {
	setmon(c, m, c->tags, true);
	if (c->isfloating) {
		c->float_geom = c->geom =
			setclient_coordinate_center(c, c->mon, c->geom, 0, 0);
	}
}

/* Clamp client geometry so it stays within the given bounding box. */
void applybounds(Client *c, struct wlr_box *bbox) {
	
	c->geom.width = MAX(1 + 2 * (int32_t)c->bw, c->geom.width);
	c->geom.height = MAX(1 + 2 * (int32_t)c->bw, c->geom.height);

	if (c->geom.x >= bbox->x + bbox->width)
		c->geom.x = bbox->x + bbox->width - c->geom.width;
	if (c->geom.y >= bbox->y + bbox->height)
		c->geom.y = bbox->y + bbox->height - c->geom.height;
	if (c->geom.x + c->geom.width <= bbox->x)
		c->geom.x = bbox->x;
	if (c->geom.y + c->geom.height <= bbox->y)
		c->geom.y = bbox->y;
}

/* Clear fullscreen and maximized flags on every visible client of monitor m. */
void clear_fullscreen_and_maximized_state(Monitor *m) {
	Client *fc = NULL;
	wl_list_for_each(fc, &clients, link) {
		if (fc && VISIBLEON(fc, m) && ISFULLSCREEN(fc)) {
			clear_fullscreen_flag(fc);
		}
	}
}

/* Exit fullscreen or maximized state for client c, except in scroller tiled layouts. */
void clear_fullscreen_flag(Client *c) {

	if ((c->mon->pertag->ltidxs[get_tags_first_tag_num(c->tags)]->id ==
			 SCROLLER ||
		 c->mon->pertag->ltidxs[get_tags_first_tag_num(c->tags)]->id ==
			 VERTICAL_SCROLLER) &&
		!c->isfloating) {
		return;
	}

	if (c->isfullscreen) {
		setfullscreen(c, false);
	}

	if (c->ismaximizescreen) {
		setmaximizescreen(c, 0);
	}
}

/* Set client's pending fullscreen flag and notify foreign-toplevel listeners. */
void client_pending_fullscreen_state(Client *c, int32_t isfullscreen) {
	c->isfullscreen = isfullscreen;

	if (c->foreign_toplevel && !c->iskilling)
		wlr_foreign_toplevel_handle_v1_set_fullscreen(c->foreign_toplevel,
													  isfullscreen);
}

/* Set client's pending maximized flag and notify foreign-toplevel listeners. */
void client_pending_maximized_state(Client *c, int32_t ismaximized) {
	c->ismaximizescreen = ismaximized;
	if (c->foreign_toplevel && !c->iskilling)
		wlr_foreign_toplevel_handle_v1_set_maximized(c->foreign_toplevel,
													 ismaximized);
}

/* Set client's pending minimized flag and notify foreign-toplevel listeners. */
void client_pending_minimized_state(Client *c, int32_t isminimized) {
	c->isminimized = isminimized;
	if (c->foreign_toplevel && !c->iskilling)
		wlr_foreign_toplevel_handle_v1_set_minimized(c->foreign_toplevel,
													 isminimized);
}

/* Reveal a scratchpad client: force float, size and center it, then show on current tag. */
void show_scratchpad(Client *c) {
	c->is_scratchpad_show = 1;
	if (c->isfullscreen || c->ismaximizescreen) {
		client_pending_fullscreen_state(c, 0);
		client_pending_maximized_state(c, 0);
		c->bw = c->isnoborder ? 0 : config.borderpx;
	}

	if (!c->isfloating) {
		setfloating(c, 1);
		c->geom.width = c->iscustomsize
							? c->float_geom.width
							: c->mon->w.width * config.scratchpad_width_ratio;
		c->geom.height =
			c->iscustomsize ? c->float_geom.height
							: c->mon->w.height * config.scratchpad_height_ratio;
		
		c->float_geom = c->geom = c->animainit_geom = c->animation.current =
			setclient_coordinate_center(c, c->mon, c->geom, 0, 0);
		c->iscustomsize = 1;
		resize(c, c->geom, 0);
	}

	c->oldtags = c->mon->tagset[c->mon->seltags];
	wl_list_remove(&c->link);
	wl_list_insert(clients.prev->next, &c->link);
	show_hide_client(c);
	setborder_color(c);
}

/* Record the current monitor's name on the client for later re-placement after a hot-plug. */
void client_update_oldmonname_record(Client *c, Monitor *m) {
	if (!c || c->iskilling || !client_surface(c)->mapped)
		return;
	memset(c->oldmonname, 0, sizeof(c->oldmonname));
	strncpy(c->oldmonname, m->wlr_output->name, sizeof(c->oldmonname) - 1);
	c->oldmonname[sizeof(c->oldmonname) - 1] = '\0';
}

/* Replace swallowed client w with new client c, transferring geometry, tags, and tree state. */
void swallow(Client *c, Client *w) {
	c->bw = w->bw;
	c->isfloating = w->isfloating;
	c->isurgent = w->isurgent;
	c->is_in_scratchpad = w->is_in_scratchpad;
	c->is_scratchpad_show = w->is_scratchpad_show;
	c->tags = w->tags;
	c->geom = w->geom;
	c->float_geom = w->float_geom;
	c->stack_inner_per = w->stack_inner_per;
	c->master_inner_per = w->master_inner_per;
	c->master_mfact_per = w->master_mfact_per;
	c->scroller_proportion = w->scroller_proportion;
	c->isglobal = w->isglobal;

	c->stack_proportion = w->stack_proportion;

	wl_list_insert(&w->link, &c->link);
	wl_list_insert(&w->flink, &c->flink);

	if (w->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_output_leave(w->foreign_toplevel,
													w->mon->wlr_output);
		wlr_foreign_toplevel_handle_v1_destroy(w->foreign_toplevel);
		w->foreign_toplevel = NULL;
	}

	wlr_scene_node_set_enabled(&w->scene->node, false);
	wlr_scene_node_set_enabled(&c->scene->node, true);
	wlr_scene_node_set_enabled(&c->scene_surface->node, true);

	if (!c->foreign_toplevel && c->mon)
		add_foreign_toplevel(c);
	else if (c->foreign_toplevel && c->mon) {
		wlr_foreign_toplevel_handle_v1_output_enter(c->foreign_toplevel,
													c->mon->wlr_output);
	}

	client_pending_fullscreen_state(c, w->isfullscreen);
	client_pending_maximized_state(c, w->ismaximizescreen);
	client_pending_minimized_state(c, w->isminimized);

	if (!w->mon)
		return;

	const Layout *layout = w->mon->pertag->ltidxs[w->mon->pertag->curtag];

	if (layout->id == DWINDLE || layout->id == SCROLLER ||
		layout->id == VERTICAL_SCROLLER) {

		for (uint32_t t = 0; t < LENGTH(tags) + 1; t++) {
			
			if (layout->id == DWINDLE) {

				DwindleNode **root = &w->mon->pertag->dwindle_root[t];
				dwindle_remove(root, c);
				DwindleNode *dnode = dwindle_find_leaf(*root, w);
				if (dnode)
					dnode->client = c;
			}

			if (layout->id == SCROLLER || layout->id == VERTICAL_SCROLLER) {
				struct TagScrollerState *st = w->mon->pertag->scroller_state[t];
				if (!st)
					continue;
				
				struct ScrollerStackNode *cn = find_scroller_node(st, c);
				if (cn)
					scroller_node_remove(st, cn);

				struct ScrollerStackNode *wn = find_scroller_node(st, w);
				if (wn)
					wn->client = c;
			}
		}
	}

	if (layout->id == SCROLLER || layout->id == VERTICAL_SCROLLER) {
		sync_scroller_state_to_clients(w->mon, w->mon->pertag->curtag);
	}
}

/* Toggle a scratchpad client's visibility, optionally moving it across monitors. */
bool switch_scratchpad_client_state(Client *c) {

	if (config.scratchpad_cross_monitor && selmon && c->mon != selmon &&
		c->is_in_scratchpad) {
		
		Monitor *oldmon = c->mon;
		c->scratchpad_switching_mon = true;
		c->mon = selmon;
		reset_foreign_tolevel(c, oldmon, c->mon);
		client_update_oldmonname_record(c, selmon);

		c->float_geom.width =
			(int32_t)(c->float_geom.width * c->mon->w.width / oldmon->w.width);
		c->float_geom.height = (int32_t)(c->float_geom.height *
										 c->mon->w.height / oldmon->w.height);

		c->float_geom =
			setclient_coordinate_center(c, c->mon, c->float_geom, 0, 0);

		if (c->is_scratchpad_show) {
			c->tags = get_tags_first_tag(selmon->tagset[selmon->seltags]);
			resize(c, c->float_geom, 0);
			arrange(selmon, false, false);
			focusclient(c, true);
			c->scratchpad_switching_mon = false;
			return true;
		} else {
			resize(c, c->float_geom, 0);
			c->scratchpad_switching_mon = false;
		}
	}

	if (c->is_in_scratchpad && c->is_scratchpad_show &&
		(c->mon->tagset[c->mon->seltags] & c->tags) == 0) {
		c->tags = c->mon->tagset[c->mon->seltags];
		arrange(c->mon, false, false);
		focusclient(c, true);
		return true;
	} else if (c->is_in_scratchpad && c->is_scratchpad_show &&
			   (c->mon->tagset[c->mon->seltags] & c->tags) != 0) {
		set_minimized(c);
		return true;
	} else if (c && c->is_in_scratchpad && !c->is_scratchpad_show) {
		show_scratchpad(c);
		return true;
	}

	return false;
}

/* Apply named-scratchpad semantics: hide other scratchpads if single-scratchpad is enabled. */
void apply_named_scratchpad(Client *target_client) {
	Client *c = NULL;
	wl_list_for_each(c, &clients, link) {

		if (!config.scratchpad_cross_monitor && c->mon != selmon) {
			continue;
		}

		if (config.single_scratchpad && c->is_in_scratchpad &&
			c->is_scratchpad_show && c != target_client) {
			set_minimized(c);
		}
	}

	if (!target_client->is_in_scratchpad) {
		set_minimized(target_client);
		switch_scratchpad_client_state(target_client);
	} else
		switch_scratchpad_client_state(target_client);
}

/* Listener: renderer lost — recreate renderer and allocator, then rebind to all outputs. */
void gpureset(struct wl_listener *listener, void *data) {
	struct wlr_renderer *old_drw = drw;
	struct wlr_allocator *old_alloc = alloc;
	struct Monitor *m = NULL;

	wlr_log(WLR_DEBUG, "gpu reset");

	drw = fx_renderer_create(backend);
	if (!drw)
		die("couldn't recreate renderer");

	if (!(alloc = wlr_allocator_autocreate(backend, drw)))
		die("couldn't recreate allocator");

	wl_list_remove(&gpu_reset.link);
	wl_signal_add(&drw->events.lost, &gpu_reset);

	wlr_compositor_set_renderer(compositor, drw);

	wl_list_for_each(m, &mons, link) {
		wlr_output_init_render(m->wlr_output, alloc, drw);
	}

	wlr_allocator_destroy(old_alloc);
	wlr_renderer_destroy(old_drw);
}

/* POSIX signal handler: reap child processes for SIGCHLD or quit on SIGINT/SIGTERM. */
void handlesig(int32_t signo) {
	if (signo == SIGCHLD)
		while (waitpid(-1, NULL, WNOHANG) > 0)
			;
	else if (signo == SIGINT || signo == SIGTERM)
		quit(NULL);
}

/* Trigger overview when the cursor enters/leaves the configured screen-corner hot area. */
void toggle_hotarea(int32_t x_root, int32_t y_root) {
	
	Arg arg = {0};

	if (!selmon)
		return;

	if (grabc)
		return;

	unsigned hx, hy;

	switch (config.hotarea_corner) {
	case BOTTOM_RIGHT:
		hx = selmon->m.x + selmon->m.width - config.hotarea_size;
		hy = selmon->m.y + selmon->m.height - config.hotarea_size;
		break;
	case TOP_LEFT:
		hx = selmon->m.x + config.hotarea_size;
		hy = selmon->m.y + config.hotarea_size;
		break;
	case TOP_RIGHT:
		hx = selmon->m.x + selmon->m.width - config.hotarea_size;
		hy = selmon->m.y + config.hotarea_size;
		break;
	case BOTTOM_LEFT:
	default:
		hx = selmon->m.x + config.hotarea_size;
		hy = selmon->m.y + selmon->m.height - config.hotarea_size;
		break;
	}

	int in_hotarea = 0;

	switch (config.hotarea_corner) {
	case BOTTOM_RIGHT:
		in_hotarea = (y_root > hy && x_root > hx &&
					  x_root <= (selmon->m.x + selmon->m.width) &&
					  y_root <= (selmon->m.y + selmon->m.height));
		break;
	case TOP_LEFT:
		in_hotarea = (y_root < hy && x_root < hx && x_root >= selmon->m.x &&
					  y_root >= selmon->m.y);
		break;
	case TOP_RIGHT:
		in_hotarea = (y_root < hy && x_root > hx &&
					  x_root <= (selmon->m.x + selmon->m.width) &&
					  y_root >= selmon->m.y);
		break;
	case BOTTOM_LEFT:
	default:
		in_hotarea = (y_root > hy && x_root < hx && x_root >= selmon->m.x &&
					  y_root <= (selmon->m.y + selmon->m.height));
		break;
	}

	if (config.enable_hotarea == 1 && selmon->is_in_hotarea == 0 &&
		in_hotarea) {
		toggleoverview(&arg);
		selmon->is_in_hotarea = 1;
	} else if (config.enable_hotarea == 1 && selmon->is_in_hotarea == 1 &&
			   !in_hotarea) {
		selmon->is_in_hotarea = 0;
	}
}

/* Copy applicable window-rule properties onto the client struct. */
static void apply_rule_properties(Client *c, const ConfigWinRule *r) {
	APPLY_INT_PROP(c, r, isterm);
	APPLY_INT_PROP(c, r, allow_csd);
	APPLY_INT_PROP(c, r, force_fakemaximize);
	APPLY_INT_PROP(c, r, force_tiled_state);
	APPLY_INT_PROP(c, r, force_tearing);
	APPLY_INT_PROP(c, r, noswallow);
	APPLY_INT_PROP(c, r, nofocus);
	APPLY_INT_PROP(c, r, nofadein);
	APPLY_INT_PROP(c, r, nofadeout);
	APPLY_INT_PROP(c, r, no_force_center);
	APPLY_INT_PROP(c, r, isfloating);
	APPLY_INT_PROP(c, r, isfullscreen);
	APPLY_INT_PROP(c, r, isfakefullscreen);
	APPLY_INT_PROP(c, r, isnoborder);
	APPLY_INT_PROP(c, r, isnoshadow);
	APPLY_INT_PROP(c, r, isnoradius);
	APPLY_INT_PROP(c, r, isnoanimation);
	APPLY_INT_PROP(c, r, isopensilent);
	APPLY_INT_PROP(c, r, istagsilent);
	APPLY_INT_PROP(c, r, isnamedscratchpad);
	APPLY_INT_PROP(c, r, isglobal);
	APPLY_INT_PROP(c, r, isoverlay);
	APPLY_INT_PROP(c, r, ignore_maximize);
	APPLY_INT_PROP(c, r, ignore_minimize);
	APPLY_INT_PROP(c, r, isnosizehint);
	APPLY_INT_PROP(c, r, indleinhibit_when_focus);
	APPLY_INT_PROP(c, r, isunglobal);
	APPLY_INT_PROP(c, r, noblur);
	APPLY_INT_PROP(c, r, allow_shortcuts_inhibit);

	APPLY_FLOAT_PROP(c, r, scroller_proportion);
	APPLY_FLOAT_PROP(c, r, scroller_proportion_single);
	APPLY_FLOAT_PROP(c, r, focused_opacity);
	APPLY_FLOAT_PROP(c, r, unfocused_opacity);

	APPLY_STRING_PROP(c, r, animation_type_open);
	APPLY_STRING_PROP(c, r, animation_type_close);
}

/* Offset a new floating window slightly so it doesn't overlap existing floating windows. */
void set_float_malposition(Client *tc) {
	Client *c = NULL;
	int32_t x, y, offset, xreverse, yreverse;
	x = tc->geom.x;
	y = tc->geom.y;
	xreverse = 1;
	yreverse = 1;
	offset = MIN(tc->mon->w.width / 20, tc->mon->w.height / 20);

	wl_list_for_each(c, &clients, link) {
		if (c->isfloating && c != tc && VISIBLEON(c, tc->mon) &&
			abs(x - c->geom.x) < offset && abs(y - c->geom.y) < offset) {

			x = c->geom.x + offset * xreverse;
			y = c->geom.y + offset * yreverse;
			if (x < tc->mon->w.x) {
				x = x + offset;
				xreverse = 1;
			}

			if (y < tc->mon->w.y) {
				y = y + offset;
				yreverse = 1;
			}

			if (x + tc->geom.width > tc->mon->w.x + tc->mon->w.width) {
				x = x - offset;
				xreverse = -1;
			}

			if (y + tc->geom.height > tc->mon->w.y + tc->mon->w.height) {
				y = y - offset;
				yreverse = -1;
			}
		}
	}

	tc->float_geom.x = tc->geom.x = x;
	tc->float_geom.y = tc->geom.y = y;
}

/* Pick the appropriate tags for a client when its monitor or tags change. */
void client_reset_mon_tags(Client *c, Monitor *mon, uint32_t newtags) {
	if (!newtags && mon && !mon->isoverview) {
		c->tags = mon->tagset[mon->seltags];
	} else if (!newtags && mon && mon->isoverview) {
		c->tags = mon->ovbk_current_tagset;
	} else if (newtags) {
		c->tags = newtags;
	} else {
		c->tags = mon->tagset[mon->seltags];
	}
}

/* Force client to floating if the destination tag has the open-as-floating pertag flag. */
void check_match_tag_floating_rule(Client *c, Monitor *mon) {
	if (c->tags && !c->isfloating && mon && !c->swallowedby &&
		mon->pertag->open_as_floating[get_tags_first_tag_num(c->tags)]) {
		c->isfloating = 1;
	}
}

/* Match the client against configured window rules and apply properties, geometry, and tags. */
void applyrules(Client *c) {
	
	const char *appid, *title;
	uint32_t i, newtags = 0;
	const ConfigWinRule *r;
	Monitor *m = NULL;
	Client *fc = NULL;
	Client *parent = NULL;

	if (!c)
		return;

	parent = client_get_parent(c);

	Monitor *mon = parent && parent->mon ? parent->mon : selmon;

	c->isfloating = client_is_float_type(c) || parent;

#ifdef XWAYLAND
	if (c->isfloating && client_is_x11(c)) {
		fix_xwayland_unmanaged_coordinate(c);
		c->float_geom = c->geom;
	}
#endif

	if (!(appid = client_get_appid(c)))
		appid = broken;
	if (!(title = client_get_title(c)))
		title = broken;

	for (i = 0; i < config.window_rules_count; i++) {

		r = &config.window_rules[i];

		if (!is_window_rule_matches(r, appid, title))
			continue;

		apply_rule_properties(c, r);

		if (r->tags > 0) {
			newtags |= r->tags;
		} else if (parent) {
			newtags = parent->tags;
		}

		wl_list_for_each(m, &mons, link) {
			if (match_monitor_spec(r->monitor, m)) {
				mon = m;
			}
		}

		if (c->isnamedscratchpad) {
			c->isfloating = 1;
		}

		if (r->width > 1)
			c->float_geom.width = r->width;
		else if (r->width > 0 && r->width <= 1)
			c->float_geom.width = round(mon->m.width * r->width);
		if (r->height > 1)
			c->float_geom.height = r->height;
		else if (r->height > 0 && r->height <= 1)
			c->float_geom.height = round(mon->m.height * r->height);

		if (r->width > 0 || r->height > 0) {
			c->iscustomsize = 1;
		}

		if (r->offsetx || r->offsety) {
			c->iscustompos = 1;
			c->float_geom = c->geom = setclient_coordinate_center(
				c, mon, c->float_geom, r->offsetx, r->offsety);
		}
		if (c->isfloating) {
			c->geom = c->float_geom.width > 0 && c->float_geom.height > 0
						  ? c->float_geom
						  : c->geom;
			if (!c->isnosizehint)
				client_set_size_bound(c);
		}
	}

	if (mon)
		set_size_per(mon, c);

	if (!c->iscustompos &&
		(!client_is_x11(c) || (c->geom.x == 0 && c->geom.y == 0))) {
		c->float_geom = c->geom =
			setclient_coordinate_center(c, mon, c->geom, 0, 0);
	} else {
		c->float_geom = c->geom;
	}

	struct wlr_surface *surface = client_surface(c);
	if (!surface || !surface->mapped)
		return;

	c->pid = client_get_pid(c);
	if (!c->noswallow && !c->isfloating && !client_is_float_type(c) &&
		!c->surface.xdg->initial_commit) {
		Client *p = termforwin(c);
		if (p && !p->isminimized) {
			c->swallowedby = p;
			p->swallowing = c;
			wl_list_remove(&c->link);
			wl_list_remove(&c->flink);
			swallow(c, p);
			wl_list_remove(&p->link);
			wl_list_remove(&p->flink);
			mon = p->mon;
			newtags = p->tags;
		}
	}

	int32_t fullscreen_state_backup =
		c->isfullscreen || client_wants_fullscreen(c);

	bool should_init_get_focus =
		!c->isopensilent &&
		!(client_is_x11_popup(c) && client_should_ignore_focus(c)) && mon &&
		(!c->istagsilent || !newtags || newtags & mon->tagset[mon->seltags]);

	if (!should_init_get_focus) {
		wl_list_remove(&c->flink);
		wl_list_insert(fstack.prev, &c->flink);
	}

	setmon(c, mon, newtags, should_init_get_focus);

	if (!c->isfloating) {
		c->old_stack_inner_per = c->stack_inner_per;
		c->old_master_inner_per = c->master_inner_per;
	}

	if (c->mon &&
		!(c->mon == selmon && c->tags & c->mon->tagset[c->mon->seltags]) &&
		!c->isopensilent && !c->istagsilent) {
		c->animation.tag_from_rule = true;
		view_in_mon(&(Arg){.ui = c->tags}, true, c->mon, true);
	}

	setfullscreen(c, fullscreen_state_backup);

	if (c->isfakefullscreen) {
		setfakefullscreen(c, 1);
	}

	wl_list_for_each(fc, &clients,
					 link) if (fc && fc != c && c->tags & fc->tags && c->mon &&
							   VISIBLEON(fc, c->mon) && ISFULLSCREEN(fc) &&
							   !c->isfloating) {
		clear_fullscreen_flag(fc);
		arrange(c->mon, false, false);
	}

	if (c->isfloating && !c->iscustompos && !c->isnamedscratchpad) {
		wl_list_remove(&c->link);
		wl_list_insert(clients.prev, &c->link);
		set_float_malposition(c);
	}

	if (c->isnamedscratchpad) {
		apply_named_scratchpad(c);
	}

	if (c->isoverlay && c->scene) {
		wlr_scene_node_reparent(&c->scene->node, layers[LyrOverlay]);
		wlr_scene_node_raise_to_top(&c->scene->node);
	}
}

/* Configure each layer surface in list and shrink usable_area when exclusive zones apply. */
void arrangelayer(Monitor *m, struct wl_list *list, struct wlr_box *usable_area,
				  int32_t exclusive) {
	LayerSurface *l = NULL;
	struct wlr_box full_area = m->m;

	wl_list_for_each(l, list, link) {
		struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;

		if (exclusive != (layer_surface->current.exclusive_zone > 0) ||
			!layer_surface->initialized)
			continue;

		if (l->being_unmapped)
			continue;

		wlr_scene_layer_surface_v1_configure(l->scene_layer, &full_area,
											 usable_area);
		wlr_scene_node_set_position(&l->popups->node, l->scene->node.x,
									l->scene->node.y);
	}
}

/* Snap a floating client's edges to nearby windows, monitor edges, or usable-area boundaries. */
void apply_window_snap(Client *c) {
	int32_t snap_up = 99999, snap_down = 99999, snap_left = 99999,
			snap_right = 99999;
	int32_t snap_up_temp = 0, snap_down_temp = 0, snap_left_temp = 0,
			snap_right_temp = 0;
	int32_t snap_up_screen = 0, snap_down_screen = 0, snap_left_screen = 0,
			snap_right_screen = 0;
	int32_t snap_up_mon = 0, snap_down_mon = 0, snap_left_mon = 0,
			snap_right_mon = 0;

	uint32_t cbw = !render_border || c->fake_no_border ? config.borderpx : 0;
	uint32_t tcbw;
	uint32_t cx, cy, cw, ch, tcx, tcy, tcw, tch;
	cx = c->geom.x + cbw;
	cy = c->geom.y + cbw;
	cw = c->geom.width - 2 * cbw;
	ch = c->geom.height - 2 * cbw;

	Client *tc = NULL;
	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	if (!c->isfloating || !config.enable_floating_snap)
		return;

	wl_list_for_each(tc, &clients, link) {
		if (tc && tc->isfloating && !tc->iskilling &&
			client_surface(tc)->mapped && VISIBLEON(tc, c->mon)) {

			tcbw = !render_border || tc->fake_no_border ? config.borderpx : 0;
			tcx = tc->geom.x + tcbw;
			tcy = tc->geom.y + tcbw;
			tcw = tc->geom.width - 2 * tcbw;
			tch = tc->geom.height - 2 * tcbw;

			snap_left_temp = cx - tcx - tcw;
			snap_right_temp = tcx - cx - cw;
			snap_up_temp = cy - tcy - tch;
			snap_down_temp = tcy - cy - ch;

			if (snap_left_temp < snap_left && snap_left_temp >= 0) {
				snap_left = snap_left_temp;
			}
			if (snap_right_temp < snap_right && snap_right_temp >= 0) {
				snap_right = snap_right_temp;
			}
			if (snap_up_temp < snap_up && snap_up_temp >= 0) {
				snap_up = snap_up_temp;
			}
			if (snap_down_temp < snap_down && snap_down_temp >= 0) {
				snap_down = snap_down_temp;
			}
		}
	}

	snap_left_mon = cx - c->mon->m.x;
	snap_right_mon = c->mon->m.x + c->mon->m.width - cx - cw;
	snap_up_mon = cy - c->mon->m.y;
	snap_down_mon = c->mon->m.y + c->mon->m.height - cy - ch;

	if (snap_up_mon >= 0 && snap_up_mon < snap_up)
		snap_up = snap_up_mon;
	if (snap_down_mon >= 0 && snap_down_mon < snap_down)
		snap_down = snap_down_mon;
	if (snap_left_mon >= 0 && snap_left_mon < snap_left)
		snap_left = snap_left_mon;
	if (snap_right_mon >= 0 && snap_right_mon < snap_right)
		snap_right = snap_right_mon;

	snap_left_screen = cx - c->mon->w.x;
	snap_right_screen = c->mon->w.x + c->mon->w.width - cx - cw;
	snap_up_screen = cy - c->mon->w.y;
	snap_down_screen = c->mon->w.y + c->mon->w.height - cy - ch;

	if (snap_up_screen >= 0 && snap_up_screen < snap_up)
		snap_up = snap_up_screen;
	if (snap_down_screen >= 0 && snap_down_screen < snap_down)
		snap_down = snap_down_screen;
	if (snap_left_screen >= 0 && snap_left_screen < snap_left)
		snap_left = snap_left_screen;
	if (snap_right_screen >= 0 && snap_right_screen < snap_right)
		snap_right = snap_right_screen;

	if (snap_left < snap_right && snap_left < config.snap_distance) {
		c->geom.x = c->geom.x - snap_left;
	}

	if (snap_right <= snap_left && snap_right < config.snap_distance) {
		c->geom.x = c->geom.x + snap_right;
	}

	if (snap_up < snap_down && snap_up < config.snap_distance) {
		c->geom.y = c->geom.y - snap_up;
	}

	if (snap_down <= snap_up && snap_down < config.snap_distance) {
		c->geom.y = c->geom.y + snap_down;
	}

	c->float_geom = c->geom;
	resize(c, c->geom, 0);
}

/* Give keyboard focus to a layer surface and notify the input-method relay. */
void focuslayer(LayerSurface *l) {
	focusclient(NULL, 0);
	dwl_im_relay_set_focus(dwl_input_method_relay, l->layer_surface->surface);
	client_notify_enter(l->layer_surface->surface, wlr_seat_get_keyboard(seat));
}

/* Re-evaluate exclusive keyboard focus on layer surfaces and move focus back to a client if needed. */
void reset_exclusive_layers_focus(Monitor *m) {
	LayerSurface *l = NULL;
	int32_t i;
	bool neet_change_focus_to_client = false;
	uint32_t layers_above_shell[] = {
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
		ZWLR_LAYER_SHELL_V1_LAYER_TOP,
		ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM,
	};

	if (!m)
		return;

	for (i = 0; i < (int32_t)LENGTH(layers_above_shell); i++) {
		wl_list_for_each(l, &m->layers[layers_above_shell[i]], link) {
			if (l == exclusive_focus &&
				l->layer_surface->current.keyboard_interactive !=
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE) {

				exclusive_focus = NULL;

				neet_change_focus_to_client = true;
			}

			if (l->layer_surface->surface ==
					seat->keyboard_state.focused_surface &&
				l->being_unmapped) {
				neet_change_focus_to_client = true;
			}

			if (l->layer_surface->current.keyboard_interactive ==
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE &&
				l->layer_surface->surface ==
					seat->keyboard_state.focused_surface) {
				neet_change_focus_to_client = true;
			}

			if (locked ||
				l->layer_surface->current.keyboard_interactive !=
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE ||
				l->being_unmapped)
				continue;
			
			exclusive_focus = l;
			neet_change_focus_to_client = false;
			if (l->layer_surface->surface !=
				seat->keyboard_state.focused_surface)
				focuslayer(l);
			return;
		}
	}

	if (neet_change_focus_to_client) {
		focusclient(focustop(selmon), 1);
	}
}

/* Position all layer surfaces on monitor m and re-arrange tiles if usable area shrank. */
void arrangelayers(Monitor *m) {
	int32_t i;
	struct wlr_box usable_area = m->m;

	if (!m->wlr_output->enabled)
		return;

	if (m->iscleanuping)
		return;

	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 1);

	if (!wlr_box_equal(&usable_area, &m->w)) {
		m->w = usable_area;
		arrange(m, false, false);
	}

	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 0);
}

/* Return true if the pointer device reports tap-capable fingers (i.e. is a trackpad). */
bool pointer_is_trackpad(struct wlr_pointer *pointer) {
	struct libinput_device *device;

	if (wlr_input_device_is_libinput(&pointer->base) &&
		(device = wlr_libinput_get_device_handle(&pointer->base))) {
		if (libinput_device_config_tap_get_finger_count(device) > 0) {
			return true;
		}
	}

	return false;
}

/* Listener: pointer axis (scroll) — dispatch axis bindings or forward to focused client. */
LEMON_HOT void
axisnotify(struct wl_listener *listener, void *data) {
	
	struct wlr_pointer_axis_event *event = data;
	struct wlr_keyboard *keyboard, *hard_keyboard;
	uint32_t mods, hard_mods;
	AxisBinding *a;
	int32_t ji;
	uint32_t adir;
	double target_scroll_factor;
	
	handlecursoractivity();
	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	if (check_trackpad_disabled(event->pointer)) {
		return;
	}

	hard_keyboard = &kb_group->wlr_group->keyboard;
	hard_mods = hard_keyboard ? wlr_keyboard_get_modifiers(hard_keyboard) : 0;

	keyboard = wlr_seat_get_keyboard(seat);
	mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;

	mods = mods | hard_mods;

	if (event->orientation == WL_POINTER_AXIS_VERTICAL_SCROLL)
		adir = event->delta > 0 ? AxisDown : AxisUp;
	else
		adir = event->delta > 0 ? AxisRight : AxisLeft;

	for (ji = 0; ji < config.axis_bindings_count; ji++) {
		if (config.axis_bindings_count < 1)
			break;
		a = &config.axis_bindings[ji];
		if (CLEANMASK(mods) == CLEANMASK(a->mod) &&
			adir == a->dir && a->func) {
			if (event->time_msec - axis_apply_time >
					config.axis_bind_apply_timeout ||
				axis_apply_dir * event->delta < 0) {
				a->func(&a->arg);
				axis_apply_time = event->time_msec;
				axis_apply_dir = event->delta > 0 ? 1 : -1;
				return;
			} else {
				axis_apply_dir = event->delta > 0 ? 1 : -1;
				axis_apply_time = event->time_msec;
				return;
			}
		}
	}

	target_scroll_factor = pointer_is_trackpad(event->pointer)
							   ? config.trackpad_scroll_factor
							   : config.axis_scroll_factor;

	wlr_seat_pointer_notify_axis(
		seat,
		event->time_msec, event->orientation,
		event->delta * target_scroll_factor,
		roundf(event->delta_discrete * target_scroll_factor), event->source,
		event->relative_direction);
}

/* Match the accumulated swipe motion against gesture bindings and dispatch the action. */
int32_t ongesture(struct wlr_pointer_swipe_end_event *event) {
	struct wlr_keyboard *keyboard, *hard_keyboard;
	uint32_t mods, hard_mods;
	const GestureBinding *g;

	uint32_t motion;
	uint32_t adx = (int32_t)round(fabs(swipe_dx));
	uint32_t ady = (int32_t)round(fabs(swipe_dy));
	int32_t handled = 0;
	int32_t ji;

	if (event->cancelled) {
		return handled;
	}

	if (adx * adx + ady * ady <
		config.swipe_min_threshold * config.swipe_min_threshold) {
		return handled;
	}

	if (adx > ady) {
		motion = swipe_dx < 0 ? SWIPE_LEFT : SWIPE_RIGHT;
	} else {
		motion = swipe_dy < 0 ? SWIPE_UP : SWIPE_DOWN;
	}

	hard_keyboard = &kb_group->wlr_group->keyboard;
	hard_mods = hard_keyboard ? wlr_keyboard_get_modifiers(hard_keyboard) : 0;

	keyboard = wlr_seat_get_keyboard(seat);
	mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;

	mods = mods | hard_mods;

	for (ji = 0; ji < config.gesture_bindings_count; ji++) {
		if (config.gesture_bindings_count < 1)
			break;
		g = &config.gesture_bindings[ji];
		if (CLEANMASK(mods) == CLEANMASK(g->mod) &&
			swipe_fingers == g->fingers_count && motion == g->motion &&
			g->func) {
			g->func(&g->arg);
			handled = 1;
		}
	}
	return handled;
}

/* Listener: pointer swipe begin — forward to pointer-gestures protocol. */
void swipe_begin(struct wl_listener *listener, void *data) {
	struct wlr_pointer_swipe_begin_event *event = data;

	wlr_pointer_gestures_v1_send_swipe_begin(pointer_gestures, seat,
											 event->time_msec, event->fingers);
}

/* Listener: pointer swipe update — accumulate delta and forward to clients. */
void swipe_update(struct wl_listener *listener, void *data) {
	struct wlr_pointer_swipe_update_event *event = data;

	swipe_fingers = event->fingers;

	swipe_dx += event->dx;
	swipe_dy += event->dy;

	wlr_pointer_gestures_v1_send_swipe_update(
		pointer_gestures, seat, event->time_msec, event->dx, event->dy);
}

/* Listener: pointer swipe end — fire gesture binding then reset accumulator. */
void swipe_end(struct wl_listener *listener, void *data) {
	struct wlr_pointer_swipe_end_event *event = data;
	ongesture(event);
	swipe_dx = 0;
	swipe_dy = 0;
	
	wlr_pointer_gestures_v1_send_swipe_end(pointer_gestures, seat,
										   event->time_msec, event->cancelled);
}

/* Listener: pointer pinch begin — forward to pointer-gestures protocol. */
void pinch_begin(struct wl_listener *listener, void *data) {
	struct wlr_pointer_pinch_begin_event *event = data;

	wlr_pointer_gestures_v1_send_pinch_begin(pointer_gestures, seat,
											 event->time_msec, event->fingers);
}

/* Listener: pointer pinch update — forward delta, scale and rotation to clients. */
void pinch_update(struct wl_listener *listener, void *data) {
	struct wlr_pointer_pinch_update_event *event = data;

	wlr_pointer_gestures_v1_send_pinch_update(
		pointer_gestures, seat, event->time_msec, event->dx, event->dy,
		event->scale, event->rotation);
}

/* Listener: pointer pinch end — forward end event to clients. */
void pinch_end(struct wl_listener *listener, void *data) {
	struct wlr_pointer_pinch_end_event *event = data;

	wlr_pointer_gestures_v1_send_pinch_end(pointer_gestures, seat,
										   event->time_msec, event->cancelled);
}

/* Listener: pointer hold begin — forward to pointer-gestures protocol. */
void hold_begin(struct wl_listener *listener, void *data) {
	struct wlr_pointer_hold_begin_event *event = data;

	wlr_pointer_gestures_v1_send_hold_begin(pointer_gestures, seat,
											event->time_msec, event->fingers);
}

/* Listener: pointer hold end — forward end event to clients. */
void hold_end(struct wl_listener *listener, void *data) {
	struct wlr_pointer_hold_end_event *event = data;

	wlr_pointer_gestures_v1_send_hold_end(pointer_gestures, seat,
										  event->time_msec, event->cancelled);
}

/* Find the tiled client under (or nearest to) the cursor on the same monitor as c. */
Client *find_closest_tiled_client(Client *c) {
	Client *tc, *closest = NULL;
	long min_dist = LONG_MAX;

	wl_list_for_each(tc, &clients, link) {
		if (tc == c || !ISTILED(tc) || !VISIBLEON(tc, c->mon))
			continue;

		if (cursor->x >= tc->geom.x &&
			cursor->x < tc->geom.x + tc->geom.width &&
			cursor->y >= tc->geom.y &&
			cursor->y < tc->geom.y + tc->geom.height) {
			return tc;
		}

		int32_t dx = tc->geom.x + (int32_t)(tc->geom.width / 2) - cursor->x;
		int32_t dy = tc->geom.y + (int32_t)(tc->geom.height / 2) - cursor->y;
		long dist = (long)dx * dx + (long)dy * dy;

		if (dist < min_dist) {
			min_dist = dist;
			closest = tc;
		}
	}

	return closest;
}

/* Insert dragged client c into the tile tree based on the target's drop direction and layout. */
void place_drag_tile_client(Client *c) {
	Client *closest = find_closest_tiled_client(c);

	if (closest && closest->mon) {
		const Layout *layout =
			closest->mon->pertag->ltidxs[closest->mon->pertag->curtag];

		if (closest->drop_direction == UNDIR) {
			setfloating(c, 0);
			exchange_two_client(c, closest);
			return;
		}

		if (layout->id == SCROLLER) {
			scroller_drop_tile(c, closest, 0);
			return;
		}
		if (layout->id == VERTICAL_SCROLLER) {
			scroller_drop_tile(c, closest, 1);
			return;
		}
		if (layout->id == DWINDLE) {
			uint32_t tag = c->mon->pertag->curtag;
			bool insert_before = (closest->drop_direction == LEFT ||
								  closest->drop_direction == UP);
			bool split_h = (closest->drop_direction == LEFT ||
							closest->drop_direction == RIGHT);
			dwindle_insert(&c->mon->pertag->dwindle_root[tag], c, closest,
						   config.dwindle_split_ratio, insert_before, split_h,
						   !config.dwindle_drop_simple_split);
			setfloating(c, 0);
			return;
		}

		if (closest->drop_direction == LEFT || closest->drop_direction == UP) {
			wl_list_remove(&c->link);
			wl_list_insert(closest->link.prev, &c->link);
		} else {
			wl_list_remove(&c->link);
			wl_list_insert(&closest->link, &c->link);
		}
	}

	setfloating(c, 0);
}

/* Return true when the given pointer is a trackpad and trackpad input is config-disabled. */
bool check_trackpad_disabled(struct wlr_pointer *pointer) {
	if (!config.disable_trackpad) {
		return false;
	}

	return pointer_is_trackpad(pointer);
}

/* Listener: pointer button — refocus client, run mouse bindings, end drag/resize on release. */
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
	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

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
			if (hit < 0) {
				window_cycler_destroy();
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
				Client *victim = window_cycler.clients[hit];
				Monitor *m = window_cycler.mon;
				window_cycler_destroy();
				if (victim && !victim->iskilling)
					pending_kill_client(victim);
				if (m && window_cycler_build(m) <= 1)
					window_cycler_destroy();
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

/* Inform the idle notifier whether any visible non-excluded surface holds an idle inhibitor. */
void checkidleinhibitor(struct wlr_surface *exclude) {
	int32_t inhibited = 0;
	Client *c = NULL;
	struct wlr_surface *surface = NULL;
	struct wlr_idle_inhibitor_v1 *inhibitor;

	wl_list_for_each(inhibitor, &idle_inhibit_mgr->inhibitors, link) {
		surface = wlr_surface_get_root_surface(inhibitor->surface);

		if (exclude == surface) {
			continue;
		}

		toplevel_from_wlr_surface(inhibitor->surface, &c, NULL);

		if (config.idleinhibit_ignore_visible) {
			inhibited = 1;
			break;
		}

		struct wlr_scene_tree *tree = surface->data;
		if (!tree || (tree->node.enabled && (!c || !c->animation.tagouting))) {
			inhibited = 1;
			break;
		}
	}

	wlr_idle_notifier_v1_set_inhibited(idle_notifier, inhibited);
	idle_inhibited = inhibited;
}

/* Listener: cached cursor surface destroyed — drop the dangling pointer. */
void last_cursor_surface_destroy(struct wl_listener *listener, void *data) {
	last_cursor.surface = NULL;
	wl_list_remove(&listener->link);
}

/* Listener: cursor-shape protocol set-shape — apply the requested xcursor name. */
void setcursorshape(struct wl_listener *listener, void *data) {
	struct wlr_cursor_shape_manager_v1_request_set_shape_event *event = data;
	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	
	if (event->seat_client == seat->pointer_state.focused_client) {
		
		if (last_cursor.surface &&
			last_cursor_surface_destroy_listener.link.prev != NULL)
			wl_list_remove(&last_cursor_surface_destroy_listener.link);

		last_cursor.shape = event->shape;
		last_cursor.surface = NULL;
		if (!cursor_hidden)
			wlr_cursor_set_xcursor(cursor, cursor_mgr,
								   wlr_cursor_shape_v1_name(event->shape));
	}
}

/* Remove every globally registered wl_listener so cleanup can free associated objects. */
void cleanuplisteners(void) {
	wl_list_remove(&print_status_listener.link);
	wl_list_remove(&cursor_axis.link);
	wl_list_remove(&cursor_button.link);
	wl_list_remove(&cursor_frame.link);
	wl_list_remove(&cursor_motion.link);
	wl_list_remove(&cursor_motion_absolute.link);
	wl_list_remove(&gpu_reset.link);
	wl_list_remove(&new_idle_inhibitor.link);
	wl_list_remove(&layout_change.link);
	wl_list_remove(&new_input_device.link);
	wl_list_remove(&new_virtual_keyboard.link);
	wl_list_remove(&new_virtual_pointer.link);
	wl_list_remove(&new_pointer_constraint.link);
	wl_list_remove(&new_output.link);
	wl_list_remove(&new_xdg_toplevel.link);
	wl_list_remove(&new_xdg_decoration.link);
	wl_list_remove(&new_xdg_popup.link);
	wl_list_remove(&new_layer_surface.link);
	wl_list_remove(&output_mgr_apply.link);
	wl_list_remove(&output_mgr_test.link);
	wl_list_remove(&output_power_mgr_set_mode.link);
	wl_list_remove(&request_activate.link);
	wl_list_remove(&request_cursor.link);
	wl_list_remove(&request_set_psel.link);
	wl_list_remove(&request_set_sel.link);
	wl_list_remove(&request_set_cursor_shape.link);
	wl_list_remove(&request_start_drag.link);
	wl_list_remove(&start_drag.link);
	wl_list_remove(&new_session_lock.link);
	wl_list_remove(&tearing_new_object.link);
	wl_list_remove(&keyboard_shortcuts_inhibit_new_inhibitor.link);
	if (drm_lease_manager) {
		wl_list_remove(&drm_lease_request.link);
	}
#ifdef XWAYLAND
	wl_list_remove(&new_xwayland_surface.link);
	wl_list_remove(&xwayland_ready.link);
#endif
}

/* Tear down xwayland, clients, child process, cursor, backend, and the scene graph. */
void cleanup(void) {
	surface_cache_save();
	clip_cleanup();
	cleanuplisteners();
#ifdef XWAYLAND
	wlr_xwayland_destroy(xwayland);
	xwayland = NULL;
#endif

	wl_display_destroy_clients(dpy);
	if (child_pid > 0) {
		kill(-child_pid, SIGTERM);
		waitpid(child_pid, NULL, 0);
	}
	wlr_xcursor_manager_destroy(cursor_mgr);

	destroykeyboardgroup(&kb_group->destroy, NULL);

	dwl_im_relay_finish(dwl_input_method_relay);

	wlr_backend_destroy(backend);

	wl_display_destroy(dpy);
	
	wlr_scene_node_destroy(&scene->tree.node);
}

/* Listener: output destroyed — destroy layer surfaces, workspace handles, and the Monitor. */
void cleanupmon(struct wl_listener *listener, void *data) {
	Monitor *m = wl_container_of(listener, m, destroy);
	LayerSurface *l = NULL, *tmp = NULL;
	uint32_t i;

	m->iscleanuping = true;

	for (i = 0; i < LENGTH(m->layers); i++) {
		wl_list_for_each_safe(l, tmp, &m->layers[i], link)
			wlr_layer_surface_v1_destroy(l->layer_surface);
	}

	wlr_ext_workspace_group_handle_v1_output_leave(m->ext_group, m->wlr_output);
	wlr_ext_workspace_group_handle_v1_destroy(m->ext_group);
	cleanup_workspaces_by_monitor(m);

	wl_list_remove(&m->destroy.link);
	wl_list_remove(&m->frame.link);
	wl_list_remove(&m->link);
	wl_list_remove(&m->request_state.link);
	if (m->lock_surface)
		destroylocksurface(&m->destroy_lock_surface, NULL);
	m->wlr_output->data = NULL;
	wlr_output_layout_remove(output_layout, m->wlr_output);
	if (m->ov_dim) {
		wlr_scene_node_destroy(&m->ov_dim->node);
		m->ov_dim = NULL;
	}
	wlr_scene_output_destroy(m->scene_output);

	closemon(m);
	if (m->blur) {
		wlr_scene_node_destroy(&m->blur->node);
		m->blur = NULL;
	}
	if (m->skip_frame_timeout) {
		monitor_stop_skip_frame_timer(m);
		wl_event_source_remove(m->skip_frame_timeout);
		m->skip_frame_timeout = NULL;
	}
	if (m->battery_frame_throttle) {
		wl_event_source_remove(m->battery_frame_throttle);
		m->battery_frame_throttle = NULL;
	}
	if (m->render_deadline) {
		wl_event_source_remove(m->render_deadline);
		m->render_deadline = NULL;
	}
	m->wlr_output->data = NULL;

	cleanup_monitor_dwindle(m);
	cleanup_monitor_scroller(m);

	free(m->pertag);
	free(m);
}

/* Migrate every client from monitor m to selmon (or detach if no monitor remains). */
void closemon(Monitor *m) {
	
	Client *c = NULL;
	int32_t i = 0, nmons = wl_list_length(&mons);
	if (!nmons) {
		selmon = NULL;
	} else if (m == selmon) {
		do
			selmon = wl_container_of(mons.next, selmon, link);
		while (!selmon->wlr_output->enabled && i++ < nmons);

		if (!selmon->wlr_output->enabled)
			selmon = NULL;
	}

	wl_list_for_each(c, &clients, link) {
		if (c->mon == m) {

			if (selmon == NULL) {
				if (c->foreign_toplevel) {
					wlr_foreign_toplevel_handle_v1_output_leave(
						c->foreign_toplevel, c->mon->wlr_output);
					wlr_foreign_toplevel_handle_v1_destroy(c->foreign_toplevel);
					c->foreign_toplevel = NULL;
				}

				c->mon = NULL;
			} else {
				client_change_mon(c, selmon);
			}
			
			if (c->oldmonname[0] == '\0') {
				client_update_oldmonname_record(c, m);
			}
		}
	}
	if (selmon) {
		focusclient(focustop(selmon), 1);
		printstatus();
	}
}

/* Per-buffer callback: enable backdrop blur on layer-surface scene buffers. */
static void iter_layer_scene_buffers(struct wlr_scene_buffer *buffer,
									 int32_t sx, int32_t sy, void *user_data) {
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(buffer);
	if (!scene_surface) {
		return;
	}

	wlr_scene_buffer_set_backdrop_blur(buffer, true);
	wlr_scene_buffer_set_backdrop_blur_ignore_transparent(buffer, true);
	if (config.blur_optimized) {
		wlr_scene_buffer_set_backdrop_blur_optimized(buffer, true);
	} else {
		wlr_scene_buffer_set_backdrop_blur_optimized(buffer, false);
	}
}

/* Mark the monitor's optimized blur dirty when a background layer surface changes. */
void layer_flush_blur_background(LayerSurface *l) {
	if (!config.blur)
		return;

	if (l->layer_surface->current.layer ==
		ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND) {
		if (l->mon) {
			wlr_scene_optimized_blur_mark_dirty(l->mon->blur);
		}
	}
}

/* Listener: layer-surface mapped — apply layer rules, create shadow, kick off open animation. */
void maplayersurfacenotify(struct wl_listener *listener, void *data) {
	LayerSurface *l = wl_container_of(listener, l, map);
	struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;
	int32_t ji;
	ConfigLayerRule *r;

	l->mapped = 1;

	if (!l->mon)
		return;
	strncpy(l->mon->last_surface_ws_name, layer_surface->namespace,
			sizeof(l->mon->last_surface_ws_name) - 1);
	l->mon->last_surface_ws_name[sizeof(l->mon->last_surface_ws_name) - 1] =
		'\0';

	get_layer_target_geometry(l, &l->geom);

	l->noanim = 0;
	l->dirty = false;
	l->noblur = 0;
	l->shadow = NULL;
	l->need_output_flush = true;

	for (ji = 0; ji < config.layer_rules_count; ji++) {
		if (config.layer_rules_count < 1)
			break;
		if (regex_match(config.layer_rules[ji].layer_name,
						l->layer_surface->namespace)) {

			r = &config.layer_rules[ji];
			APPLY_INT_PROP(l, r, noblur);
			APPLY_INT_PROP(l, r, noanim);
			APPLY_INT_PROP(l, r, noshadow);
			APPLY_STRING_PROP(l, r, animation_type_open);
			APPLY_STRING_PROP(l, r, animation_type_close);
		}
	}

	if (layer_surface->current.exclusive_zone == 0 &&
		layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM &&
		layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND) {
		l->shadow =
			wlr_scene_shadow_create(l->scene, 0, 0, config.border_radius,
									config.shadows_blur, config.shadowscolor);
		wlr_scene_node_lower_to_bottom(&l->shadow->node);
		wlr_scene_node_set_enabled(&l->shadow->node, true);
	}

	if (config.animations && config.layer_animations && !l->noanim) {
		l->animation.duration = config.animation_duration_open;
		l->animation.action = OPEN;
		layer_set_pending_state(l);
	}
	
	arrangelayers(l->mon);
	reset_exclusive_layers_focus(l->mon);
}

/* Listener: layer surface committed — handle initial configure, layer changes, blur and reflow. */
void commitlayersurfacenotify(struct wl_listener *listener, void *data) {
	LayerSurface *l = wl_container_of(listener, l, surface_commit);
	struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;
	struct wlr_scene_tree *scene_layer =
		layers[layermap[layer_surface->current.layer]];
	struct wlr_layer_surface_v1_state old_state;
	struct wlr_box box;

	if (l->layer_surface->initial_commit) {
		client_set_scale(layer_surface->surface, l->mon->wlr_output->scale);

		old_state = l->layer_surface->current;
		l->layer_surface->current = l->layer_surface->pending;
		arrangelayers(l->mon);
		l->layer_surface->current = old_state;
		
		if (!exclusive_focus &&
			l->layer_surface->current.keyboard_interactive ==
				ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND) {
			focuslayer(l);
		}
		return;
	}

	if (l->mapped && !layer_surface->surface->buffer) {
		wlr_scene_node_set_enabled(&l->scene->node, false);
		return;
	} else {
		wlr_scene_node_set_enabled(&l->scene->node, true);
	}

	get_layer_target_geometry(l, &box);

	if (config.animations && config.layer_animations && !l->noanim &&
		l->mapped &&
		layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM &&
		layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND &&
		!wlr_box_equal(&box, &l->geom)) {

		l->geom.x = box.x;
		l->geom.y = box.y;
		l->geom.width = box.width;
		l->geom.height = box.height;
		l->animation.action = MOVE;
		l->animation.duration = config.animation_duration_move;
		l->need_output_flush = true;
		layer_set_pending_state(l);
	}

	if (config.blur && config.blur_layer) {

		if (!l->noblur &&
			layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM &&
			layer_surface->current.layer !=
				ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND &&
			!l->blur_state_applied) {

			wlr_scene_node_for_each_buffer(&l->scene->node,
										   iter_layer_scene_buffers, l);
			l->blur_state_applied = true;
		}
	}

	layer_flush_blur_background(l);

	if (layer_surface->current.committed == 0 &&
		l->mapped == layer_surface->surface->mapped)
		return;
	l->mapped = layer_surface->surface->mapped;

	if (layer_surface->current.committed & WLR_LAYER_SURFACE_V1_STATE_LAYER) {
		if (scene_layer != l->scene->node.parent) {
			wlr_scene_node_reparent(&l->scene->node, scene_layer);
			wl_list_remove(&l->link);
			wl_list_insert(&l->mon->layers[layer_surface->current.layer],
						   &l->link);
			wlr_scene_node_reparent(
				&l->popups->node,
				(layer_surface->current.layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP
					 ? layers[LyrTop]
					 : scene_layer));
		}
	}

	arrangelayers(l->mon);

	if (layer_surface->current.committed &
		WLR_LAYER_SURFACE_V1_STATE_KEYBOARD_INTERACTIVITY) {
		reset_exclusive_layers_focus(l->mon);
	}
}

/* Listener: xdg surface committed — handle initial commit setup and resize on geometry change. */
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

/* Listener: xdg-decoration destroyed — detach decoration-related listeners from the client. */
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

/* Listener: xdg popup destroyed — remove its listeners and free the Popup wrapper. */
static void destroypopup(struct wl_listener *listener, void *data) {
	Popup *popup = wl_container_of(listener, popup, destroy);
	wl_list_remove(&popup->destroy.link);
	wl_list_remove(&popup->reposition.link);
	free(popup);
}

/* Listener: popup surface initial commit — attach to scene tree and unconstrain to parent. */
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

/* Listener: popup repositioned — re-run unconstrain to keep it on screen. */
static void repositionpopup(struct wl_listener *listener, void *data) {
	Popup *popup = wl_container_of(listener, popup, reposition);
	(void)popup_unconstrain(popup);
}

/* Listener: new xdg popup — allocate Popup state and wire up destroy/commit/reposition listeners. */
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

/* Listener: new xdg-toplevel decoration — attach mode/destroy listeners and apply mode. */
void createdecoration(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel_decoration_v1 *deco = data;
	Client *c = deco->toplevel->base->data;
	c->decoration = deco;

	LISTEN(&deco->events.request_mode, &c->set_decoration_mode,
		   requestdecorationmode);
	LISTEN(&deco->events.destroy, &c->destroy_decoration, destroydecoration);

	requestdecorationmode(&c->set_decoration_mode, deco);
}

/* Listener: new idle inhibitor — listen for its destruction and recompute inhibit state. */
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

/* Listener: new wlr-layer-shell surface — build scene nodes and attach map/commit/unmap. */
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

void createmon(struct wl_listener *listener, void *data) {
	
	struct wlr_output *wlr_output = data;
	const ConfigMonitorRule *r;
	uint32_t i;
	int32_t ji, vrr, custom;
	struct wlr_output_state state;
	Monitor *m = NULL;
	bool custom_monitor_mode = false;

	if (!wlr_output_init_render(wlr_output, alloc, drw))
		return;

	if (wlr_output->non_desktop) {
		if (drm_lease_manager) {
			wlr_drm_lease_v1_manager_offer_output(drm_lease_manager,
												  wlr_output);
		}
		return;
	}

	struct wl_event_loop *loop = wl_display_get_event_loop(dpy);
	m = wlr_output->data = ecalloc(1, sizeof(*m));

	m->iscleanuping = false;
	m->skip_frame_timeout =
		wl_event_loop_add_timer(loop, monitor_skip_frame_timeout_callback, m);
	m->battery_frame_throttle =
		wl_event_loop_add_timer(loop, battery_frame_throttle_callback, m);
	m->render_deadline =
		wl_event_loop_add_timer(loop, render_deadline_callback, m);
	m->last_anim_schedule_ms = 0;
	m->skiping_frame = false;
	m->resizing_count_pending = 0;
	m->resizing_count_current = 0;

	m->wlr_output = wlr_output;
	m->wlr_output->data = m;

	wl_list_init(&m->dwl_ipc_outputs);

	for (i = 0; i < LENGTH(m->layers); i++)
		wl_list_init(&m->layers[i]);

	m->gappih = config.gappih;
	m->gappiv = config.gappiv;
	m->gappoh = config.gappoh;
	m->gappov = config.gappov;
	m->isoverview = 0;
	m->sel = NULL;
	m->is_in_hotarea = 0;
	m->m.x = INT32_MAX;
	m->m.y = INT32_MAX;
	float scale = 1;
	enum wl_output_transform rr = WL_OUTPUT_TRANSFORM_NORMAL;

	wlr_output_state_init(&state);
	wlr_output_state_set_scale(&state, scale);
	wlr_output_state_set_transform(&state, rr);

	/* Force horizontal-RGB subpixel order so clients can do LCD subpixel AA.
	   Only correct on standard RGB-stripe panels; opt-in via config. */
	if (config.subpixel_rgb)
		wlr_output_state_set_subpixel(&state, WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB);

	for (ji = 0; ji < config.monitor_rules_count; ji++) {
		if (config.monitor_rules_count < 1)
			break;

		r = &config.monitor_rules[ji];

		if (monitor_matches_rule(m, r)) {
			m->m.x = r->x == INT32_MAX ? INT32_MAX : r->x;
			m->m.y = r->y == INT32_MAX ? INT32_MAX : r->y;
			vrr = r->vrr >= 0 ? r->vrr : 0;
			custom = r->custom >= 0 ? r->custom : 0;
			scale = r->scale;
			rr = r->rr;

			if (apply_rule_to_state(m, r, &state, vrr, custom)) {
				custom_monitor_mode = true;
			}
			break;
		}
	}

	if (!custom_monitor_mode)
		wlr_output_state_set_mode(&state,
								  wlr_output_preferred_mode(wlr_output));

	LISTEN(&wlr_output->events.frame, &m->frame, rendermon);
	LISTEN(&wlr_output->events.destroy, &m->destroy, cleanupmon);
	LISTEN(&wlr_output->events.request_state, &m->request_state,
		   requestmonstate);

	wlr_output_state_set_enabled(&state, 1);
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	wl_list_insert(&mons, &m->link);

	m->pertag = calloc(1, sizeof(Pertag));
	for (int i = 0; i < LENGTH(tags) + 1; i++)
		m->pertag->scroller_state[i] = NULL;

	if (chvt_backup_tag &&
		regex_match(chvt_backup_selmon, m->wlr_output->name)) {
		m->tagset[0] = m->tagset[1] = (1 << (chvt_backup_tag - 1)) & TAGMASK;
		m->pertag->curtag = m->pertag->prevtag = chvt_backup_tag;
		chvt_backup_tag = 0;
		memset(chvt_backup_selmon, 0, sizeof(chvt_backup_selmon));
	} else {
		m->tagset[0] = m->tagset[1] = 1;
		m->pertag->curtag = m->pertag->prevtag = 1;
	}

	for (i = 0; i <= LENGTH(tags); i++) {
		m->pertag->nmasters[i] = config.default_nmaster;
		m->pertag->mfacts[i] = config.default_mfact;
		m->pertag->ltidxs[i] = &layouts[0];
	}

	parse_tagrule(m);

	m->scene_output = wlr_scene_output_create(scene, wlr_output);

	/* Overview dim backdrop: a black rect sitting just under the tiled
	   clients, faded in while this monitor is in overview. */
	m->ov_dim = wlr_scene_rect_create(&scene->tree, 0, 0,
									  (float[4]){0, 0, 0, 0});
	wlr_scene_node_place_below(&m->ov_dim->node, &layers[LyrTile]->node);
	wlr_scene_node_set_enabled(&m->ov_dim->node, false);
	m->ov_dim_cur = 0.0f;

	if (m->m.x == INT32_MAX || m->m.y == INT32_MAX)
		wlr_output_layout_add_auto(output_layout, wlr_output);
	else
		wlr_output_layout_add(output_layout, wlr_output, m->m.x, m->m.y);

	if (config.blur) {
		m->blur = wlr_scene_optimized_blur_create(&scene->tree, 0, 0);
		wlr_scene_node_set_position(&m->blur->node, m->m.x, m->m.y);
		wlr_scene_node_reparent(&m->blur->node, layers[LyrBlur]);
		wlr_scene_optimized_blur_set_size(m->blur, m->m.width, m->m.height);
	}
	m->ext_group = wlr_ext_workspace_group_handle_v1_create(
		ext_manager, EXT_WORKSPACE_ENABLE_CAPS);
	wlr_ext_workspace_group_handle_v1_output_enter(m->ext_group, m->wlr_output);

	for (i = 1; i <= LENGTH(tags); i++) {
		add_workspace_by_tag(i, m);
	}

	printstatus();
}

void
createnotify(struct wl_listener *listener, void *data) {
	
	struct wlr_xdg_toplevel *toplevel = data;
	Client *c = NULL;

	c = toplevel->base->data = ecalloc(1, sizeof(*c));
	c->surface.xdg = toplevel->base;
	c->bw = config.borderpx;

	LISTEN(&toplevel->base->surface->events.commit, &c->commit, commitnotify);
	LISTEN(&toplevel->base->surface->events.map, &c->map, mapnotify);
	LISTEN(&toplevel->base->surface->events.unmap, &c->unmap, unmapnotify);
	LISTEN(&toplevel->events.destroy, &c->destroy, destroynotify);
	LISTEN(&toplevel->events.request_fullscreen, &c->fullscreen,
		   fullscreennotify);
	LISTEN(&toplevel->events.request_maximize, &c->maximize, maximizenotify);
	LISTEN(&toplevel->events.request_minimize, &c->minimize, minimizenotify);
	LISTEN(&toplevel->events.set_title, &c->set_title, updatetitle);
}

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

void
destroynotify(struct wl_listener *listener, void *data) {
	
	Client *c = wl_container_of(listener, c, destroy);
	wl_list_remove(&c->destroy.link);
	wl_list_remove(&c->set_title.link);
	wl_list_remove(&c->fullscreen.link);
	wl_list_remove(&c->maximize.link);
	wl_list_remove(&c->minimize.link);
#ifdef XWAYLAND
	if (c->type != XDGShell) {
		wl_list_remove(&c->activate.link);
		wl_list_remove(&c->associate.link);
		wl_list_remove(&c->configure.link);
		wl_list_remove(&c->dissociate.link);
		wl_list_remove(&c->set_hints.link);
	} else
#endif
	{
		wl_list_remove(&c->commit.link);
		wl_list_remove(&c->map.link);
		wl_list_remove(&c->unmap.link);
	}
	free(c);
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

/* Architecture brief §9: recompute render tier for every mapped client.
   Cheap: walks the clients list once and runs at most O(N) box checks. Called
   from focusclient and after arrange, never on the per-frame hot path. */
static void recompute_render_tiers(void) {
	Client *c;
	wl_list_for_each(c, &clients, link) {
		if (!c->mon || !client_surface(c)->mapped) {
			c->render_tier = TIER_HIDDEN;
			continue;
		}
		if (!VISIBLEON(c, c->mon)) {
			c->render_tier = TIER_HIDDEN;
			continue;
		}
		if (c->mon->sel == c) {
			c->render_tier = TIER_FOCUS;
			continue;
		}
		Client *fs = c->mon->sel;
		if (fs && fs != c && fs->isfullscreen && !c->isfloating) {
			c->render_tier = TIER_OCCLUDED;
			continue;
		}
		c->render_tier = TIER_VISIBLE;
	}
}

/* I/O priority classes for ioprio_set (no glibc wrapper exists). */
#ifndef IOPRIO_WHO_PGRP
#define IOPRIO_WHO_PGRP 2
#endif
#define IOPRIO_CLASS_SHIFT 13
#define IOPRIO_CLASS_BE 2
#define IOPRIO_CLASS_IDLE 3
#define IOPRIO_PRIO_VALUE(klass, data) (((klass) << IOPRIO_CLASS_SHIFT) | (data))

/* syscall() is not exposed under _POSIX_C_SOURCE; declare it for ioprio_set. */
extern long syscall(long number, ...);

static bool qos_warned = false;

/* Apply CPU nice + I/O class to a client's process group (focus QoS).
   Demotion always succeeds; raising priority needs CAP_SYS_NICE, so a single
   EPERM is logged once and otherwise ignored (background demotion still wins). */
static void qos_apply(Client *c, int32_t nice_val, int32_t io_class) {
	if (!config.focus_qos || !c || c->iskilling)
		return;
	pid_t pid = client_get_pid(c);
	if (pid <= 0)
		return;
	pid_t pgid = getpgid(pid);
	if (pgid <= 0)
		pgid = pid;

	errno = 0;
	if (setpriority(PRIO_PGRP, pgid, nice_val) != 0 && errno == EPERM &&
		!qos_warned) {
		wlr_log(WLR_INFO,
				"focus_qos: raising priority needs CAP_SYS_NICE "
				"(setcap cap_sys_nice+ep on the lemon binary); background "
				"demotion still applies without it");
		qos_warned = true;
	}
	syscall(SYS_ioprio_set, IOPRIO_WHO_PGRP, pgid,
			IOPRIO_PRIO_VALUE(io_class, 4));
}

/* Give a client foreground QoS: normal nice and best-effort I/O. */
static void qos_promote(Client *c) { qos_apply(c, 0, IOPRIO_CLASS_BE); }

/* Throttle a backgrounded client: higher nice and idle-class I/O. */
static void qos_demote(Client *c) {
	qos_apply(c, config.focus_qos_bg_nice, IOPRIO_CLASS_IDLE);
}

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

void
fullscreennotify(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, fullscreen);

	if (!c || c->iskilling)
		return;

	setfullscreen(c, client_wants_fullscreen(c));
}

void requestmonstate(struct wl_listener *listener, void *data) {
	
	Monitor *m = wl_container_of(listener, m, request_state);
	const struct wlr_output_event_request_state *event = data;

	if (event->state->committed == WLR_OUTPUT_STATE_MODE) {
		switch (event->state->mode_type) {
		case WLR_OUTPUT_STATE_MODE_FIXED:
			wlr_output_state_set_mode(&m->pending, event->state->mode);
			break;
		case WLR_OUTPUT_STATE_MODE_CUSTOM:
			wlr_output_state_set_custom_mode(&m->pending,
											 event->state->custom_mode.width,
											 event->state->custom_mode.height,
											 event->state->custom_mode.refresh);
			break;
		}
		updatemons(NULL, NULL);
		wlr_output_schedule_frame(m->wlr_output);
		return;
	}

	if (!wlr_output_commit_state(m->wlr_output, event->state)) {
		wlr_log(WLR_ERROR,
				"Backend requested a new state that could not be applied");
	}
}

void inputdevice(struct wl_listener *listener, void *data) {
	
	struct wlr_input_device *device = data;
	uint32_t caps;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		createkeyboard(wlr_keyboard_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_POINTER:
		createpointer(wlr_pointer_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_SWITCH:
		createswitch(wlr_switch_from_input_device(device));
		break;
	default:
		
		break;
	}

	caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&kb_group->wlr_group->devices))
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	wlr_seat_set_capabilities(seat, caps);
}

int32_t keyrepeat(void *data) {
	KeyboardGroup *group = data;
	int32_t i;
	if (!group->nsyms || group->wlr_group->keyboard.repeat_info.rate <= 0)
		return 0;

	wl_event_source_timer_update(
		group->key_repeat_source,
		1000 / group->wlr_group->keyboard.repeat_info.rate);

	for (i = 0; i < group->nsyms; i++)
		keybinding(WL_KEYBOARD_KEY_STATE_PRESSED, false, group->mods,
				   group->keysyms[i], group->keycode);

	return 0;
}

bool is_keyboard_shortcut_inhibitor(struct wlr_surface *surface) {
	KeyboardShortcutsInhibitor *kbsinhibitor;

	wl_list_for_each(kbsinhibitor, &keyboard_shortcut_inhibitors, link) {
		if (kbsinhibitor->inhibitor->surface == surface) {
			return true;
		}
	}
	return false;
}

int32_t
keybinding(uint32_t state, bool locked, uint32_t mods, xkb_keysym_t sym,
		   uint32_t keycode) {
	
	int32_t handled = 0;
	const KeyBinding *k;
	int32_t ji;
	int32_t isbreak = 0;

	if (is_keyboard_shortcut_inhibitor(seat->keyboard_state.focused_surface)) {
		return false;
	}

	for (ji = 0; ji < config.key_bindings_count; ji++) {
		if (config.key_bindings_count < 1)
			break;

		if (locked && config.key_bindings[ji].islockapply == false)
			continue;

		if (state == WL_KEYBOARD_KEY_STATE_RELEASED &&
			config.key_bindings[ji].isreleaseapply == false)
			continue;

		if (state == WL_KEYBOARD_KEY_STATE_PRESSED &&
			config.key_bindings[ji].isreleaseapply == true)
			continue;

		if (state != WL_KEYBOARD_KEY_STATE_PRESSED &&
			state != WL_KEYBOARD_KEY_STATE_RELEASED)
			continue;

		k = &config.key_bindings[ji];
		if ((k->iscommonmode || (k->isdefaultmode && keymode.isdefault) ||
			 (strcmp(keymode.mode, k->mode) == 0)) &&
			CLEANMASK(mods) == CLEANMASK(k->mod) &&
			((k->keysymcode.type == KEY_TYPE_SYM &&
			  xkb_keysym_to_lower(sym) ==
				  xkb_keysym_to_lower(k->keysymcode.keysym)) ||
			 (k->keysymcode.type == KEY_TYPE_CODE &&
			  (keycode == k->keysymcode.keycode.keycode1 ||
			   keycode == k->keysymcode.keycode.keycode2 ||
			   keycode == k->keysymcode.keycode.keycode3))) &&
			k->func) {

			if (!k->ispassapply)
				handled = 1;
			else
				handled = 0;

			isbreak = k->func(&k->arg);

			if (isbreak)
				break;
		}
	}
	return handled;
}

bool keypressglobal(struct wlr_surface *last_surface,
					struct wlr_keyboard *keyboard,
					struct wlr_keyboard_key_event *event, uint32_t mods,
					xkb_keysym_t keysym, uint32_t keycode) {
	Client *c = NULL, *lastc = focustop(selmon);
	uint32_t keycodes[32] = {0};
	int32_t reset = false;
	const char *appid = NULL;
	const char *title = NULL;
	int32_t ji;
	const ConfigWinRule *r;

	for (ji = 0; ji < config.window_rules_count; ji++) {
		if (config.window_rules_count < 1)
			break;
		r = &config.window_rules[ji];

		if (!r->globalkeybinding.mod ||
			(!r->globalkeybinding.keysymcode.keysym &&
			 !r->globalkeybinding.keysymcode.keycode.keycode1 &&
			 !r->globalkeybinding.keysymcode.keycode.keycode2 &&
			 !r->globalkeybinding.keysymcode.keycode.keycode3))
			continue;

		if (((r->globalkeybinding.keysymcode.type == KEY_TYPE_SYM &&
			  r->globalkeybinding.keysymcode.keysym == keysym) ||
			 (r->globalkeybinding.keysymcode.type == KEY_TYPE_CODE &&
			  (r->globalkeybinding.keysymcode.keycode.keycode1 == keycode ||
			   r->globalkeybinding.keysymcode.keycode.keycode2 == keycode ||
			   r->globalkeybinding.keysymcode.keycode.keycode3 == keycode))) &&
			r->globalkeybinding.mod == mods) {
			wl_list_for_each(c, &clients, link) {
				if (c && c != lastc) {
					appid = client_get_appid(c);
					title = client_get_title(c);

					if ((r->title && regex_match(r->title, title) && !r->id) ||
						(r->id && regex_match(r->id, appid) && !r->title) ||
						(r->id && regex_match(r->id, appid) && r->title &&
						 regex_match(r->title, title))) {
						reset = true;
						wlr_seat_keyboard_enter(seat, client_surface(c),
												keycodes, 0,
												&keyboard->modifiers);
						wlr_seat_keyboard_send_key(seat, event->time_msec,
												   event->keycode,
												   event->state);
						goto done;
					}
				}
			}
		}
	}

done:
	if (reset)
		wlr_seat_keyboard_enter(seat, last_surface, keycodes, 0,
								&keyboard->modifiers);
	return reset;
}

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

	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);
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

	/* Commit window cycler selection on Alt release (keycodes 64=Alt_L,
	   108=Alt_R). The cycler is opened by the window_cycler_next/prev
	   action and held visible while Alt stays down. */
	if (window_cycler.active && !locked && group == kb_group &&
		event->state == WL_KEYBOARD_KEY_STATE_RELEASED &&
		(keycode == 64 || keycode == 108)) {
		window_cycler_commit();
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

	for (i = 0; i < nsyms; i++)
		handled =
			keybinding(event->state, locked, mods, syms[i], keycode) || handled;

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

void pending_kill_client(Client *c) {
	if (!c || c->iskilling)
		return;
	client_send_close(c);
}

void locksession(struct wl_listener *listener, void *data) {
	struct wlr_session_lock_v1 *session_lock = data;
	SessionLock *lock;
	if (!config.allow_lock_transparent) {
		wlr_scene_node_set_enabled(&locked_bg->node, true);
	}
	if (cur_lock) {
		wlr_session_lock_v1_destroy(session_lock);
		return;
	}
	lock = session_lock->data = ecalloc(1, sizeof(*lock));
	focusclient(NULL, 0);

	lock->scene = wlr_scene_tree_create(layers[LyrBlock]);
	cur_lock = lock->lock = session_lock;
	locked = 1;

	LISTEN(&session_lock->events.new_surface, &lock->new_surface,
		   createlocksurface);
	LISTEN(&session_lock->events.destroy, &lock->destroy, destroysessionlock);
	LISTEN(&session_lock->events.unlock, &lock->unlock, unlocksession);

	wlr_session_lock_v1_send_locked(session_lock);
}

static void iter_xdg_scene_buffers(struct wlr_scene_buffer *buffer, int32_t sx,
								   int32_t sy, void *user_data) {
	Client *c = user_data;

	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(buffer);
	if (!scene_surface) {
		return;
	}

	struct wlr_surface *surface = scene_surface->surface;
	
	if (wlr_subsurface_try_from_wlr_surface(surface) != NULL)
		return;

	if (config.blur && c && !c->noblur && !c->isfullscreen &&
		!c->ismaximizescreen) {
		wlr_scene_buffer_set_backdrop_blur(buffer, true);
		wlr_scene_buffer_set_backdrop_blur_ignore_transparent(buffer, false);
		if (config.blur_optimized) {
			wlr_scene_buffer_set_backdrop_blur_optimized(buffer, true);
		} else {
			wlr_scene_buffer_set_backdrop_blur_optimized(buffer, false);
		}
	} else {
		wlr_scene_buffer_set_backdrop_blur(buffer, false);
	}
}

void init_client_properties(Client *c) {
	c->grid_col_per = 1.0f;
	c->grid_row_per = 1.0f;
	c->drop_direction = UNDIR;
	c->enable_drop_area_draw = false;
	c->isfocusing = false;
	c->isfloating = 0;
	c->isfakefullscreen = 0;
	c->isnoanimation = 0;
	c->isopensilent = 0;
	c->istagsilent = 0;
	c->noswallow = 0;
	c->isterm = 0;
	c->noblur = 0;
	c->tearing_hint = 0;
	c->overview_isfullscreenbak = 0;
	c->overview_ismaximizescreenbak = 0;
	c->overview_isfloatingbak = 0;
	c->pid = 0;
	c->swallowing = NULL;
	c->swallowedby = NULL;
	c->ismaster = 0;
	c->old_ismaster = 0;
	c->isleftstack = 0;
	c->ismaximizescreen = 0;
	c->isfullscreen = 0;
	c->need_float_size_reduce = 0;
	c->iskilling = 0;
	c->istagswitching = 0;
	c->isglobal = 0;
	c->isminimized = 0;
	c->isoverlay = 0;
	c->isunglobal = 0;
	c->is_in_scratchpad = 0;
	c->isnamedscratchpad = 0;
	c->is_scratchpad_show = 0;
	c->need_float_size_reduce = 0;
	c->is_clip_to_hide = 0;
	c->is_restoring_from_ov = 0;
	c->isurgent = 0;
	c->need_output_flush = 0;
	c->last_active_ms = (uint32_t)get_now_in_ms();
	c->scroller_proportion = config.scroller_default_proportion;
	c->is_pending_open_animation = true;
	c->drag_to_tile = false;
	c->scratchpad_switching_mon = false;
	c->fake_no_border = false;
	c->focused_opacity = config.focused_opacity;
	c->unfocused_opacity = config.unfocused_opacity;
	c->nofocus = 0;
	c->nofadein = 0;
	c->nofadeout = 0;
	c->no_force_center = 0;
	c->isnoborder = 0;
	c->isnosizehint = 0;
	c->isnoradius = 0;
	c->isnoshadow = 0;
	c->ignore_maximize = 1;
	c->ignore_minimize = 1;
	c->iscustomsize = 0;
	c->iscustompos = 0;
	c->master_mfact_per = 0.0f;
	c->master_inner_per = 0.0f;
	c->stack_inner_per = 0.0f;
	c->old_stack_inner_per = 0.0f;
	c->old_master_inner_per = 0.0f;
	c->old_master_mfact_per = 0.0f;
	c->isterm = 0;
	c->allow_csd = 0;
	c->force_fakemaximize = 0;
	c->force_tiled_state = 1;
	c->force_tearing = 0;
	c->allow_shortcuts_inhibit = SHORTCUTS_INHIBIT_ENABLE;
	c->indleinhibit_when_focus = 0;
	c->scroller_proportion_single = 0.0f;
	c->float_geom.width = 0;
	c->float_geom.height = 0;
	c->float_geom.x = 0;
	c->float_geom.y = 0;
	c->stack_proportion = 0.0f;
	memset(c->oldmonname, 0, sizeof(c->oldmonname));
	memcpy(c->opacity_animation.initial_border_color, config.bordercolor,
		   sizeof(c->opacity_animation.initial_border_color));
	memcpy(c->opacity_animation.current_border_color, config.bordercolor,
		   sizeof(c->opacity_animation.current_border_color));
	c->opacity_animation.initial_opacity = c->unfocused_opacity;
	c->opacity_animation.current_opacity = c->unfocused_opacity;
}

void
mapnotify(struct wl_listener *listener, void *data) {
	
	Client *at_client = NULL;
	Client *c = wl_container_of(listener, c, map);
	int32_t i = 0;
	
	c->scene = client_surface(c)->data = wlr_scene_tree_create(layers[LyrTile]);
	wlr_scene_node_set_enabled(&c->scene->node, c->type != XDGShell);
	c->scene_surface =
		c->type == XDGShell
			? wlr_scene_xdg_surface_create(c->scene, c->surface.xdg)
			: wlr_scene_subsurface_tree_create(c->scene, client_surface(c));
	c->scene->node.data = c->scene_surface->node.data = c;

	client_get_geometry(c, &c->geom);

	if (client_is_x11(c))
		init_client_properties(c);

	if (client_is_unmanaged(c) || client_is_x11_popup(c)) {
		c->bw = 0;
		c->isnoborder = 1;
	} else {
		c->bw = config.borderpx;
	}

	if (client_should_global(c)) {
		c->isunglobal = 1;
	}

	c->geom.width += 2 * c->bw;
	c->geom.height += 2 * c->bw;

	if (client_is_unmanaged(c)) {
		
#ifdef XWAYLAND
		if (client_is_x11(c)) {
			fix_xwayland_unmanaged_coordinate(c);
			LISTEN(&c->surface.xwayland->events.set_geometry, &c->set_geometry,
				   setgeometrynotify);
		}
#endif
		wlr_scene_node_reparent(&c->scene->node, layers[LyrOverlay]);
		wlr_scene_node_set_position(&c->scene->node, c->geom.x, c->geom.y);
		if (client_wants_focus(c)) {
			focusclient(c, 1);
			exclusive_focus = c;
		}
		return;
	}

	/* splitindicator rects are only ever shown under the dwindle layout
	   with dwindle_manual_split=true. Allocate them on first use inside
	   apply_border() instead of paying the cost for every client. */
	for (i = 0; i < 2; i++) {
		c->splitindicator[i] = NULL;
	}

	/* droparea scene rect is allocated lazily on first drag (rare event).
	   Saves one wlr_scene_rect + damage tracking state per client. */
	c->droparea = NULL;

	c->border = wlr_scene_rect_create(
		c->scene, 0, 0, c->isurgent ? config.urgentcolor : config.bordercolor);
	wlr_scene_node_lower_to_bottom(&c->border->node);
	wlr_scene_node_set_position(&c->border->node, 0, 0);
	wlr_scene_rect_set_corner_radius(c->border, config.border_radius,
									 config.border_radius_location_default);
	wlr_scene_node_set_enabled(&c->border->node, true);

	c->shadow =
		wlr_scene_shadow_create(c->scene, 0, 0, config.border_radius,
								config.shadows_blur, config.shadowscolor);

	wlr_scene_node_lower_to_bottom(&c->shadow->node);
	wlr_scene_node_set_enabled(&c->shadow->node, true);

	if (config.new_is_master && selmon && !is_scroller_layout(selmon))
		
		wl_list_insert(&clients, &c->link);
	else if (selmon && is_scroller_layout(selmon) &&
			 selmon->visible_scroll_tiling_clients > 0) {

		if (selmon->sel && ISSCROLLTILED(selmon->sel) &&
			VISIBLEON(selmon->sel, selmon)) {
			at_client = scroll_get_stack_tail_client(selmon->sel);
		} else {
			at_client = center_tiled_select(selmon);
		}

		if (at_client) {
			wl_list_insert(&at_client->link, &c->link);
		} else {
			wl_list_insert(clients.prev, &c->link);
		}
	} else
		wl_list_insert(clients.prev, &c->link);

	wl_list_insert(&fstack, &c->flink);

	applyrules(c);

	if (!c->isfloating || c->force_tiled_state) {
		client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT |
								WLR_EDGE_RIGHT);
	}

	wlr_scene_node_for_each_buffer(&c->scene_surface->node,
								   iter_xdg_scene_buffers, c);

	setborder_color(c);

	c->is_pending_open_animation = true;
	resize(c, c->geom, 0);
	printstatus();

	/* Under the vertical scroller, a freshly opened window gets maximize-to-screen
	   toggled on it — done after the trailing resize so it isn't reverted. */
	if (!c->isfloating && c->mon && !c->ismaximizescreen &&
		c->mon->pertag->ltidxs[c->mon->pertag->curtag]->id == VERTICAL_SCROLLER)
		setmaximizescreen(c, 1);
}

void maximizenotify(struct wl_listener *listener, void *data) {

	Client *c = wl_container_of(listener, c, maximize);

	if (!c || !c->mon || c->iskilling || c->ignore_maximize)
		return;

	if (!client_is_x11(c) && !c->surface.xdg->initialized) {
		return;
	}

	if (client_request_maximize(c, data)) {
		setmaximizescreen(c, 1);
	} else {
		setmaximizescreen(c, 0);
	}
}

void unminimize(Client *c) {
	if (c && c->is_in_scratchpad && c->is_scratchpad_show) {
		client_pending_minimized_state(c, 0);
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		setborder_color(c);
		return;
	}

	if (c && c->isminimized) {
		show_hide_client(c);
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		setborder_color(c);
		arrange(c->mon, false, false);
		return;
	}
}

void set_minimized(Client *c) {

	if (!c || !c->mon)
		return;

	c->isglobal = 0;
	c->oldtags = c->mon->tagset[c->mon->seltags];
	c->mini_restore_tag = c->tags;
	c->tags = 0;
	client_pending_minimized_state(c, 1);
	c->is_in_scratchpad = 1;
	c->is_scratchpad_show = 0;
	focusclient(focustop(selmon), 1);
	arrange(c->mon, false, false);

	CLIENT_SET_FOREIGN_ACTIVATED(c, false);

	wl_list_remove(&c->link);
	wl_list_insert(clients.prev, &c->link);
}

void minimizenotify(struct wl_listener *listener, void *data) {

	Client *c = wl_container_of(listener, c, minimize);

	if (!c || !c->mon || c->iskilling || c->isminimized)
		return;

	if (client_request_minimize(c, data) && !c->ignore_minimize) {
		if (!c->isminimized)
			set_minimized(c);
		client_set_minimized(c, true);
	} else {
		if (c->isminimized)
			unminimize(c);
		client_set_minimized(c, false);
	}
}

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

		/* Cycler eats motion: update the hover index, then bail out
		   before any sloppy-focus / pointer routing fires (the overlay
		   sits on LyrFadeOut which xytonode skips, so otherwise the
		   cursor would silently re-focus whatever is under the
		   thumbnails). */
		if (window_cycler.active) {
			window_cycler_hover_at(cursor->x, cursor->y);
			return;
		}

		/* Track pointer velocity (EMA) while dragging, for the momentum
		   hand-off on release. Reset after an idle gap so a slow drag start
		   does not inherit a stale fling. */
		if (config.animation_momentum &&
			(cursor_mode == CurMove || cursor_mode == CurResize)) {
			uint32_t vnow = frame_now_ms();
			double vdt = drag_last_ms ? (double)(vnow - drag_last_ms) / 1000.0 : 0;
			if (vdt > 0.001 && vdt < 0.1) {
				double ivx = (cursor->x - drag_last_x) / vdt;
				double ivy = (cursor->y - drag_last_y) / vdt;
				drag_vel_x = drag_vel_x * 0.6 + ivx * 0.4;
				drag_vel_y = drag_vel_y * 0.6 + ivy * 0.4;
			} else {
				drag_vel_x = drag_vel_y = 0.0;
			}
			drag_last_x = cursor->x;
			drag_last_y = cursor->y;
			drag_last_ms = vnow;
		}

		/* Throttle idle-notify D-Bus signal to once per 250 ms.
		   Continuous cursor motion was flooding the bus and adding latency
		   on every event. Activity is still reported promptly enough for
		   any idle-watcher (250 ms is well under typical inhibit windows). */
		{
			static uint32_t last_idle_notify_ms = 0;
			uint32_t now_ms_motion = frame_now_ms();
			if (now_ms_motion - last_idle_notify_ms >= 250) {
				wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);
				last_idle_notify_ms = now_ms_motion;
			}
		}

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

void pointerfocus(Client *c, struct wlr_surface *surface, double sx, double sy,
				  uint32_t time) {
	struct timespec now;

	if (config.sloppyfocus && !start_drag_window && c && time && c->scene &&
		c->scene->node.enabled && !c->animation.tagining &&
		(surface != seat->pointer_state.focused_surface) &&
		!client_is_unmanaged(c) && VISIBLEON(c, c->mon))
		focusclient(c, 0);

	if (!surface) {
		wlr_seat_pointer_notify_clear_focus(seat);
		return;
	}

	if (!time) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		time = now.tv_sec * 1000 + now.tv_nsec / 1000000;
	}

	if (!c || !c->mon || !c->mon->isoverview) {
		
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
	}

	wlr_seat_pointer_notify_motion(seat, time, sx, sy);
}

void printstatus(void) { wl_signal_emit(&lemon_print_status, NULL); }

void powermgrsetmode(struct wl_listener *listener, void *data) {
	struct wlr_output_power_v1_set_mode_event *event = data;
	struct wlr_output_state state = {0};
	Monitor *m = event->output->data;

	if (!m)
		return;

	wlr_output_state_set_enabled(&state, event->mode);
	wlr_output_commit_state(m->wlr_output, &state);

	m->asleep = !event->mode;
	updatemons(NULL, NULL);
}

void quitsignal(int32_t signo) { quit(NULL); }

LEMON_HOT void scene_buffer_apply_opacity(struct wlr_scene_buffer *buffer, int32_t sx,
								int32_t sy, void *data) {
	wlr_scene_buffer_set_opacity(buffer, *(double *)data);
}

LEMON_HOT void client_set_opacity(Client *c, double opacity) {
	opacity = CLAMP_FLOAT(opacity, 0.0f, 1.0f);
	wlr_scene_node_for_each_buffer(&c->scene_surface->node,
								   scene_buffer_apply_opacity, &opacity);
}

void monitor_stop_skip_frame_timer(Monitor *m) {
	if (m->skip_frame_timeout)
		wl_event_source_timer_update(m->skip_frame_timeout, 0);
	m->skiping_frame = false;
	m->resizing_count_pending = 0;
	m->resizing_count_current = 0;
}

static int monitor_skip_frame_timeout_callback(void *data) {
	Monitor *m = data;
	Client *c, *tmp;

	wl_list_for_each_safe(c, tmp, &clients, link) { c->configure_serial = 0; }

	monitor_stop_skip_frame_timer(m);
	wlr_output_schedule_frame(m->wlr_output);

	return 1;
}

void monitor_check_skip_frame_timeout(Monitor *m) {
	if (m->skiping_frame &&
		m->resizing_count_pending == m->resizing_count_current) {
		return;
	}

	if (m->skip_frame_timeout) {
		m->resizing_count_current = m->resizing_count_pending;
		m->skiping_frame = true;
		wl_event_source_timer_update(m->skip_frame_timeout, 100);
	}
}

/* Return true when all detected AC adapters report offline (system on battery). */
static bool detect_on_battery(void) {
	DIR *d = opendir("/sys/class/power_supply");
	if (!d)
		return false;
	bool found_ac = false;
	bool ac_online = false;
	struct dirent *e;
	while ((e = readdir(d))) {
		if (strncmp(e->d_name, "AC", 2) != 0 &&
		    strncmp(e->d_name, "ADP", 3) != 0)
			continue;
		char path[320];
		snprintf(path, sizeof(path), "/sys/class/power_supply/%s/online",
		         e->d_name);
		FILE *fp = fopen(path, "re");
		if (!fp)
			continue;
		int v = 0;
		if (fscanf(fp, "%d", &v) == 1) {
			found_ac = true;
			if (v)
				ac_online = true;
		}
		fclose(fp);
	}
	closedir(d);
	if (!found_ac)
		return false;
	return !ac_online;
}

/* Widen the kernel timer slack on battery so the scheduler coalesces wakeups
   and the CPU reaches deeper C-states; restore the tight default on AC. */
static void apply_battery_timer_slack(bool battery) {
	unsigned long ns = battery && config.battery_timer_slack_ms > 0
						   ? (unsigned long)config.battery_timer_slack_ms * 1000000UL
						   : 50000UL; /* kernel default ~50 us */
	prctl(PR_SET_TIMERSLACK, ns, 0, 0, 0);
}

/* PM_QOS request via /dev/cpu_dma_latency: bounds the max DMA wakeup latency
   the kernel may impose. Holding the fd open with a low value pins the CPUs
   in shallow C-states, cutting IRQ-to-userspace wakeup from ~100us to ~1us
   on idle cores — felt as steadier input/frame timing. Costs battery, so we
   relax to a higher tolerance on battery. -1 disables the request entirely. */
static int cpu_dma_latency_fd = -1;
static void apply_cpu_dma_latency(bool battery) {
	if (config.cpu_dma_latency_us < 0) {
		if (cpu_dma_latency_fd >= 0) {
			close(cpu_dma_latency_fd);
			cpu_dma_latency_fd = -1;
		}
		return;
	}
	if (cpu_dma_latency_fd < 0) {
		cpu_dma_latency_fd =
			open("/dev/cpu_dma_latency", O_WRONLY | O_CLOEXEC);
		if (cpu_dma_latency_fd < 0)
			return; /* not available (no permission, no kernel support) */
	}
	int32_t v = config.cpu_dma_latency_us;
	if (battery)
		v += 1000; /* allow deeper C-states on battery */
	if (write(cpu_dma_latency_fd, &v, sizeof(v)) != (ssize_t)sizeof(v)) {
		/* Write failure leaves the previous value (or unset). Not fatal. */
	}
}

/* Re-read AC state periodically; reschedule self. On an AC<->battery
   transition, widen/restore the timer slack and re-arm idle timers so the
   battery idle timeout takes effect immediately. Also nudges glibc to release
   freed heap pages back to the kernel — long sessions accumulate freed-but-
   resident memory otherwise. */
static int battery_poll_callback(void *data) {
	(void)data;
	bool was_on_battery = on_battery;
	on_battery = detect_on_battery();
	if (on_battery != was_on_battery) {
		apply_battery_timer_slack(on_battery);
		apply_cpu_dma_latency(on_battery);
		reset_idle_timers();
	}
	malloc_trim(0);
	wl_event_source_timer_update(battery_poll_source, 10000);
	return 0;
}

/* Check whether any idle inhibitor's surface belongs to this client.
   Inhibited clients (typical case: a media player while playing) are
   never sent xdg suspended by the idle hibernation scanner. */
static bool client_has_idle_inhibitor(Client *c) {
	if (!c || !idle_inhibit_mgr || !client_surface(c))
		return false;
	struct wlr_idle_inhibitor_v1 *inh = NULL;
	wl_list_for_each(inh, &idle_inhibit_mgr->inhibitors, link) {
		Client *ic = NULL;
		toplevel_from_wlr_surface(inh->surface, &ic, NULL);
		if (ic == c)
			return true;
	}
	return false;
}

/* Periodic scan: send xdg suspended to every xdg client that has been
   unfocused for more than client_hibernate_idle_secs seconds and is
   not holding an idle inhibitor. Most toolkits stop animating /
   rendering on suspended -- the window keeps its buffer (no flash on
   focus return) but drops to ~0% CPU. */
static struct wl_event_source *client_hibernate_source = NULL;
static int client_hibernate_scan_callback(void *data) {
	(void)data;
	int32_t threshold = config.client_hibernate_idle_secs;
	if (threshold > 0) {
		uint32_t now = (uint32_t)get_now_in_ms();
		uint32_t cutoff = (uint32_t)threshold * 1000u;
		Client *c = NULL;
		wl_list_for_each(c, &clients, link) {
			if (!c || c->iskilling || client_is_x11(c) ||
				client_is_unmanaged(c))
				continue;
			if (selmon && selmon->sel == c)
				continue;
			if (c->isminimized)
				continue;
			if (client_has_idle_inhibitor(c))
				continue;
			bool idle = (now - c->last_active_ms) > cutoff;
			if (idle && !c->suspended_sent)
				client_set_suspended(c, true);
			else if (!idle && c->suspended_sent)
				client_set_suspended(c, false);
		}
	}
	wl_event_source_timer_update(client_hibernate_source, 15000);
	return 0;
}

/* Deferred frame schedule used to cap animation FPS on battery. */
static int battery_frame_throttle_callback(void *data) {
	Monitor *m = data;
	if (m->wlr_output && m->wlr_output->enabled && allow_frame_scheduling)
		wlr_output_schedule_frame(m->wlr_output);
	return 0;
}

/* Decide whether m's scene output must be committed this frame. Commit when
   the scene has pending damage, or when a legacy wlr_screencopy_v1 capture is
   in flight: those frame requests do not reliably mark the vanilla wlroots
   scene as needing a frame, so without this an idle screen would never satisfy
   them. The modern ext-image-copy-capture path drives needs_frame itself. */
static bool output_should_commit(Monitor *m) {
	if (wlr_scene_output_needs_frame(m->scene_output))
		return true;
	if (screencopy_mgr && !wl_list_empty(&screencopy_mgr->frames))
		return true;
	return false;
}

/* draw every layer-shell surface on m; true if any wants another frame */
static bool render_layer_surfaces(Monitor *m) {
	LayerSurface *l = NULL, *tmpl = NULL;
	bool more = false;
	for (int32_t i = 0; i < LENGTH(m->layers); i++) {
		wl_list_for_each_safe(l, tmpl, &m->layers[i], link) {
			if (layer_draw_frame(l))
				more = true;
		}
	}
	return more;
}

/* drive fade-out animations for dying clients and layers; true if still animating */
static bool render_fadeouts(void) {
	Client *c = NULL, *tmp = NULL;
	LayerSurface *l = NULL, *tmpl = NULL;
	bool more = false;
	wl_list_for_each_safe(c, tmp, &fadeout_clients, fadeout_link) {
		if (client_draw_fadeout_frame(c))
			more = true;
	}
	wl_list_for_each_safe(l, tmpl, &fadeout_layers, fadeout_link) {
		if (layer_draw_fadeout_frame(l))
			more = true;
	}
	return more;
}

/* draw clients owned by m. Sets *skip when a no-animation configure
   round-trip means the commit must be deferred this frame. Returns true
   if any client wants another frame. */
static bool render_clients(Monitor *m, bool *skip) {
	Client *c = NULL, *tmp = NULL;
	bool more = false;
	*skip = false;
	wl_list_for_each_safe(c, tmp, &clients, link) {
		if (LEMON_LIKELY(c->mon != m))
			continue;
		if (client_draw_frame(c))
			more = true;
		if (!config.animations && !grabc && c->configure_serial) {
			monitor_check_skip_frame_timeout(m);
			*skip = true;
			return more;
		}
	}
	if (m->skiping_frame) {
		monitor_stop_skip_frame_timer(m);
	}
	return more;
}

/* fade the overview dim backdrop toward its target; true if still fading */
static bool render_overview_dim(Monitor *m) {
	if (!m->ov_dim)
		return false;

	float target =
		(m->isoverview && config.overview_dim) ? config.overview_dim_alpha : 0.0f;

	uint32_t now = frame_now_ms();
	uint32_t dt = now - m->ov_dim_last_ms;
	m->ov_dim_last_ms = now;
	if (dt > 50)
		dt = 50; /* clamp after an idle gap so the fade never jumps */

	if (m->ov_dim_cur != target) {
		float step = (float)dt / 130.0f; /* ~130 ms fade */
		if (m->ov_dim_cur < target)
			m->ov_dim_cur = fminf(m->ov_dim_cur + step, target);
		else
			m->ov_dim_cur = fmaxf(m->ov_dim_cur - step, target);
		if (fabsf(m->ov_dim_cur - target) < 0.004f)
			m->ov_dim_cur = target;
	}

	bool visible = m->ov_dim_cur > 0.004f;
	wlr_scene_node_set_enabled(&m->ov_dim->node, visible);
	if (visible) {
		wlr_scene_node_set_position(&m->ov_dim->node, m->m.x, m->m.y);
		wlr_scene_rect_set_size(m->ov_dim, m->m.width, m->m.height);
		wlr_scene_rect_set_color(m->ov_dim,
								 (float[4]){0, 0, 0, m->ov_dim_cur});
	}

	return m->ov_dim_cur != target;
}

/* schedule the next output frame, honoring the on-battery throttle */
static void schedule_next_frame(Monitor *m) {
	if (on_battery && m->battery_frame_throttle) {
		uint32_t interval = config.battery_fps > 0
								? 1000u / (uint32_t)config.battery_fps
								: BATTERY_ANIM_INTERVAL_MS;
		uint32_t now_ms = frame_now_ms();
		uint32_t elapsed = now_ms - m->last_anim_schedule_ms;
		if (elapsed < interval) {
			wl_event_source_timer_update(m->battery_frame_throttle,
										 interval - elapsed);
			return;
		}
	}
	m->last_anim_schedule_ms = frame_now_ms();
	wlr_output_schedule_frame(m->wlr_output);
}

/* Draw all surfaces, commit, schedule next frame, and update the render-time EMA.
   Split from rendermon so the late-latch deadline timer can drive it too. */
LEMON_HOT void do_rendermon(Monitor *m) {
	struct timespec now;
	bool need_more_frames = false;
	bool skip = false;

	frame_clock_begin();

	struct timespec t0;
	bool timed = config.debug_frametime || config.late_latch;
	if (LEMON_UNLIKELY(timed))
		clock_gettime(CLOCK_MONOTONIC, &t0);

	bool frame_allow_tearing = check_tearing_frame_allow(m);

	bool layers_more = render_layer_surfaces(m);
	bool fadeouts_more = render_fadeouts();
	bool clients_more = render_clients(m, &skip);
	bool dim_more = render_overview_dim(m);
	need_more_frames = layers_more || fadeouts_more || clients_more || dim_more;

	if (!skip) {
		if (config.allow_tearing && frame_allow_tearing) {
			apply_tear_state(m);
		} else if (output_should_commit(m)) {
			wlr_scene_output_commit(m->scene_output, NULL);
		}
	}

	frame_clock_now_timespec(&now);
	wlr_scene_output_send_frame_done(m->scene_output, &now);

	if (LEMON_UNLIKELY(need_more_frames && allow_frame_scheduling))
		schedule_next_frame(m);

	if (LEMON_UNLIKELY(timed)) {
		struct timespec t1;
		clock_gettime(CLOCK_MONOTONIC, &t1);
		int64_t us = (t1.tv_sec - t0.tv_sec) * 1000000 +
					 (t1.tv_nsec - t0.tv_nsec) / 1000;
		/* EMA (1/8 weight) feeds the late-latch deadline computation. */
		m->render_ema_us = (uint32_t)((m->render_ema_us * 7 + us) / 8);
		if (LEMON_UNLIKELY(config.debug_frametime)) {
			int64_t budget_us = m->wlr_output->refresh > 0
									? 1000000000LL / m->wlr_output->refresh
									: 16666;
			if (us > budget_us)
				wlr_log(WLR_INFO, "%s: frame %lldus over %lldus budget",
						m->wlr_output->name, (long long)us, (long long)budget_us);
		}
	}

	frame_clock_end();
}

/* Late-latch deadline fired: render now, just before the next vblank. */
static int render_deadline_callback(void *data) {
	Monitor *m = data;
	m->render_pending = false;
	if (LEMON_UNLIKELY(session && !session->active))
		return 0;
	if (LEMON_UNLIKELY(!m->wlr_output->enabled || !allow_frame_scheduling))
		return 0;
	do_rendermon(m);
	return 0;
}

/* per-monitor frame callback: render immediately, or defer to just before the
   next vblank when late input latching is enabled (config-gated, default off). */
LEMON_HOT void rendermon(struct wl_listener *listener, void *data) {
	Monitor *m = wl_container_of(listener, m, frame);

	if (LEMON_UNLIKELY(session && !session->active))
		return;

	if (LEMON_UNLIKELY(!m->wlr_output->enabled || !allow_frame_scheduling))
		return;

	if (LEMON_UNLIKELY(config.late_latch && !config.allow_tearing &&
					   m->render_deadline && !m->render_pending &&
					   m->wlr_output->refresh > 0)) {
		int64_t period_us = 1000000000LL / m->wlr_output->refresh;
		int64_t delay_us =
			period_us - (int64_t)m->render_ema_us - config.latency_margin_us;
		if (delay_us > 1000) {
			m->render_pending = true;
			wl_event_source_timer_update(m->render_deadline,
										 (uint32_t)(delay_us / 1000));
			return;
		}
	}

	do_rendermon(m);
}

void requestdecorationmode(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_decoration_mode);
	struct wlr_xdg_toplevel_decoration_v1 *deco = data;

	if (c->surface.xdg->initialized) {
		
		enum wlr_xdg_toplevel_decoration_v1_mode requested_mode =
			deco->requested_mode;

		if (!c->allow_csd) {
			requested_mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
		}

		wlr_xdg_toplevel_decoration_v1_set_mode(c->decoration, requested_mode);
	}
}

static void requestdrmlease(struct wl_listener *listener, void *data) {
	struct wlr_drm_lease_request_v1 *req = data;
	struct wlr_drm_lease_v1 *lease = wlr_drm_lease_request_v1_grant(req);

	if (!lease) {
		wlr_log(WLR_ERROR, "Failed to grant lease request");
		wlr_drm_lease_request_v1_reject(req);
	}
}

void requeststartdrag(struct wl_listener *listener, void *data) {
	struct wlr_seat_request_start_drag_event *event = data;

	if (wlr_seat_validate_pointer_grab_serial(seat, event->origin,
											  event->serial))
		wlr_seat_start_pointer_drag(seat, event->drag, event->serial);
	else
		wlr_data_source_destroy(event->drag->source);
}

void setborder_color(Client *c) {
	if (!c || !c->mon)
		return;

	float *border_color = get_border_color(c);
	memcpy(c->opacity_animation.target_border_color, border_color,
		   sizeof(c->opacity_animation.target_border_color));
	client_set_border_color(c, border_color);
}

void exchange_two_client(Client *c1, Client *c2) {
	Monitor *tmp_mon = NULL;
	uint32_t tmp_tags;
	double master_inner_per = 0.0f;
	double master_mfact_per = 0.0f;
	double stack_inner_per = 0.0f;
	struct ScrollerStackNode *n1 = NULL;
	struct ScrollerStackNode *n2 = NULL;
	struct TagScrollerState *st1 = NULL;
	struct TagScrollerState *st2 = NULL;

	if (c1 == NULL || c2 == NULL || c1->mon == NULL || c2->mon == NULL ||
		(!config.exchange_cross_monitor && c1->mon != c2->mon)) {
		return;
	}

	master_inner_per = c1->master_inner_per;
	master_mfact_per = c1->master_mfact_per;
	stack_inner_per = c1->stack_inner_per;
	c1->master_inner_per = c2->master_inner_per;
	c1->master_mfact_per = c2->master_mfact_per;
	c1->stack_inner_per = c2->stack_inner_per;
	c2->master_inner_per = master_inner_per;
	c2->master_mfact_per = master_mfact_per;
	c2->stack_inner_per = stack_inner_per;

	bool c1_scroller = c1->mon && is_scroller_layout(c1->mon);
	bool c2_scroller = c2->mon && is_scroller_layout(c2->mon);
	Monitor *m1 = c1->mon;
	Monitor *m2 = c2->mon;
	uint32_t tag1 = m1->pertag->curtag;
	uint32_t tag2 = m2->pertag->curtag;

	if (c1_scroller) {
		st1 = ensure_scroller_state(m1, tag1);
		n1 = find_scroller_node(st1, c1);
	}

	if (c2_scroller) {
		st2 = ensure_scroller_state(m2, tag2);
		n2 = find_scroller_node(st2, c2);
	}

	if (!n1 || !n2)
		goto exchange_common;

	if (n1 && n2) {

		if (m1 != m2 && (n1->prev_in_stack || n2->prev_in_stack ||
						 n1->next_in_stack || n2->next_in_stack))
			return;

		struct ScrollerStackNode *head1 = n1;
		while (head1->prev_in_stack)
			head1 = head1->prev_in_stack;
		struct ScrollerStackNode *head2 = n2;
		while (head2->prev_in_stack)
			head2 = head2->prev_in_stack;

		if (head1 == head2) {
			float tmp_scroller = n1->scroller_proportion;
			float tmp_stack = n1->stack_proportion;
			n1->scroller_proportion = n2->scroller_proportion;
			n1->stack_proportion = n2->stack_proportion;
			n2->scroller_proportion = tmp_scroller;
			n2->stack_proportion = tmp_stack;

			struct ScrollerStackNode *p1 = n1->prev_in_stack;
			struct ScrollerStackNode *next1 = n1->next_in_stack;
			struct ScrollerStackNode *p2 = n2->prev_in_stack;
			struct ScrollerStackNode *next2 = n2->next_in_stack;

			if (n1->next_in_stack == n2) {
				n1->next_in_stack = next2;
				n2->prev_in_stack = p1;
				n1->prev_in_stack = n2;
				n2->next_in_stack = n1;
				if (p1)
					p1->next_in_stack = n2;
				if (next2)
					next2->prev_in_stack = n1;
			} else if (n2->next_in_stack == n1) {
				n2->next_in_stack = next1;
				n1->prev_in_stack = p2;
				n2->prev_in_stack = n1;
				n1->next_in_stack = n2;
				if (p2)
					p2->next_in_stack = n1;
				if (next1)
					next1->prev_in_stack = n2;
			} else {
				if (p1)
					p1->next_in_stack = n2;
				if (next1)
					next1->prev_in_stack = n2;
				if (p2)
					p2->next_in_stack = n1;
				if (next2)
					next2->prev_in_stack = n1;
				n1->prev_in_stack = p2;
				n1->next_in_stack = next2;
				n2->prev_in_stack = p1;
				n2->next_in_stack = next1;
			}

			sync_scroller_state_to_clients(m1, tag1);
			arrange(m1, false, false);
		} else {
			
			if (n1 != head1 || n2 != head2) {
				
				exchange_two_client(head1->client, head2->client);
				return;
			}
		}
	}

exchange_common:

	if (m1 != m2 && ((n1 && n1->prev_in_stack) || (n2 && n2->prev_in_stack) ||
					 (n1 && n1->next_in_stack) || (n2 && n2->next_in_stack)))
		return;

	struct wl_list *tmp1_prev = c1->link.prev;
	struct wl_list *tmp2_prev = c2->link.prev;
	struct wl_list *tmp1_next = c1->link.next;
	struct wl_list *tmp2_next = c2->link.next;

	if (c1->link.next == &c2->link) {
		c1->link.next = c2->link.next;
		c1->link.prev = &c2->link;
		c2->link.next = &c1->link;
		c2->link.prev = tmp1_prev;
		tmp1_prev->next = &c2->link;
		tmp2_next->prev = &c1->link;
	} else if (c2->link.next == &c1->link) {
		c2->link.next = c1->link.next;
		c2->link.prev = &c1->link;
		c1->link.next = &c2->link;
		c1->link.prev = tmp2_prev;
		tmp2_prev->next = &c1->link;
		tmp1_next->prev = &c2->link;
	} else {
		c2->link.next = tmp1_next;
		c2->link.prev = tmp1_prev;
		c1->link.next = tmp2_next;
		c1->link.prev = tmp2_prev;
		tmp1_prev->next = &c2->link;
		tmp1_next->prev = &c2->link;
		tmp2_prev->next = &c1->link;
		tmp2_next->prev = &c1->link;
	}

	const Layout *layout1 = c1->mon->pertag->ltidxs[c1->mon->pertag->curtag];

	const Layout *layout2 = c2->mon->pertag->ltidxs[c2->mon->pertag->curtag];

	if (c1->mon != c2->mon) {

		if (layout1->id == DWINDLE && layout2->id == DWINDLE) {
			DwindleNode **c1_root =
				&m1->pertag->dwindle_root[m1->pertag->curtag];
			DwindleNode *c1node = dwindle_find_leaf(*c1_root, c1);

			DwindleNode **c2_root =
				&m2->pertag->dwindle_root[m2->pertag->curtag];
			DwindleNode *c2node = dwindle_find_leaf(*c2_root, c2);

			if (c1node)
				c1node->client = c2;

			if (c2node)
				c2node->client = c1;
		}

		tmp_mon = c2->mon;
		tmp_tags = c2->tags;
		c2->mon = c1->mon;
		c1->mon = tmp_mon;
		c2->tags = c1->tags;
		c1->tags = tmp_tags;

		arrange(c1->mon, false, false);
		arrange(c2->mon, false, false);
	} else {
		if (layout1->id == DWINDLE && layout2->id == DWINDLE) {
			dwindle_swap_clients(
				&c1->mon->pertag->dwindle_root[c1->mon->pertag->curtag], c1,
				c2);
		}
		arrange(c1->mon, false, false);
	}

	wl_list_remove(&c2->flink);
	wl_list_insert(&c1->flink, &c2->flink);
}

void set_activation_env() {
	if (!getenv("DBUS_SESSION_BUS_ADDRESS")) {
		wlr_log(WLR_INFO, "Not updating dbus execution environment: "
						  "DBUS_SESSION_BUS_ADDRESS not set");
		return;
	}

	wlr_log(WLR_INFO, "Updating dbus execution environment");

	char *env_keys = join_strings(env_vars, " ");

	const char *arg1 = env_keys;
	char *cmd1 = string_printf("dbus-update-activation-environment %s", arg1);
	if (!cmd1) {
		wlr_log(WLR_ERROR, "Failed to allocate command string");
		goto cleanup;
	}
	spawn(&(Arg){.v = cmd1});
	free(cmd1);

	const char *action = "import-environment";
	char *cmd2 = string_printf("systemctl --user %s %s", action, env_keys);
	if (!cmd2) {
		wlr_log(WLR_ERROR, "Failed to allocate command string");
		goto cleanup;
	}
	spawn(&(Arg){.v = cmd2});
	free(cmd2);

cleanup:
	free(env_keys);
}

void
run(char *startup_cmd) {

	set_env();

	const char *socket = wl_display_add_socket_auto(dpy);
	if (!socket)
		die("startup: display_add_socket_auto");
	setenv("WAYLAND_DISPLAY", socket, 1);

	if (!wlr_backend_start(backend))
		die("startup: backend_start");

	if (startup_cmd) {
		int32_t piperw[2];
		if (pipe(piperw) < 0)
			die("startup: pipe:");
		if ((child_pid = fork()) < 0)
			die("startup: fork:");
		if (child_pid == 0) {
			setsid();
			dup2(piperw[0], STDIN_FILENO);
			close(piperw[0]);
			close(piperw[1]);
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, NULL);
			die("startup: execl:");
		}
		dup2(piperw[1], STDOUT_FILENO);
		close(piperw[1]);
		close(piperw[0]);
	}

	if (fd_set_nonblock(STDOUT_FILENO) < 0)
		close(STDOUT_FILENO);

	printstatus();

	selmon = xytomon(cursor->x, cursor->y);

	wlr_cursor_warp_closest(cursor, NULL, cursor->x, cursor->y);
	wlr_cursor_set_xcursor(cursor, cursor_mgr, "left_ptr");
	handlecursoractivity();

	set_activation_env();

	run_exec();
	run_exec_once();

	/* Warm-start D-Bus activation of xdg-desktop-portal so that the first
	   GTK/Qt app launched does not pay the portal cold-start latency. The
	   call is fire-and-forget; if dbus-send is missing or the portal is
	   absent, nothing breaks. */
	Arg portal_warm = {.v = "dbus-send --session --print-reply=literal "
	                        "--dest=org.freedesktop.portal.Desktop "
	                        "/org/freedesktop/portal/desktop "
	                        "org.freedesktop.DBus.Peer.Ping "
	                        ">/dev/null 2>&1 || true"};
	spawn_shell(&portal_warm);

	reload_ready = true;
	wl_display_run(dpy);
}

void setcursor(struct wl_listener *listener, void *data) {
	
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	
	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	
	if (event->seat_client == seat->pointer_state.focused_client) {
		
		if (last_cursor.surface &&
			last_cursor_surface_destroy_listener.link.prev != NULL)
			wl_list_remove(&last_cursor_surface_destroy_listener.link);

		last_cursor.shape = 0;
		last_cursor.surface = event->surface;
		last_cursor.hotspot_x = event->hotspot_x;
		last_cursor.hotspot_y = event->hotspot_y;

		if (event->surface)
			wl_signal_add(&event->surface->events.destroy,
						  &last_cursor_surface_destroy_listener);

		if (!cursor_hidden)
			wlr_cursor_set_surface(cursor, event->surface, event->hotspot_x,
								   event->hotspot_y);
	}
}

void
setfloating(Client *c, int32_t floating) {

	Client *fc = NULL;
	struct wlr_box target_box;
	int32_t old_floating_state = c->isfloating;
	c->isfloating = floating;
	bool window_size_outofrange = false;

	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	target_box = c->geom;

	if (floating == 1 && c != grabc) {

		if (c->isfullscreen) {
			client_pending_fullscreen_state(c, 0);
			client_set_fullscreen(c, 0);
		}

		client_pending_maximized_state(c, 0);
		exit_scroller_stack(c);

		if (!client_is_x11(c) && !c->iscustompos)
			target_box =
				setclient_coordinate_center(c, c->mon, target_box, 0, 0);
		else
			target_box = c->geom;

		if (c->float_geom.width > 0 && c->float_geom.height > 0) {
			if (c->mon &&
				c->float_geom.width >= c->mon->w.width - config.gappoh) {
				c->float_geom.width = c->mon->w.width * 0.9;
				window_size_outofrange = true;
			}
			if (c->mon &&
				c->float_geom.height >= c->mon->w.height - config.gappov) {
				c->float_geom.height = c->mon->w.height * 0.9;
				window_size_outofrange = true;
			}
			if (window_size_outofrange) {
				c->float_geom =
					setclient_coordinate_center(c, c->mon, c->float_geom, 0, 0);
			}
			resize(c, c->float_geom, 0);
		} else {
			resize(c, target_box, 0);
		}

		c->need_float_size_reduce = 0;
	} else if (c->isfloating && c == grabc) {
		c->need_float_size_reduce = 0;
	} else {
		c->need_float_size_reduce = 1;
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		
		wl_list_for_each(fc, &clients,
						 link) if (fc && fc != c && VISIBLEON(fc, c->mon) &&
								   c->tags & fc->tags && ISFULLSCREEN(fc) &&
								   old_floating_state) {
			clear_fullscreen_flag(fc);
		}
	}

	if (c->isoverlay) {
		wlr_scene_node_reparent(&c->scene->node, layers[LyrOverlay]);
	} else if (client_should_overtop(c) && c->isfloating) {
		wlr_scene_node_reparent(&c->scene->node, layers[LyrTop]);
	} else {
		wlr_scene_node_reparent(&c->scene->node,
								layers[c->isfloating ? LyrTop : LyrTile]);
	}

	if (!c->isfloating && old_floating_state &&
		(c->old_stack_inner_per > 0.0f || c->old_master_inner_per > 0.0f)) {
		restore_size_per(c->mon, c);
	}

	if (c->isfloating && !old_floating_state) {
		save_old_size_per(c->mon);
	}

	if (!c->force_fakemaximize)
		client_set_maximized(c, false);

	if (!c->isfloating || c->force_tiled_state) {
		client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT |
								WLR_EDGE_RIGHT);
	} else {
		client_set_tiled(c, WLR_EDGE_NONE);
	}

	arrange(c->mon, false, false);

	if (!c->isfloating) {
		c->old_master_inner_per = c->master_inner_per;
		c->old_stack_inner_per = c->stack_inner_per;
	}

	setborder_color(c);
	printstatus();
}

void reset_maximizescreen_size(Client *c) {
	c->geom.x = c->mon->w.x + config.gappoh;
	c->geom.y = c->mon->w.y + config.gappov;
	c->geom.width = c->mon->w.width - 2 * config.gappoh;
	c->geom.height = c->mon->w.height - 2 * config.gappov;
	resize(c, c->geom, 0);
}

void exit_scroller_stack(Client *c) {
	if (!c || !c->mon)
		return;

	uint32_t tag = c->mon->pertag->curtag;
	struct TagScrollerState *st = c->mon->pertag->scroller_state[tag];
	if (st) {
		struct ScrollerStackNode *n = find_scroller_node(st, c);
		if (n) {
			scroller_node_remove(st, n);
			return;
		}
	}
}

void setmaximizescreen(Client *c, int32_t maximizescreen) {
	struct wlr_box maximizescreen_box;
	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	if (c->mon->isoverview)
		return;

	int32_t old_maximizescreen_state = c->ismaximizescreen;
	client_pending_maximized_state(c, maximizescreen);

	if (maximizescreen) {

		if (c->isfullscreen) {
			client_pending_fullscreen_state(c, 0);
			client_set_fullscreen(c, 0);
		}

		exit_scroller_stack(c);

		maximizescreen_box.x = c->mon->w.x + config.gappoh;
		maximizescreen_box.y = c->mon->w.y + config.gappov;
		maximizescreen_box.width = c->mon->w.width - 2 * config.gappoh;
		maximizescreen_box.height = c->mon->w.height - 2 * config.gappov;
		wlr_scene_node_raise_to_top(&c->scene->node);
		if (!is_scroller_layout(c->mon) || c->isfloating)
			resize(c, maximizescreen_box, 0);
	} else {
		c->bw = c->isnoborder ? 0 : config.borderpx;
		if (c->isfloating)
			setfloating(c, 1);
	}

	wlr_scene_node_reparent(&c->scene->node,
							layers[c->isfloating ? LyrTop : LyrTile]);
	if (!c->ismaximizescreen && old_maximizescreen_state) {
		restore_size_per(c->mon, c);
	}

	if (c->ismaximizescreen && !old_maximizescreen_state) {
		save_old_size_per(c->mon);
	}

	if (!c->force_fakemaximize && !c->ismaximizescreen) {
		client_set_maximized(c, false);
	} else if (!c->force_fakemaximize && c->ismaximizescreen) {
		client_set_maximized(c, true);
	}

	arrange(c->mon, false, false);
}

void setfakefullscreen(Client *c, int32_t fakefullscreen) {
	c->isfakefullscreen = fakefullscreen;
	if (!c->mon)
		return;

	if (c->isfullscreen)
		setfullscreen(c, 0);

	client_set_fullscreen(c, fakefullscreen);
}

void setfullscreen(Client *c, int32_t fullscreen)
{

	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	if (c->mon->isoverview)
		return;

	int32_t old_fullscreen_state = c->isfullscreen;
	c->isfullscreen = fullscreen;
	if (old_fullscreen_state != fullscreen) {
		c->focus_opacity_dirty = true;
		if (c->scene_surface)
			wlr_scene_node_for_each_buffer(&c->scene_surface->node,
										   iter_xdg_scene_buffers, c);
	}

	client_set_fullscreen(c, fullscreen);
	client_pending_fullscreen_state(c, fullscreen);

	if (fullscreen) {

		if (c->ismaximizescreen && !c->force_fakemaximize) {
			client_set_maximized(c, false);
		}

		client_pending_maximized_state(c, 0);

		exit_scroller_stack(c);
		c->isfakefullscreen = 0;

		c->bw = 0;
		wlr_scene_node_raise_to_top(&c->scene->node);
		if (!is_scroller_layout(c->mon) || c->isfloating)
			resize(c, c->mon->m, 1);
	} else {
		c->bw = c->isnoborder ? 0 : config.borderpx;
		if (c->isfloating)
			setfloating(c, 1);
	}

	if (c->isoverlay) {
		wlr_scene_node_reparent(&c->scene->node, layers[LyrOverlay]);
	} else if (client_should_overtop(c) && c->isfloating) {
		wlr_scene_node_reparent(&c->scene->node, layers[LyrTop]);
	} else {
		wlr_scene_node_reparent(
			&c->scene->node,
			layers[fullscreen || c->isfloating ? LyrTop : LyrTile]);
	}

	if (!c->isfullscreen && old_fullscreen_state) {
		restore_size_per(c->mon, c);
	}

	if (c->isfullscreen && !old_fullscreen_state) {
		save_old_size_per(c->mon);
	}

	arrange(c->mon, false, false);
}

void setgaps(int32_t oh, int32_t ov, int32_t ih, int32_t iv) {
	selmon->gappoh = MAX(oh, 0);
	selmon->gappov = MAX(ov, 0);
	selmon->gappih = MAX(ih, 0);
	selmon->gappiv = MAX(iv, 0);
	arrange(selmon, false, false);
}

void reset_keyboard_layout(void) {
	if (!kb_group || !kb_group->wlr_group || !seat) {
		wlr_log(WLR_ERROR, "Invalid keyboard group or seat");
		return;
	}

	struct wlr_keyboard *keyboard = &kb_group->wlr_group->keyboard;
	if (!keyboard || !keyboard->keymap) {
		wlr_log(WLR_ERROR, "Invalid keyboard or keymap");
		return;
	}

	xkb_layout_index_t current = xkb_state_serialize_layout(
		keyboard->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
	const int32_t num_layouts = xkb_keymap_num_layouts(keyboard->keymap);
	if (num_layouts < 1) {
		wlr_log(WLR_INFO, "No layouts available");
		return;
	}

	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!context) {
		wlr_log(WLR_ERROR, "Failed to create XKB context");
		return;
	}

	struct xkb_keymap *new_keymap = xkb_keymap_new_from_names(
		context, &config.xkb_rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!new_keymap) {
		
		wlr_log(WLR_ERROR,
				"Unexpected failure to create keymap after validation");
		goto cleanup_context;
	}

	const int32_t new_num_layouts = xkb_keymap_num_layouts(new_keymap);
	if (new_num_layouts < 1) {
		wlr_log(WLR_ERROR, "New keymap has no layouts");
		xkb_keymap_unref(new_keymap);
		goto cleanup_context;
	}

	if (current >= new_num_layouts) {
		wlr_log(WLR_INFO,
				"Current layout index %u out of range for new keymap, "
				"resetting to 0",
				current);
		current = 0;
	}

	uint32_t depressed = keyboard->modifiers.depressed;
	uint32_t latched = keyboard->modifiers.latched;
	uint32_t locked = keyboard->modifiers.locked;

	wlr_keyboard_set_keymap(keyboard, new_keymap);

	wlr_keyboard_notify_modifiers(keyboard, depressed, latched, locked, 0);
	keyboard->modifiers.group = current;

	wlr_seat_set_keyboard(seat, keyboard);
	wlr_seat_keyboard_notify_modifiers(seat, &keyboard->modifiers);

	InputDevice *id;
	wl_list_for_each(id, &inputdevices, link) {
		if (id->wlr_device->type != WLR_INPUT_DEVICE_KEYBOARD) {
			continue;
		}

		struct wlr_keyboard *tkb = (struct wlr_keyboard *)id->device_data;

		wlr_keyboard_set_keymap(tkb, keyboard->keymap);
		wlr_keyboard_notify_modifiers(tkb, depressed, latched, locked, 0);
		tkb->modifiers.group = 0;

		wlr_seat_set_keyboard(seat, tkb);
		wlr_seat_keyboard_notify_modifiers(seat, &tkb->modifiers);
	}

	xkb_keymap_unref(new_keymap);

cleanup_context:
	xkb_context_unref(context);
}

void setmon(Client *c, Monitor *m, uint32_t newtags, bool focus) {
	Monitor *oldmon = c->mon;

	if (oldmon == m)
		return;

	if (oldmon && oldmon->sel == c) {
		oldmon->sel = NULL;
	}

	if (oldmon && oldmon->prevsel == c) {
		oldmon->prevsel = NULL;
	}

	c->mon = m;

	if (oldmon)
		arrange(oldmon, false, false);
	if (m) {
		
		reset_foreign_tolevel(c, oldmon, m);
		resize(c, c->geom, 0);
		client_reset_mon_tags(c, m, newtags);
		check_match_tag_floating_rule(c, m);
		setfloating(c, c->isfloating);
		setfullscreen(c, c->isfullscreen);
	}

	if (focus && !client_is_x11_popup(c)) {
		focusclient(focustop(selmon), 1);
	}
}

void setpsel(struct wl_listener *listener, void *data) {
	
	struct wlr_seat_request_set_primary_selection_event *event = data;
	wlr_seat_set_primary_selection(seat, event->source, event->serial);
}

void setsel(struct wl_listener *listener, void *data) {

	struct wlr_seat_request_set_selection_event *event = data;
	if (clipboard.enabled) {
		if (clipboard.ignore_next) {
			clipboard.ignore_next = false;
		} else if (event->source) {
			clip_capture_from_source(event->source);
		}
	}
	wlr_seat_set_selection(seat, event->source, event->serial);
}

void show_hide_client(Client *c) {
	uint32_t target = 1;

	set_size_per(c->mon, c);
	target = get_tags_first_tag(c->oldtags);

	if (!c->is_in_scratchpad) {
		tag_client(&(Arg){.ui = target}, c);
	} else {
		c->tags = c->oldtags;
		arrange(c->mon, false, false);
	}
	client_pending_minimized_state(c, 0);
	focusclient(c, 1);

	CLIENT_SET_FOREIGN_ACTIVATED(c, true);
}

void create_output(struct wlr_backend *backend, void *data) {
	bool *done = data;
	if (*done) {
		return;
	}

	if (wlr_backend_is_wl(backend)) {
		wlr_wl_output_create(backend);
		*done = true;
	} else if (wlr_backend_is_headless(backend)) {
		wlr_headless_add_output(backend, 1920, 1080);
		*done = true;
	}
#if WLR_HAS_X11_BACKEND
	else if (wlr_backend_is_x11(backend)) {
		wlr_x11_output_create(backend);
		*done = true;
	}
#endif
}

void handle_print_status(struct wl_listener *listener, void *data) {

	Monitor *m = NULL;
	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled) {
			continue;
		}
		dwl_ext_workspace_printstatus(m);

		dwl_ipc_output_printstatus(m);
	}
}

void setup(void) {

	setenv("XDG_CURRENT_DESKTOP", "lemon", 1);
	setenv("_JAVA_AWT_WM_NONREPARENTING", "1", 1);

	parse_config();
	if (cli_debug_log) {
		config.log_level = WLR_DEBUG;
	}
	init_baked_points();
	surface_cache_load();

	/* SIGCHLD is reaped synchronously through a signalfd on the event loop
	   (deterministic, no async-signal-handler hazards). If that fails, fall
	   back to the legacy handler so zombies are still reaped. */
	int32_t drm_fd, i;
	int32_t sig[] = {SIGINT, SIGTERM, SIGPIPE};
	struct sigaction sa = {.sa_flags = SA_RESTART, .sa_handler = handlesig};
	sigemptyset(&sa.sa_mask);

	for (i = 0; i < LENGTH(sig); i++)
		sigaction(sig[i], &sa, NULL);

	wlr_log_init(config.log_level, NULL);

	dpy = wl_display_create();
	event_loop = wl_display_get_event_loop(dpy);

	/* Reap SIGCHLD synchronously via signalfd on the now-ready event loop.
	   Fall back to the legacy async handler if signalfd setup fails. */
	if (setup_sigchld_signalfd() < 0) {
		struct sigaction chld = {.sa_flags = SA_RESTART | SA_NOCLDSTOP,
								 .sa_handler = handlesig};
		sigemptyset(&chld.sa_mask);
		sigaction(SIGCHLD, &chld, NULL);
	}
	(void)setup_sigusr1_signalfd();

	if (!(backend = wlr_backend_autocreate(event_loop, &session)))
		die("couldn't create backend");

	headless_backend = wlr_headless_backend_create(event_loop);
	if (!headless_backend) {
		wlr_log(WLR_ERROR, "Failed to create secondary headless backend");
	} else {
		wlr_multi_backend_add(backend, headless_backend);
	}

	scene = wlr_scene_create();
	root_bg = wlr_scene_rect_create(&scene->tree, 0, 0, config.rootcolor);
	for (i = 0; i < NUM_LAYERS; i++)
		layers[i] = wlr_scene_tree_create(&scene->tree);
	drag_icon = wlr_scene_tree_create(&scene->tree);
	wlr_scene_node_place_below(&drag_icon->node, &layers[LyrBlock]->node);

	/* scenefx 0.4 asserts wlr_renderer_is_fx() inside its scene tree, so
	   the renderer MUST be fx_renderer for the scene to work. Vulkan
	   adoption needs scenefx to grow a Vulkan backend upstream first. */
	drw = fx_renderer_create(backend);
	if (!drw)
		die("couldn't create renderer");

	wl_signal_add(&drw->events.lost, &gpu_reset);

	wlr_renderer_init_wl_shm(drw, dpy);

	if (wlr_renderer_get_texture_formats(drw, WLR_BUFFER_CAP_DMABUF)) {
		wlr_drm_create(dpy, drw);
		wlr_scene_set_linux_dmabuf_v1(
			scene, wlr_linux_dmabuf_v1_create_with_renderer(dpy, 4, drw));
	}

	if (config.syncobj_enable) {
		if ((drm_fd = wlr_renderer_get_drm_fd(drw)) >= 0 &&
			drw->features.timeline && backend->features.timeline) {
			wlr_linux_drm_syncobj_manager_v1_create(dpy, 1, drm_fd);
			wlr_log(WLR_INFO,
					"explicit sync (linux-drm-syncobj-v1) enabled");
		} else {
			wlr_log(WLR_INFO,
					"explicit sync requested but unsupported by "
					"renderer/backend; using implicit sync");
		}
	}

	if (!(alloc = wlr_allocator_autocreate(backend, drw)))
		die("couldn't create allocator");

	compositor = wlr_compositor_create(dpy, 6, drw);
	wlr_export_dmabuf_manager_v1_create(dpy);
	screencopy_mgr = wlr_screencopy_manager_v1_create(dpy);
	wlr_ext_image_copy_capture_manager_v1_create(dpy, 1);
	wlr_ext_output_image_capture_source_manager_v1_create(dpy, 1);
	wlr_data_control_manager_v1_create(dpy);
	wlr_data_device_manager_create(dpy);
	wlr_primary_selection_v1_device_manager_create(dpy);
	clipboard.enabled = config.clipboard_history;
	clipboard.max_entries = (size_t)config.clipboard_history_max_entries;
	clipboard.max_bytes_per_entry =
		(size_t)config.clipboard_history_max_bytes;
	if (clipboard.enabled)
		clip_init();
	wlr_viewporter_create(dpy);
	wlr_single_pixel_buffer_manager_v1_create(dpy);
	wlr_fractional_scale_manager_v1_create(dpy, 1);
	wlr_presentation_create(dpy, backend, 2);
	wlr_subcompositor_create(dpy);
	wlr_alpha_modifier_v1_create(dpy);
	wlr_ext_data_control_manager_v1_create(dpy, 1);

	wl_signal_init(&lemon_print_status);
	wl_signal_add(&lemon_print_status, &print_status_listener);

	activation = wlr_xdg_activation_v1_create(dpy);
	wl_signal_add(&activation->events.request_activate, &request_activate);

	wlr_scene_set_gamma_control_manager_v1(
		scene, wlr_gamma_control_manager_v1_create(dpy));

	power_mgr = wlr_output_power_manager_v1_create(dpy);
	wl_signal_add(&power_mgr->events.set_mode, &output_power_mgr_set_mode);

	tearing_control = wlr_tearing_control_manager_v1_create(dpy, 1);
	tearing_new_object.notify = handle_tearing_new_object;
	wl_signal_add(&tearing_control->events.new_object, &tearing_new_object);

	content_type_mgr = wlr_content_type_manager_v1_create(dpy, 1);

	output_layout = wlr_output_layout_create(dpy);
	wl_signal_add(&output_layout->events.change, &layout_change);
	wlr_xdg_output_manager_v1_create(dpy, output_layout);

	wl_list_init(&mons);
	wl_signal_add(&backend->events.new_output, &new_output);

	wl_list_init(&clients);
	wl_list_init(&fstack);
	wl_list_init(&fadeout_clients);
	wl_list_init(&fadeout_layers);

	idle_notifier = wlr_idle_notifier_v1_create(dpy);

	idle_inhibit_mgr = wlr_idle_inhibit_v1_create(dpy);
	wl_signal_add(&idle_inhibit_mgr->events.new_inhibitor, &new_idle_inhibitor);

	keep_idle_inhibit_source = wl_event_loop_add_timer(
		wl_display_get_event_loop(dpy), keep_idle_inhibit, NULL);

	client_hibernate_source = wl_event_loop_add_timer(
		wl_display_get_event_loop(dpy), client_hibernate_scan_callback, NULL);
	wl_event_source_timer_update(client_hibernate_source, 15000);

	setup_idle_timers();

	layer_shell = wlr_layer_shell_v1_create(dpy, 4);
	wl_signal_add(&layer_shell->events.new_surface, &new_layer_surface);

	xdg_shell = wlr_xdg_shell_create(dpy, 6);
	wl_signal_add(&xdg_shell->events.new_toplevel, &new_xdg_toplevel);
	wl_signal_add(&xdg_shell->events.new_popup, &new_xdg_popup);

	session_lock_mgr = wlr_session_lock_manager_v1_create(dpy);
	wl_signal_add(&session_lock_mgr->events.new_lock, &new_session_lock);

	locked_bg =
		wlr_scene_rect_create(layers[LyrBlock], sgeom.width, sgeom.height,
							  (float[4]){0.1, 0.1, 0.1, 1.0});
	wlr_scene_node_set_enabled(&locked_bg->node, false);

	wlr_server_decoration_manager_set_default_mode(
		wlr_server_decoration_manager_create(dpy),
		WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
	xdg_decoration_mgr = wlr_xdg_decoration_manager_v1_create(dpy);
	wl_signal_add(&xdg_decoration_mgr->events.new_toplevel_decoration,
				  &new_xdg_decoration);

	pointer_constraints = wlr_pointer_constraints_v1_create(dpy);
	wl_signal_add(&pointer_constraints->events.new_constraint,
				  &new_pointer_constraint);

	relative_pointer_mgr = wlr_relative_pointer_manager_v1_create(dpy);

	cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(cursor, output_layout);

	set_xcursor_env();

	cursor_mgr =
		wlr_xcursor_manager_create(config.cursor_theme, config.cursor_size);
	/* Pre-decode the cursor theme at the two scales most clients use, so the
	   first cursor change after a launch does not stall on disk IO. */
	wlr_xcursor_manager_load(cursor_mgr, 1);
	wlr_xcursor_manager_load(cursor_mgr, 2);

	wl_signal_add(&cursor->events.motion, &cursor_motion);
	wl_signal_add(&cursor->events.motion_absolute, &cursor_motion_absolute);
	wl_signal_add(&cursor->events.button, &cursor_button);
	wl_signal_add(&cursor->events.axis, &cursor_axis);
	wl_signal_add(&cursor->events.frame, &cursor_frame);

	cursor_shape_mgr = wlr_cursor_shape_manager_v1_create(dpy, 1);
	wl_signal_add(&cursor_shape_mgr->events.request_set_shape,
				  &request_set_cursor_shape);
	hide_cursor_source = wl_event_loop_add_timer(wl_display_get_event_loop(dpy),
												 hidecursor, cursor);

	on_battery = detect_on_battery();
	apply_battery_timer_slack(on_battery);
	apply_cpu_dma_latency(on_battery);
	battery_poll_source = wl_event_loop_add_timer(
		wl_display_get_event_loop(dpy), battery_poll_callback, NULL);
	wl_event_source_timer_update(battery_poll_source, 10000);
	
	wl_list_init(&inputdevices);
	wl_list_init(&keyboard_shortcut_inhibitors);
	wl_signal_add(&backend->events.new_input, &new_input_device);
	virtual_keyboard_mgr = wlr_virtual_keyboard_manager_v1_create(dpy);
	wl_signal_add(&virtual_keyboard_mgr->events.new_virtual_keyboard,
				  &new_virtual_keyboard);
	virtual_pointer_mgr = wlr_virtual_pointer_manager_v1_create(dpy);
	wl_signal_add(&virtual_pointer_mgr->events.new_virtual_pointer,
				  &new_virtual_pointer);

	pointer_gestures = wlr_pointer_gestures_v1_create(dpy);
	LISTEN_STATIC(&cursor->events.swipe_begin, swipe_begin);
	LISTEN_STATIC(&cursor->events.swipe_update, swipe_update);
	LISTEN_STATIC(&cursor->events.swipe_end, swipe_end);
	LISTEN_STATIC(&cursor->events.pinch_begin, pinch_begin);
	LISTEN_STATIC(&cursor->events.pinch_update, pinch_update);
	LISTEN_STATIC(&cursor->events.pinch_end, pinch_end);
	LISTEN_STATIC(&cursor->events.hold_begin, hold_begin);
	LISTEN_STATIC(&cursor->events.hold_end, hold_end);

	seat = wlr_seat_create(dpy, "seat0");

	wl_list_init(&last_cursor_surface_destroy_listener.link);
	wl_signal_add(&seat->events.request_set_cursor, &request_cursor);
	wl_signal_add(&seat->events.request_set_selection, &request_set_sel);
	wl_signal_add(&seat->events.request_set_primary_selection,
				  &request_set_psel);
	wl_signal_add(&seat->events.request_start_drag, &request_start_drag);
	wl_signal_add(&seat->events.start_drag, &start_drag);

	kb_group = createkeyboardgroup();
	wl_list_init(&kb_group->destroy.link);

	keyboard_shortcuts_inhibit = wlr_keyboard_shortcuts_inhibit_v1_create(dpy);
	wl_signal_add(&keyboard_shortcuts_inhibit->events.new_inhibitor,
				  &keyboard_shortcuts_inhibit_new_inhibitor);

	output_mgr = wlr_output_manager_v1_create(dpy);
	wl_signal_add(&output_mgr->events.apply, &output_mgr_apply);
	wl_signal_add(&output_mgr->events.test, &output_mgr_test);

	wlr_scene_set_blur_data(
		scene, config.blur_params.num_passes, config.blur_params.radius,
		config.blur_params.noise, config.blur_params.brightness,
		config.blur_params.contrast, config.blur_params.saturation);

	input_method_manager = wlr_input_method_manager_v2_create(dpy);
	text_input_manager = wlr_text_input_manager_v3_create(dpy);

	dwl_input_method_relay = dwl_im_relay_create();

	drm_lease_manager = wlr_drm_lease_v1_manager_create(dpy, backend);
	if (drm_lease_manager) {
		wl_signal_add(&drm_lease_manager->events.request, &drm_lease_request);
	} else {
		wlr_log(WLR_DEBUG, "Failed to create wlr_drm_lease_device_v1.");
		wlr_log(WLR_INFO, "VR will not be available.");
	}

	wl_global_create(dpy, &zdwl_ipc_manager_v2_interface, 2, NULL,
					 dwl_ipc_manager_bind);

	foreign_toplevel_manager = wlr_foreign_toplevel_manager_v1_create(dpy);
	struct wlr_xdg_foreign_registry *foreign_registry =
		wlr_xdg_foreign_registry_create(dpy);
	wlr_xdg_foreign_v1_create(dpy, foreign_registry);
	wlr_xdg_foreign_v2_create(dpy, foreign_registry);

	workspaces_init();
#ifdef XWAYLAND
	
	xwayland =
		wlr_xwayland_create(dpy, compositor, !config.xwayland_persistence);
	if (xwayland) {
		wl_signal_add(&xwayland->events.ready, &xwayland_ready);
		wl_signal_add(&xwayland->events.new_surface, &new_xwayland_surface);

		setenv("DISPLAY", xwayland->display_name, 1);
	} else {
		fprintf(stderr,
				"failed to setup XWayland X server, continuing without it\n");
	}
	sync_keymap = wl_event_loop_add_timer(wl_display_get_event_loop(dpy),
										  synckeymap, NULL);
#endif
}

void startdrag(struct wl_listener *listener, void *data) {
	struct wlr_drag *drag = data;
	if (!drag->icon)
		return;

	drag->icon->data = &wlr_scene_drag_icon_create(drag_icon, drag->icon)->node;
	LISTEN_STATIC(&drag->icon->events.destroy, destroydragicon);
}

void tag_client(const Arg *arg, Client *target_client) {
	Client *fc = NULL;
	if (target_client && arg->ui & TAGMASK) {

		target_client->tags = arg->ui & TAGMASK;
		target_client->istagswitching = 1;

		wl_list_for_each(fc, &clients, link) {
			if (fc && fc != target_client && target_client->tags & fc->tags &&
				ISFULLSCREEN(fc) && !target_client->isfloating) {
				clear_fullscreen_flag(fc);
			}
		}
		view(&(Arg){.ui = arg->ui, .i = arg->i}, true);

	} else {
		view(arg, true);
	}

	focusclient(target_client, 1);
	printstatus();
}

uint32_t want_restore_fullscreen(Client *target_client) {
	Client *c = NULL;
	wl_list_for_each(c, &clients, link) {
		if (c && c != target_client && c->tags == target_client->tags &&
			c == selmon->sel &&
			c->mon->pertag->ltidxs[get_tags_first_tag_num(c->tags)]->id !=
				SCROLLER &&
			c->mon->pertag->ltidxs[get_tags_first_tag_num(c->tags)]->id !=
				VERTICAL_SCROLLER) {
			return 0;
		}
	}

	return 1;
}

void overview_backup(Client *c) {
	c->overview_isfloatingbak = c->isfloating;
	c->overview_isfullscreenbak = c->isfullscreen;
	c->overview_ismaximizescreenbak = c->ismaximizescreen;
	c->overview_isfullscreenbak = c->isfullscreen;
	c->animation.tagining = false;
	c->animation.tagouted = false;
	c->animation.tagouting = false;
	c->ov_anim = true;
	c->overview_backup_geom = c->geom;
	c->overview_backup_bw = c->bw;
	if (c->isfloating) {
		c->isfloating = 0;
	}
	if (c->isfullscreen || c->ismaximizescreen) {
		client_pending_fullscreen_state(c, 0);
		client_pending_maximized_state(c, 0);
	}
	c->bw = c->isnoborder ? 0 : config.overview_borderpx;

	client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT |
							WLR_EDGE_RIGHT);
}

void overview_restore(Client *c, const Arg *arg) {
	c->isfloating = c->overview_isfloatingbak;
	c->isfullscreen = c->overview_isfullscreenbak;
	c->ismaximizescreen = c->overview_ismaximizescreenbak;
	c->overview_isfloatingbak = 0;
	c->overview_isfullscreenbak = 0;
	c->overview_ismaximizescreenbak = 0;
	c->geom = c->overview_backup_geom;
	c->bw = c->overview_backup_bw;
	c->animation.tagining = false;
	c->ov_anim = true;
	c->is_restoring_from_ov = (arg->ui & c->tags & TAGMASK) == 0 ? true : false;

	if (c->isfloating) {
		
		resize(c, c->overview_backup_geom, 0);
	} else if (c->isfullscreen || c->ismaximizescreen) {
		if (want_restore_fullscreen(c) && c->ismaximizescreen) {
			setmaximizescreen(c, 1);
		} else if (want_restore_fullscreen(c) && c->isfullscreen) {
			setfullscreen(c, 1);
		} else {
			client_pending_fullscreen_state(c, 0);
			client_pending_maximized_state(c, 0);
			setfullscreen(c, false);
		}
	} else {
		if (c->is_restoring_from_ov) {
			c->is_restoring_from_ov = false;
			resize(c, c->overview_backup_geom, 0);
		}
	}

	if (c->bw == 0 &&
		!c->isfullscreen) {
		c->bw = c->isnoborder ? 0 : config.borderpx;
	}

	if (c->isfloating && !c->force_tiled_state) {
		client_set_tiled(c, WLR_EDGE_NONE);
	}
}

/* Runtime idle timer: pairs a configured idle binding with its event source. */
struct IdleTimer {
	const IdleBinding *binding;
	struct wl_event_source *source;
};
static struct IdleTimer *idle_timers = NULL;
static int32_t idle_timers_count = 0;

/* Built-in idle action (idle_timeout/idle_action). One timer, no polling. */
static struct wl_event_source *idle_action_source = NULL;
static bool idle_screen_off = false;

/* Effective built-in idle timeout in ms: the shorter battery timeout when
   unplugged and idle_timeout_battery is set, otherwise idle_timeout. */
static int32_t idle_action_timeout_ms(void) {
	if (on_battery && config.idle_timeout_battery > 0)
		return config.idle_timeout_battery * 1000;
	return config.idle_timeout * 1000;
}

/* DPMS: enable or disable every output. off=true blanks the screens. */
void idle_set_screens(bool off) {
	Monitor *m;
	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output)
			continue;
		struct wlr_output_state state = {0};
		wlr_output_state_set_enabled(&state, !off);
		wlr_output_commit_state(m->wlr_output, &state);
		m->asleep = off;
	}
	idle_screen_off = off;
	updatemons(NULL, NULL);
}

/* Built-in action after idle_timeout: blank screen, suspend, or hibernate.
   Suspended while the session is inactive or a client inhibits idle. */
int32_t idle_action_callback(void *data) {
	if (session && !session->active)
		return 0;
	if (idle_inhibited) {
		wl_event_source_timer_update(idle_action_source,
									 idle_action_timeout_ms());
		return 0;
	}
	switch (config.idle_action) {
	case IDLE_ACTION_SUSPEND:
		spawn_shell(&(Arg){.v = "systemctl suspend"});
		break;
	case IDLE_ACTION_HIBERNATE:
		spawn_shell(&(Arg){.v = "systemctl hibernate"});
		break;
	case IDLE_ACTION_OFF:
	default:
		idle_set_screens(true);
		break;
	}
	return 0;
}

/* ---- Smooth pre-idle backlight dimming (driven by the spring integrator) ---- */
static struct wl_event_source *predim_lead_source = NULL; /* arms the fade */
static struct wl_event_source *predim_anim_source = NULL; /* spring ticker */
static bool predim_active = false;	   /* fade in progress / holding floor */
static int32_t predim_saved_pct = -1;  /* brightness percent before dimming */
static double predim_vis = 0.0;		   /* spring position, in percent */
static double predim_vel = 0.0;
static double predim_target = 0.0;
static uint32_t predim_last_ms = 0;

/* Read the first backlight's brightness as a 1..100 percent (0 on failure). */
static int32_t backlight_read_pct(void) {
	DIR *d = opendir("/sys/class/backlight");
	if (!d)
		return 0;
	struct dirent *e;
	char path[512];
	int32_t pct = 0;
	while ((e = readdir(d))) {
		if (e->d_name[0] == '.')
			continue;
		long cur = -1, max = -1;
		FILE *f;
		snprintf(path, sizeof(path), "/sys/class/backlight/%s/brightness",
				 e->d_name);
		if ((f = fopen(path, "r"))) {
			if (fscanf(f, "%ld", &cur) != 1)
				cur = -1;
			fclose(f);
		}
		snprintf(path, sizeof(path), "/sys/class/backlight/%s/max_brightness",
				 e->d_name);
		if ((f = fopen(path, "r"))) {
			if (fscanf(f, "%ld", &max) != 1)
				max = -1;
			fclose(f);
		}
		if (cur >= 0 && max > 0) {
			pct = (int32_t)((cur * 100 + max / 2) / max);
			break;
		}
	}
	closedir(d);
	return pct;
}

/* Apply a brightness percent via brightnessctl (reliable sysfs write). */
static void backlight_set_pct(int32_t pct) {
	if (pct < 1)
		pct = 1;
	if (pct > 100)
		pct = 100;
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "brightnessctl set %d%%", pct);
	spawn_shell(&(Arg){.v = cmd});
}

/* Spring-tick the backlight toward predim_target; stop once settled. */
int32_t predim_anim_tick(void *data) {
	uint32_t now = frame_now_ms();
	double dt =
		predim_last_ms ? (double)(now - predim_last_ms) / 1000.0 : 1.0 / 30.0;
	if (dt > SPRING_MAX_DT)
		dt = SPRING_MAX_DT;
	if (dt <= 0.0)
		dt = 1.0 / 1000.0;
	predim_last_ms = now;

	predim_vis = spring_axis_step(predim_vis, &predim_vel, predim_target, dt,
								  1.0, 120.0, 16.0);
	backlight_set_pct((int32_t)(predim_vis + 0.5));

	if (fabs(predim_vis - predim_target) < 0.6 && fabs(predim_vel) < 1.0) {
		backlight_set_pct((int32_t)(predim_target + 0.5));
		predim_last_ms = 0;
		return 0; /* settled: stop ticking */
	}
	if (predim_anim_source)
		wl_event_source_timer_update(predim_anim_source, 30);
	return 0;
}

/* Lead timer fired: capture current brightness and spring it down to floor. */
int32_t predim_lead_callback(void *data) {
	if ((session && !session->active) || idle_inhibited)
		return 0;
	predim_saved_pct = backlight_read_pct();
	if (predim_saved_pct <= 0)
		return 0; /* no backlight present */
	predim_active = true;
	predim_vis = predim_saved_pct;
	predim_vel = 0.0;
	predim_target = config.pre_idle_dim_floor;
	predim_last_ms = 0;
	if (predim_anim_source)
		wl_event_source_timer_update(predim_anim_source, 1);
	return 0;
}

/* On activity: spring brightness elastically back to its pre-dim value and
   re-arm the lead timer. No-op when not dimming. */
static void predim_restore_and_rearm(void) {
	if (predim_active && predim_saved_pct > 0) {
		predim_target = predim_saved_pct;
		predim_last_ms = 0;
		if (predim_anim_source)
			wl_event_source_timer_update(predim_anim_source, 1);
		predim_active = false;
	}
	if (predim_lead_source) {
		int32_t lead = config.idle_timeout - config.pre_idle_dim_lead;
		if (lead > 0)
			wl_event_source_timer_update(predim_lead_source, lead * 1000);
	}
}

/* Fire one idle binding's action once its timeout elapses with no activity.
   Skips while the session is inactive or a client inhibits idle, re-arming in
   the inhibit case so it can still fire once the inhibit clears. */
int32_t idle_timer_callback(void *data) {
	struct IdleTimer *t = data;
	if (session && !session->active)
		return 0;
	if (idle_inhibited) {
		wl_event_source_timer_update(t->source, t->binding->timeout_ms);
		return 0;
	}
	if (t->binding->func)
		t->binding->func(&t->binding->arg);
	return 0;
}

/* Re-arm every idle timer to its full timeout; called on user activity.
   Also wakes blanked screens and re-arms the built-in idle action. */
void reset_idle_timers(void) {
	if (idle_screen_off)
		idle_set_screens(false);
	predim_restore_and_rearm();
	for (int32_t i = 0; i < idle_timers_count; i++)
		wl_event_source_timer_update(idle_timers[i].source,
									 idle_timers[i].binding->timeout_ms);
	if (idle_action_source)
		wl_event_source_timer_update(idle_action_source,
									 idle_action_timeout_ms());
}

/* Tear down all idle timer event sources. */
void destroy_idle_timers(void) {
	if (idle_action_source) {
		wl_event_source_remove(idle_action_source);
		idle_action_source = NULL;
	}
	if (predim_lead_source) {
		wl_event_source_remove(predim_lead_source);
		predim_lead_source = NULL;
	}
	if (predim_anim_source) {
		wl_event_source_remove(predim_anim_source);
		predim_anim_source = NULL;
	}
	predim_active = false;
	predim_saved_pct = -1;
	if (!idle_timers)
		return;
	for (int32_t i = 0; i < idle_timers_count; i++) {
		if (idle_timers[i].source)
			wl_event_source_remove(idle_timers[i].source);
	}
	free(idle_timers);
	idle_timers = NULL;
	idle_timers_count = 0;
}

/* (Re)create armed event-loop timers: the built-in idle action plus one per
   configured idlebind. Called at setup and on config hot-reload. */
void setup_idle_timers(void) {
	destroy_idle_timers();

	if (config.idle_timeout > 0) {
		idle_action_source =
			wl_event_loop_add_timer(event_loop, idle_action_callback, NULL);
		if (idle_action_source)
			wl_event_source_timer_update(idle_action_source,
										 idle_action_timeout_ms());
	}

	/* Pre-idle dimming: a disarmed spring ticker plus a lead timer fired
	   pre_idle_dim_lead seconds before the full idle timeout. */
	if (config.pre_idle_dim && config.idle_timeout > config.pre_idle_dim_lead) {
		predim_anim_source =
			wl_event_loop_add_timer(event_loop, predim_anim_tick, NULL);
		predim_lead_source =
			wl_event_loop_add_timer(event_loop, predim_lead_callback, NULL);
		if (predim_lead_source)
			wl_event_source_timer_update(
				predim_lead_source,
				(config.idle_timeout - config.pre_idle_dim_lead) * 1000);
	}

	if (config.idle_bindings_count <= 0)
		return;
	idle_timers = ecalloc(config.idle_bindings_count, sizeof(*idle_timers));
	for (int32_t i = 0; i < config.idle_bindings_count; i++) {
		idle_timers[i].binding = &config.idle_bindings[i];
		idle_timers[i].source = wl_event_loop_add_timer(
			event_loop, idle_timer_callback, &idle_timers[i]);
		if (idle_timers[i].source)
			wl_event_source_timer_update(idle_timers[i].source,
										 config.idle_bindings[i].timeout_ms);
	}
	idle_timers_count = config.idle_bindings_count;
}

void handlecursoractivity(void) {
	reset_idle_timers();
	wl_event_source_timer_update(hide_cursor_source,
								 config.cursor_hide_timeout * 1000);

	if (!cursor_hidden)
		return;

	cursor_hidden = false;

	if (last_cursor.shape)
		wlr_cursor_set_xcursor(cursor, cursor_mgr,
							   wlr_cursor_shape_v1_name(last_cursor.shape));
	else if (last_cursor.surface)
		wlr_cursor_set_surface(cursor, last_cursor.surface,
							   last_cursor.hotspot_x, last_cursor.hotspot_y);
}

int32_t hidecursor(void *data) {
	wlr_cursor_unset_image(cursor);
	cursor_hidden = true;
	return 1;
}

void check_keep_idle_inhibit(Client *c) {
	if (c && c->indleinhibit_when_focus && keep_idle_inhibit_source) {
		wl_event_source_timer_update(keep_idle_inhibit_source, 1000);
	}
}

int32_t keep_idle_inhibit(void *data) {

	if (!idle_inhibit_mgr) {
		wl_event_source_timer_update(keep_idle_inhibit_source, 0);
		return 1;
	}

	if (session && !session->active) {
		wl_event_source_timer_update(keep_idle_inhibit_source, 0);
		return 1;
	}

	if (!selmon || !selmon->sel || !selmon->sel->indleinhibit_when_focus) {
		wl_event_source_timer_update(keep_idle_inhibit_source, 0);
		return 1;
	}

	if (seat && idle_notifier) {
		wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);
		reset_idle_timers();
		wl_event_source_timer_update(keep_idle_inhibit_source, 1000);
	}
	return 1;
}

void unlocksession(struct wl_listener *listener, void *data) {
	SessionLock *lock = wl_container_of(listener, lock, unlock);
	destroylock(lock, 1);
}

void unmaplayersurfacenotify(struct wl_listener *listener, void *data) {
	LayerSurface *l = wl_container_of(listener, l, unmap);

	l->mapped = 0;
	l->being_unmapped = true;

	init_fadeout_layers(l);

	wlr_scene_node_set_enabled(&l->scene->node, false);

	if (l == exclusive_focus)
		exclusive_focus = NULL;

	if (l->layer_surface->output && (l->mon = l->layer_surface->output->data))
		arrangelayers(l->mon);

	reset_exclusive_layers_focus(l->mon);

	motionnotify(0, NULL, 0, 0, 0, 0);
	layer_flush_blur_background(l);
	wlr_scene_node_destroy(&l->shadow->node);
	l->shadow = NULL;
	l->being_unmapped = false;
}

void unmapnotify(struct wl_listener *listener, void *data) {

	Client *c = wl_container_of(listener, c, unmap);
	Monitor *m = NULL;
	Client *nextfocus = NULL;
	c->iskilling = 1;

	/* If the window the cycler is referencing is about to disappear, tear
	   the overlay down so we never dereference a freed Client. */
	if (window_cycler.active) {
		for (int32_t i = 0; i < window_cycler.count; i++) {
			if (window_cycler.clients[i] == c) {
				window_cycler_destroy();
				break;
			}
		}
	}
	/* Clear the cycler-raised tile pointer if its window is about to
	   tear down -- avoid reparenting a freed scene tree later. */
	if (cycler_raised_tile == c)
		cycler_raised_tile = NULL;

	/* Persist last-known size for this app_id before the client tears down,
	   so the next launch can configure straight to the right geometry. */
	if (!client_is_x11(c) && c->geom.width > 0 && c->geom.height > 0) {
		surface_cache_store(client_get_appid(c),
		                    c->geom.width - 2 * (int32_t)c->bw,
		                    c->geom.height - 2 * (int32_t)c->bw);
	}
	struct ScrollerStackNode *target_node =
		c->mon ? find_scroller_node(
					 c->mon->pertag->scroller_state[c->mon->pertag->curtag], c)
			   : NULL;
	struct ScrollerStackNode *prev_node =
		target_node ? target_node->prev_in_stack : NULL;
	struct ScrollerStackNode *next_node =
		target_node ? target_node->next_in_stack : NULL;

	if (config.animations && !c->is_clip_to_hide && !c->isminimized &&
		(!c->mon || VISIBLEON(c, c->mon)))
		init_fadeout_client(c);

	if (c->swallowedby) {
		c->swallowedby->mon = c->mon;
		swallow(c->swallowedby, c);
	} else {
		scroller_remove_client(c);
		dwindle_remove_client(c);
	}

	if (c == grabc) {
		cursor_mode = CurNormal;
		grabc = NULL;
	}

	if (c == dropc) {
		dropc = NULL;
	}

	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled) {
			continue;
		}
		if (c == m->sel) {
			m->sel = NULL;
		}
		if (c == m->prevsel) {
			m->prevsel = NULL;
		}
	}

	if (c->mon && c->mon == selmon) {
		if (next_node && !c->swallowedby) {
			nextfocus = next_node->client;
		} else if (prev_node && !c->swallowedby) {
			nextfocus = prev_node->client;
		} else {
			nextfocus = focustop(selmon);
		}

		if (nextfocus) {
			focusclient(nextfocus, 0);
		}

		if (!nextfocus && selmon->isoverview) {
			Arg arg = {0};
			toggleoverview(&arg);
		}
	}

	if (client_is_unmanaged(c)) {
#ifdef XWAYLAND
		if (client_is_x11(c)) {
			wl_list_remove(&c->set_geometry.link);
		}
#endif
		if (c == exclusive_focus)
			exclusive_focus = NULL;
		if (client_surface(c) == seat->keyboard_state.focused_surface)
			focusclient(focustop(selmon), 1);
	} else {
		if (!c->swallowing)
			wl_list_remove(&c->link);
		setmon(c, NULL, 0, true);
		if (!c->swallowing)
			wl_list_remove(&c->flink);
	}

	if (c->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_destroy(c->foreign_toplevel);
		c->foreign_toplevel = NULL;
	}

	if (c->swallowedby) {
		setmaximizescreen(c->swallowedby, c->ismaximizescreen);
		setfullscreen(c->swallowedby, c->isfullscreen);
		c->swallowedby->swallowing = NULL;
		c->swallowedby = NULL;
	}

	if (c->swallowing) {
		c->swallowing->swallowedby = NULL;
		c->swallowing = NULL;
	}

	c->stack_proportion = 0.0f;

	wlr_scene_node_destroy(&c->scene->node);
	printstatus();
	motionnotify(0, NULL, 0, 0, 0, 0);
}

void updatemons(struct wl_listener *listener, void *data) {
	
	struct wlr_output_configuration_v1 *output_config =
		wlr_output_configuration_v1_create();
	Client *c = NULL;
	struct wlr_output_configuration_head_v1 *config_head;
	Monitor *m = NULL;
	int32_t mon_pos_offsetx, mon_pos_offsety, oldx, oldy;

	wl_list_for_each(m, &mons, link) {
		if (m->wlr_output->enabled || m->asleep)
			continue;
		config_head = wlr_output_configuration_head_v1_create(output_config,
															  m->wlr_output);
		config_head->state.enabled = 0;
		
		wlr_output_layout_remove(output_layout, m->wlr_output);
		closemon(m);
		m->m = m->w = (struct wlr_box){0};
	}
	
	wl_list_for_each(m, &mons, link) {
		if (m->wlr_output->enabled &&
			!wlr_output_layout_get(output_layout, m->wlr_output))
			wlr_output_layout_add_auto(output_layout, m->wlr_output);
	}

	wlr_output_layout_get_box(output_layout, NULL, &sgeom);

	wlr_scene_node_set_position(&root_bg->node, sgeom.x, sgeom.y);
	wlr_scene_rect_set_size(root_bg, sgeom.width, sgeom.height);

	wlr_scene_node_set_position(&locked_bg->node, sgeom.x, sgeom.y);
	wlr_scene_rect_set_size(locked_bg, sgeom.width, sgeom.height);

	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled)
			continue;
		config_head = wlr_output_configuration_head_v1_create(output_config,
															  m->wlr_output);

		oldx = m->m.x;
		oldy = m->m.y;
		
		wlr_output_layout_get_box(output_layout, m->wlr_output, &m->m);
		m->w = m->m;
		mon_pos_offsetx = m->m.x - oldx;
		mon_pos_offsety = m->m.y - oldy;

		wl_list_for_each(c, &clients, link) {
			
			if (c->isfloating && c->mon == m) {
				c->geom.x += mon_pos_offsetx;
				c->geom.y += mon_pos_offsety;
				c->float_geom = c->geom;
				resize(c, c->geom, 1);
			}

			if (c->mon && c->mon != m && client_surface(c)->mapped &&
				strcmp(c->oldmonname, m->wlr_output->name) == 0) {
				client_change_mon(c, m);
			}
		}

		wlr_scene_output_set_position(m->scene_output, m->m.x, m->m.y);

		if (config.blur && m->blur) {
			wlr_scene_node_set_position(&m->blur->node, m->m.x, m->m.y);
			wlr_scene_optimized_blur_set_size(m->blur, m->m.width, m->m.height);
		}

		if (m->lock_surface) {
			struct wlr_scene_tree *scene_tree = m->lock_surface->surface->data;
			wlr_scene_node_set_position(&scene_tree->node, m->m.x, m->m.y);
			wlr_session_lock_surface_v1_configure(m->lock_surface, m->m.width,
												  m->m.height);
		}

		arrangelayers(m);
		
		arrange(m, false, false);
		
		if ((c = focustop(m)) && c->isfullscreen)
			resize(c, m->m, 0);

		config_head->state.x = m->m.x;
		config_head->state.y = m->m.y;

		if (!selmon)
			selmon = m;
	}

	if (selmon && selmon->wlr_output->enabled) {
		wl_list_for_each(c, &clients, link) {
			if (!c->mon && client_surface(c)->mapped) {
				c->mon = selmon;
				reset_foreign_tolevel(c, NULL, c->mon);
			}
			if (c->tags == 0 && !c->is_in_scratchpad) {
				c->tags = selmon->tagset[selmon->seltags];
				set_size_per(selmon, c);
			}
		}
		focusclient(focustop(selmon), 1);
		if (selmon->lock_surface) {
			client_notify_enter(selmon->lock_surface->surface,
								wlr_seat_get_keyboard(seat));
			client_activate_surface(selmon->lock_surface->surface, 1);
		}
	}

	wlr_cursor_move(cursor, NULL, 0, 0);

	wlr_output_manager_v1_set_configuration(output_mgr, output_config);
}

void updatetitle(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_title);

	if (!c || c->iskilling)
		return;

	const char *title;
	title = client_get_title(c);
	if (title && c->foreign_toplevel)
		wlr_foreign_toplevel_handle_v1_set_title(c->foreign_toplevel, title);
	if (c == focustop(c->mon))
		printstatus();
}

void
urgent(struct wl_listener *listener, void *data) {
	struct wlr_xdg_activation_v1_request_activate_event *event = data;
	Client *c = NULL;
	toplevel_from_wlr_surface(event->surface, &c, NULL);

	if (!c || !c->foreign_toplevel)
		return;

	if (config.focus_on_activate && !c->istagsilent && c != selmon->sel) {
		if (!(c->mon == selmon && c->tags & c->mon->tagset[c->mon->seltags]))
			view_in_mon(&(Arg){.ui = c->tags}, true, c->mon, true);
		focusclient(c, 1);
	} else if (c != focustop(selmon)) {
		c->isurgent = 1;
		if (client_surface(c)->mapped)
			setborder_color(c);
		printstatus();
	}
}

void view_in_mon(const Arg *arg, bool want_animation, Monitor *m,
				 bool changefocus) {
	uint32_t i, tmptag;

	if (!m || (arg->ui != (~0 & TAGMASK) && m->isoverview)) {
		return;
	}

	if (arg->ui == 0) {
		return;
	}

	if (arg->ui == UINT32_MAX) {
		if (m->tagset[0] != m->tagset[1]) {
			m->pertag->prevtag = get_tags_first_tag_num(m->tagset[m->seltags]);
			m->seltags ^= 1;
			m->pertag->curtag = get_tags_first_tag_num(m->tagset[m->seltags]);
			goto toggleseltags;
		} else {
			return;
		}
	}

	if ((m->tagset[m->seltags] & arg->ui & TAGMASK) != 0) {
		want_animation = false;
	}

	m->seltags ^= 1;

	if (arg->ui & TAGMASK) {
		m->tagset[m->seltags] = arg->ui & TAGMASK;
		tmptag = m->pertag->curtag;

		if (arg->ui == (~0 & TAGMASK))
			m->pertag->curtag = 0;
		else {
			for (i = 0; !(arg->ui & 1 << i) && i < LENGTH(tags) && arg->ui != 0;
				 i++)
				;
			m->pertag->curtag = i >= LENGTH(tags) ? LENGTH(tags) : i + 1;
		}

		m->pertag->prevtag =
			tmptag == m->pertag->curtag ? m->pertag->prevtag : tmptag;
	} else {
		tmptag = m->pertag->prevtag;
		m->pertag->prevtag = m->pertag->curtag;
		m->pertag->curtag = tmptag;
	}

toggleseltags:

	if (changefocus)
		focusclient(focustop(m), 1);
	arrange(m, want_animation, true);
	printstatus();
}

void view(const Arg *arg, bool want_animation) {
	Monitor *m = NULL;
	if (arg->i) {
		view_in_mon(arg, want_animation, selmon, true);
		wl_list_for_each(m, &mons, link) {
			if (!m->wlr_output->enabled || m == selmon)
				continue;
			
			view_in_mon(arg, want_animation, m, false);
		}
	} else {
		view_in_mon(arg, want_animation, selmon, true);
	}
}

static void
handle_keyboard_shortcuts_inhibitor_destroy(struct wl_listener *listener,
											void *data) {
	KeyboardShortcutsInhibitor *inhibitor =
		wl_container_of(listener, inhibitor, destroy);

	wlr_log(WLR_DEBUG, "Removing keyboard shortcuts inhibitor");

	wl_list_remove(&inhibitor->link);
	wl_list_remove(&inhibitor->destroy.link);
	free(inhibitor);
}

void handle_keyboard_shortcuts_inhibit_new_inhibitor(
	struct wl_listener *listener, void *data) {

	struct wlr_keyboard_shortcuts_inhibitor_v1 *inhibitor = data;

	if (config.allow_shortcuts_inhibit == SHORTCUTS_INHIBIT_DISABLE) {
		return;
	}

	Client *c = NULL;
	LayerSurface *l = NULL;

	int32_t type = toplevel_from_wlr_surface(inhibitor->surface, &c, &l);

	if (type < 0)
		return;

	if (type != LayerShell && c && !c->allow_shortcuts_inhibit) {
		return;
	}

	wlr_log(WLR_DEBUG, "Adding keyboard shortcuts inhibitor");

	KeyboardShortcutsInhibitor *kbsinhibitor =
		calloc(1, sizeof(KeyboardShortcutsInhibitor));

	kbsinhibitor->inhibitor = inhibitor;

	kbsinhibitor->destroy.notify = handle_keyboard_shortcuts_inhibitor_destroy;
	wl_signal_add(&inhibitor->events.destroy, &kbsinhibitor->destroy);

	wl_list_insert(&keyboard_shortcut_inhibitors, &kbsinhibitor->link);

	wlr_keyboard_shortcuts_inhibitor_v1_activate(inhibitor);
}

void virtualkeyboard(struct wl_listener *listener, void *data) {
	struct wlr_virtual_keyboard_v1 *kb = data;
	
	wlr_seat_set_capabilities(seat,
							  seat->capabilities | WL_SEAT_CAPABILITY_KEYBOARD);
	KeyboardGroup *group = createkeyboardgroup();
	
	wlr_keyboard_set_keymap(&kb->keyboard, group->wlr_group->keyboard.keymap);
	LISTEN(&kb->keyboard.base.events.destroy, &group->destroy,
		   destroykeyboardgroup);

	wlr_keyboard_group_add_keyboard(group->wlr_group, &kb->keyboard);
}

void warp_cursor(const Client *c) {
	if (INSIDEMON(c)) {
		wlr_cursor_warp_closest(cursor, NULL, c->geom.x + c->geom.width / 2.0,
								c->geom.y + c->geom.height / 2.0);
		motionnotify(0, NULL, 0, 0, 0, 0);
	}
}

void warp_cursor_to_selmon(Monitor *m) {

	wlr_cursor_warp_closest(cursor, NULL, m->w.x + m->w.width / 2.0,
							m->w.y + m->w.height / 2.0);
	wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
	handlecursoractivity();
}

void virtualpointer(struct wl_listener *listener, void *data) {
	struct wlr_virtual_pointer_v1_new_pointer_event *event = data;
	struct wlr_input_device *device = &event->new_pointer->pointer.base;
	wlr_seat_set_capabilities(seat,
							  seat->capabilities | WL_SEAT_CAPABILITY_POINTER);
	wlr_cursor_attach_input_device(cursor, device);
	if (event->suggested_output)
		wlr_cursor_map_input_to_output(cursor, device, event->suggested_output);

	handlecursoractivity();
}

#ifdef XWAYLAND
void fix_xwayland_unmanaged_coordinate(Client *c) {
	if (!selmon)
		return;

	if (c->geom.x >= selmon->m.x && c->geom.x < selmon->m.x + selmon->m.width &&
		c->geom.y >= selmon->m.y && c->geom.y < selmon->m.y + selmon->m.height)
		return;

	c->geom = setclient_coordinate_center(c, selmon, c->geom, 0, 0);
}

int32_t synckeymap(void *data) {
	reset_keyboard_layout();
	
	wlr_log(WLR_INFO, "timer to synckeymap done");
	wl_event_source_timer_update(sync_keymap, 0);
	return 0;
}

void activatex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, activate);
	bool need_arrange = false;

	if (!c || c->iskilling || !c->foreign_toplevel || client_is_unmanaged(c))
		return;

	if (c && c->swallowing)
		return;

	if (c->isminimized) {
		client_pending_minimized_state(c, 0);
		c->tags = c->mini_restore_tag;
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		setborder_color(c);
		if (VISIBLEON(c, c->mon)) {
			need_arrange = true;
		}
	}

	if (config.focus_on_activate && !c->istagsilent && c != selmon->sel) {
		if (!(c->mon == selmon && c->tags & c->mon->tagset[c->mon->seltags]))
			view_in_mon(&(Arg){.ui = c->tags}, true, c->mon, true);
		wlr_xwayland_surface_activate(c->surface.xwayland, 1);
		focusclient(c, 1);
		need_arrange = true;
	} else if (c != focustop(selmon)) {
		c->isurgent = 1;
		if (client_surface(c)->mapped)
			setborder_color(c);
	}

	if (need_arrange) {
		arrange(c->mon, false, false);
	}

	printstatus();
}

void configurex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, configure);
	struct wlr_xwayland_surface_configure_event *event = data;
	if (!client_surface(c) || !client_surface(c)->mapped) {
		wlr_xwayland_surface_configure(c->surface.xwayland, event->x, event->y,
									   event->width, event->height);
		return;
	}
	if (client_is_unmanaged(c)) {
		wlr_scene_node_set_position(&c->scene->node, event->x, event->y);
		wlr_xwayland_surface_configure(c->surface.xwayland, event->x, event->y,
									   event->width, event->height);
		return;
	}
	if ((c->isfloating && c != grabc) ||
		!c->mon->pertag->ltidxs[c->mon->pertag->curtag]->arrange) {
		resize(c,
			   (struct wlr_box){.x = event->x - c->bw,
								.y = event->y - c->bw,
								.width = event->width + c->bw * 2,
								.height = event->height + c->bw * 2},
			   0);
	} else {
		arrange(c->mon, false, false);
	}
}

void createnotifyx11(struct wl_listener *listener, void *data) {
	struct wlr_xwayland_surface *xsurface = data;
	Client *c = NULL;

	c = xsurface->data = ecalloc(1, sizeof(*c));
	c->surface.xwayland = xsurface;
	c->type = X11;
	
	LISTEN(&xsurface->events.associate, &c->associate, associatex11);
	LISTEN(&xsurface->events.destroy, &c->destroy, destroynotify);
	LISTEN(&xsurface->events.dissociate, &c->dissociate, dissociatex11);
	LISTEN(&xsurface->events.request_activate, &c->activate, activatex11);
	LISTEN(&xsurface->events.request_configure, &c->configure, configurex11);
	LISTEN(&xsurface->events.request_fullscreen, &c->fullscreen,
		   fullscreennotify);
	LISTEN(&xsurface->events.set_hints, &c->set_hints, sethints);
	LISTEN(&xsurface->events.set_title, &c->set_title, updatetitle);
	LISTEN(&xsurface->events.request_maximize, &c->maximize, maximizenotify);
	LISTEN(&xsurface->events.request_minimize, &c->minimize, minimizenotify);
}

void commitx11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, commmitx11);
	struct wlr_surface_state *state = &c->surface.xwayland->surface->current;

	if ((int32_t)c->geom.width - 2 * (int32_t)c->bw == (int32_t)state->width &&
		(int32_t)c->geom.height - 2 * (int32_t)c->bw ==
			(int32_t)state->height &&
		(int32_t)c->surface.xwayland->x ==
			(int32_t)c->geom.x + (int32_t)c->bw &&
		(int32_t)c->surface.xwayland->y ==
			(int32_t)c->geom.y + (int32_t)c->bw) {
		c->configure_serial = 0;
	}
}

void associatex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, associate);

	LISTEN(&client_surface(c)->events.map, &c->map, mapnotify);
	LISTEN(&client_surface(c)->events.unmap, &c->unmap, unmapnotify);
	LISTEN(&client_surface(c)->events.commit, &c->commmitx11, commitx11);
}

void dissociatex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, dissociate);
	wl_list_remove(&c->map.link);
	wl_list_remove(&c->unmap.link);
	wl_list_remove(&c->commmitx11.link);
}

void sethints(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_hints);
	struct wlr_surface *surface = client_surface(c);
	if (c == focustop(selmon) || !c || !c->surface.xwayland->hints)
		return;

	c->isurgent = xcb_icccm_wm_hints_get_urgency(c->surface.xwayland->hints);
	printstatus();

	if (c->isurgent && surface && surface->mapped)
		setborder_color(c);
}

void xwaylandready(struct wl_listener *listener, void *data) {
	struct wlr_xcursor *xcursor;

	wlr_xwayland_set_seat(xwayland, seat);

	if ((xcursor = wlr_xcursor_manager_get_xcursor(cursor_mgr, "default", 1)))
		wlr_xwayland_set_cursor(
			xwayland, xcursor->images[0]->buffer, xcursor->images[0]->width * 4,
			xcursor->images[0]->width, xcursor->images[0]->height,
			xcursor->images[0]->hotspot_x, xcursor->images[0]->hotspot_y);
	
	wl_event_source_timer_update(sync_keymap, 500);
}

static void setgeometrynotify(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_geometry);

	wlr_scene_node_set_position(&c->scene->node, c->surface.xwayland->x,
								c->surface.xwayland->y);
	motionnotify(0, NULL, 0, 0, 0, 0);
}
#endif

/* Immunize the compositor against the OOM killer by pinning oom_score_adj to
   the minimum (-1000). Best-effort: logs and continues on any failure. */
static void protect_from_oom_killer(void) {
	int fd = open("/proc/self/oom_score_adj", O_WRONLY | O_CLOEXEC);
	if (fd < 0) {
		wlr_log_errno(WLR_INFO, "oom_score_adj: open failed");
		return;
	}
	static const char score[] = "-1000";
	ssize_t n = write(fd, score, sizeof(score) - 1);
	if (n != (ssize_t)(sizeof(score) - 1))
		wlr_log_errno(WLR_INFO, "oom_score_adj: write failed");
	close(fd);
}

/* Event-loop callback: drain the signalfd and reap every dead child
   synchronously. Returns 0 (event handled). */
static int32_t handle_sigchld_fd(int32_t fd, uint32_t mask, void *data) {
	struct signalfd_siginfo si;
	while (read(fd, &si, sizeof(si)) == (ssize_t)sizeof(si))
		; /* drain queued SIGCHLD notifications */
	while (waitpid(-1, NULL, WNOHANG) > 0)
		; /* reap all zombies */
	return 0;
}

/* Event-loop callback: trigger a config reload when SIGUSR1 arrives. Lets
   theme generators (matugen, pywal, etc.) re-skin the compositor without
   the user mashing the reload keybind. Drops the signal if setup hasn't
   completed yet. */
static int32_t handle_sigusr1_fd(int32_t fd, uint32_t mask, void *data) {
	struct signalfd_siginfo si;
	while (read(fd, &si, sizeof(si)) == (ssize_t)sizeof(si))
		;
	if (!reload_ready)
		return 0;
	reload_config(NULL);
	return 0;
}

/* Block SIGUSR1 and route it through a signalfd on the event loop so the
   reload runs from the main thread (avoids async-signal restrictions on
   parse_config_file / scene rebuild). */
static int32_t setup_sigusr1_signalfd(void) {
	sigset_t usr1;
	sigemptyset(&usr1);
	sigaddset(&usr1, SIGUSR1);
	if (sigprocmask(SIG_BLOCK, &usr1, NULL) != 0) {
		wlr_log_errno(WLR_ERROR, "signalfd: sigprocmask SIGUSR1 failed");
		return -1;
	}
	int fd = signalfd(-1, &usr1, SFD_CLOEXEC | SFD_NONBLOCK);
	if (fd < 0) {
		wlr_log_errno(WLR_ERROR, "signalfd: SIGUSR1 creation failed");
		sigprocmask(SIG_UNBLOCK, &usr1, NULL);
		return -1;
	}
	if (!wl_event_loop_add_fd(event_loop, fd, WL_EVENT_READABLE,
							  handle_sigusr1_fd, NULL)) {
		wlr_log(WLR_ERROR, "signalfd: SIGUSR1 add_fd failed");
		close(fd);
		sigprocmask(SIG_UNBLOCK, &usr1, NULL);
		return -1;
	}
	return fd;
}

/* Block SIGCHLD and route it through a signalfd on the event loop so children
   are reaped deterministically inside the main loop, not in an async handler.
   Returns the fd, or -1 on failure (caller keeps the legacy handler). */
static int32_t setup_sigchld_signalfd(void) {
	sigset_t chld;
	sigemptyset(&chld);
	sigaddset(&chld, SIGCHLD);
	if (sigprocmask(SIG_BLOCK, &chld, NULL) != 0) {
		wlr_log_errno(WLR_ERROR, "signalfd: sigprocmask failed");
		return -1;
	}
	int fd = signalfd(-1, &chld, SFD_CLOEXEC | SFD_NONBLOCK);
	if (fd < 0) {
		wlr_log_errno(WLR_ERROR, "signalfd: creation failed");
		sigprocmask(SIG_UNBLOCK, &chld, NULL);
		return -1;
	}
	if (!wl_event_loop_add_fd(event_loop, fd, WL_EVENT_READABLE,
							  handle_sigchld_fd, NULL)) {
		wlr_log(WLR_ERROR, "signalfd: wl_event_loop_add_fd failed");
		close(fd);
		sigprocmask(SIG_UNBLOCK, &chld, NULL);
		return -1;
	}
	return fd;
}

int32_t main(int32_t argc, char *argv[]) {
	char *startup_cmd = NULL;
	int32_t c;

	while ((c = getopt(argc, argv, "s:c:hdvp")) != -1) {
		if (c == 's') {
			startup_cmd = optarg;
		} else if (c == 'd') {
			cli_debug_log = true;
		} else if (c == 'v') {
			printf("lemon " VERSION "\n");
			return EXIT_SUCCESS;
		} else if (c == 'c') {
			cli_config_path = optarg;
		} else if (c == 'p') {
			return parse_config() ? EXIT_SUCCESS : EXIT_FAILURE;
		} else {
			goto usage;
		}
	}
	if (optind < argc)
		goto usage;

	if (!getenv("XDG_RUNTIME_DIR"))
		die("XDG_RUNTIME_DIR must be set");

	/* Never let the kernel sacrifice the compositor under memory pressure. */
	protect_from_oom_killer();

	/* Step 0: tighten glibc malloc heap behavior. Lower trim threshold so
	   freed brk-region pages return to the kernel quickly (default 128 KB
	   * 2^N grows unbounded). Lower mmap threshold so large allocations
	   go through mmap and are munmap'd on free (cannot fragment heap). */
	mallopt(M_TRIM_THRESHOLD, 128 * 1024);
	mallopt(M_MMAP_THRESHOLD, 128 * 1024);
	/* Cap per-thread malloc arenas. glibc defaults to 8 * nproc arenas, each
	   able to grow its own heap — on a many-core laptop that bloats RSS and
	   fragments badly for a compositor that is almost entirely single-threaded.
	   Two arenas keep the main loop and any wlroots worker thread contention-
	   free while holding resident memory down. */
	mallopt(M_ARENA_MAX, 2);

	/* Step 1: best-effort soft realtime via SCHED_RR priority 1. Requires
	   CAP_SYS_NICE or membership in 'realtime' group with appropriate limits
	   in /etc/security/limits.d/. Falls back to nice -10. */
	{
		struct sched_param sp = {.sched_priority = 1};
		if (sched_setscheduler(0, SCHED_RR, &sp) != 0) {
			(void)setpriority(PRIO_PROCESS, 0, -10);
		}
	}

	setup();

	/* Step 2 (deferred until after parse_config so mlock_pages is
	   readable): lock current pages in RAM so a page fault never
	   stalls the input event loop. MCL_ONFAULT keeps it cheap for
	   sparse mappings. Ignored if the process lacks CAP_IPC_LOCK /
	   RLIMIT_MEMLOCK. Skipped entirely when mlock_pages=0, which
	   trades input-latency-on-pressure for ~tens of MB of resident
	   memory savings. */
	if (config.mlock_pages) {
#ifdef MCL_ONFAULT
		(void)mlockall(MCL_CURRENT | MCL_ONFAULT);
#else
		(void)mlockall(MCL_CURRENT);
#endif
	}
	run(startup_cmd);
	cleanup();
	return EXIT_SUCCESS;
usage:
	printf("Usage: lemon [OPTIONS]\n"
		   "\n"
		   "Options:\n"
		   "  -v             Show lemon version\n"
		   "  -d             Enable debug log\n"
		   "  -c <file>      Use custom configuration file\n"
		   "  -s <command>   Execute startup command\n"
		   "  -p             Check configuration file error\n");
	return EXIT_SUCCESS;
}
