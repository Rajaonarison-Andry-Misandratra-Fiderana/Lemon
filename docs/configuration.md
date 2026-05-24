# Lemon — Configuration Reference

Config lives at `~/.config/lemon/lemon.conf` (fallback `/etc/lemon/lemon.conf`).
Format is `key=value`, one per line, `#` for comments. The file **hot-reloads**
on save. Binds and rules accept comma-separated fields. The annotated example is
`assets/lemon.conf`.

---

## General

| Key | Values | Meaning |
|-----|--------|---------|
| `keymode` | string | Active key-mode name. |
| `sloppyfocus` | 0/1 | Focus follows the pointer. |
| `focus_on_activate` | 0/1 | Focus a client when it requests activation. |
| `warpcursor` | 0/1 | Warp the cursor to the focused window. |
| `focus_cross_monitor` | 0/1 | Directional focus can cross monitors. |
| `focus_cross_tag` | 0/1 | Focus can cross tags. |
| `exchange_cross_monitor` | 0/1 | `exchange_client` works across monitors. |
| `scratchpad_cross_monitor` | 0/1 | Scratchpad shared across monitors. |
| `single_scratchpad` | 0/1 | Only one scratchpad window at a time. |
| `xwayland_persistence` | 0/1 | Keep the XWayland server alive when idle. |
| `view_current_to_back` | 0/1 | Re-viewing the current tag sends it to back. |

## Animations

| Key | Values | Meaning |
|-----|--------|---------|
| `animations` | 0/1 | Master animation switch. |
| `layer_animations` | 0/1 | Animate layer-shell surfaces (bars, launchers). |
| `animation_type_open` | `fade`/`zoom`/`slide` (`+` combine) | Window open style. |
| `animation_type_close` | same | Window close style. |
| `layer_animation_type_open` / `_close` | `translate`/`fade`… | Layer-surface styles. |
| `animation_duration_open/close/move/tag/focus` | ms | Bezier durations (open/close only — move/tag use spring). |
| `animation_fade_in` / `animation_fade_out` | 0/1 | Opacity fade on open/close (scenefx). |
| `fadein_begin_opacity` / `fadeout_begin_opacity` | 0..1 | Fade start opacity. |
| `zoom_initial_ratio` / `zoom_end_ratio` | 0..1 | Scale endpoints for zoom open/close. |
| `tag_animation_direction` | 0/1 | Horizontal vs vertical workspace slide. |
| `animation_curve_open/close/move/tag/focus/opafadein/opafadeout` | 4 floats | Cubic-bezier control points. |
| `axis_bind_apply_timeout` | ms | Debounce window for `axisbind` (scroll-wheel) actions. |

### Spring physics (geometry: move / resize / tile / overview / tag)

| Key | Values | Meaning |
|-----|--------|---------|
| `animation_spring` | 0/1 | Use spring physics for geometry (else bezier). |
| `animation_spring_mass` | float | Spring mass `m` (heavier = slower). |
| `animation_spring_tension` | float | Stiffness `k` (higher = faster). |
| `animation_spring_friction` | float | Damping `c`. `c ≈ 2·√(k·m)` = no overshoot; lower = bounce. Clamped ≥1. |
| `animation_spring_tag_tension` | float | Separate, usually faster tension for workspace switch. |
| `animation_spring_tag_friction` | float | Damping for the tag spring. |
| `animation_spring_overview_tension` | float | Snappier tension used while entering/leaving overview. |
| `animation_spring_overview_friction` | float | Damping for the overview spring. |
| `animation_momentum` | 0/1 | On drag/resize release, inject pointer velocity into the spring (flick to throw). |
| `animation_momentum_scale` | 0..3 | Multiplier on the handed-off velocity. |
| `animation_motion_blur` | 0/1 | Fade a window proportional to spring speed; crisp on settle. |
| `animation_motion_blur_strength` | 0..0.9 | Max opacity drop at full speed. |

## Layout & Gaps

| Key | Values | Meaning |
|-----|--------|---------|
| `default_mfact` | 0..1 | Master area fraction. |
| `default_nmaster` | int | Master client count. |
| `new_is_master` | 0/1 | New window becomes master. |
| `smartgaps` | 0/1 | Drop gaps when a single window. |
| `no_border_when_single` / `no_radius_when_single` | 0/1 | Drop border / corner radius when single. |
| `circle_layout` | 0/1 | Cycle layouts in a ring. |
| `center_master_overspread` / `center_when_single_stack` | 0/1 | Centered-master behavior. |
| `gappih` / `gappiv` | px | Inner gaps horizontal / vertical. |
| `gappoh` / `gappov` | px | Outer gaps horizontal / vertical. |
| `overviewgappi` / `overviewgappo` | px | Gaps in overview. |
| `dwindle_*` | various | Dwindle layout: `split_ratio`, `smart_split`, `smart_resize`, `manual_split`, `preserve_split`, `vsplit`, `hsplit`, `drop_simple_split`. |

### Overview

| Key | Values | Meaning |
|-----|--------|---------|
| `enable_hotarea` | 0/1 | Trigger overview when the cursor hits a screen corner. |
| `hotarea_corner` | 0..3 | Which corner is the hot area (TL/TR/BL/BR). |
| `hotarea_size` | px | Size of the hot-corner activation box. |
| `ov_tab_mode` | 0/1 | Alt+Tab style overview (tile cycler) vs grid mode. |
| `overview_dim` | 0/1 | Fade a dim backdrop behind the overview grid. |
| `overview_dim_alpha` | 0..1 | Backdrop opacity. |
| `overview_borderpx` | px | Border width on the hovered/selected overview tile. |

### Scroller layout

`scroller_structs`, `scroller_default_proportion`, `scroller_default_proportion_single`,
`scroller_focus_center`, `scroller_prefer_center`, `scroller_prefer_overspread`,
`scroller_ignore_proportion_single`, `scroller_proportion_preset` (comma list),
`scroller_top_gap` (-1 centers, ≥0 anchors px below top), `edge_scroller_pointer_focus`.

## Appearance — borders & colors

| Key | Values | Meaning |
|-----|--------|---------|
| `borderpx` | px | Border width. |
| `border_radius` | px | Rounded-corner radius (scenefx; 0 = square). |
| `no_render_border` | 0/1 | Disable border rendering. |
| `focused_opacity` / `unfocused_opacity` | 0..1 | Per-window opacity (scenefx). |
| `rootcolor` | `0xRRGGBBAA` | Background behind windows. |
| `bordercolor` / `focuscolor` / `urgentcolor` | `0xRRGGBBAA` | Border colors. |
| `dropcolor` / `splitcolor` / `globalcolor` / `overlaycolor` / `scratchpadcolor` / `maximizescreencolor` | `0xRRGGBBAA` | State indicator colors. |
| `allow_lock_transparent` | 0/1 | Allow transparency while session locked. |
| `allow_csd` | 0/1 | Allow client-side decorations. |

### Backdrop blur (scenefx, off by default)

All blur is gated on `blur=1`. Each pass costs GPU shader time per frame —
opt in only if you want it.

| Key | Values | Meaning |
|-----|--------|---------|
| `blur` | 0/1 | Master switch for backdrop blur. |
| `blur_layer` | 0/1 | Also blur layer-shell surfaces (panels, etc). |
| `blur_optimized` | 0/1 | Use the dirty-tile optimizer (cheaper). |
| `blur_params_num_passes` | 0..10 | Two-pass Kawase iterations. |
| `blur_params_radius` | 0..100 | Sample radius per pass. |
| `blur_params_noise` | 0..1 | Dithering noise to break banding. |
| `blur_params_brightness` / `_contrast` / `_saturation` | 0..1 / 0..1 / float | Post-blur color tweaks. |

### Drop shadow (scenefx, off by default)

| Key | Values | Meaning |
|-----|--------|---------|
| `shadows` | 0/1 | Master switch for client drop shadows. |
| `shadow_only_floating` | 0/1 | Only paint shadows on floating windows. |
| `layer_shadows` | 0/1 | Extend shadows to layer-shell surfaces. |
| `shadows_size` | 0..100 | Shadow spread in px. |
| `shadows_blur` | 0..100 | Shadow blur radius. |
| `shadows_position_x` / `_y` | -1000..1000 | Shadow offset. |
| `shadowscolor` | `0xRRGGBBAA` | Shadow color. |

## Cursor

`cursor_size`, `cursor_theme`, `cursor_hide_timeout` (s).

## Input — keyboard

`xkb_rules_layout`, `xkb_rules_variant`, `xkb_rules_model`, `xkb_rules_rules`,
`xkb_rules_options`, `repeat_rate`, `repeat_delay`, `numlockon`.

## Input — pointer / touchpad

| Key | Meaning |
|-----|---------|
| `disable_trackpad` | Disable the touchpad. |
| `tap_to_click`, `tap_and_drag`, `drag_lock` | Tap behaviors. |
| `disable_while_typing` | Palm/typing rejection. |
| `left_handed`, `middle_button_emulation` | Button behavior. |
| `mouse_accel_profile` / `trackpad_accel_profile` | 0=none,1=flat,2=adaptive. |
| `mouse_accel_speed` / `trackpad_accel_speed` | -1..1. |
| `mouse_natural_scrolling` / `trackpad_natural_scrolling` | Reverse scroll. |
| `trackpad_scroll_factor` / `axis_scroll_factor` | Scroll multiplier. |
| `scroll_method`, `scroll_button`, `click_method`, `button_map`, `send_events_mode` | libinput config. |
| `swipe_min_threshold` | Min swipe distance for a gesture. |
| `enable_floating_snap` / `snap_distance` | Edge snapping for floats. |
| `drag_tile_to_tile` / `drag_tile_small` | Drag-tile behaviors. |
| `drag_floating_refresh_interval` / `drag_tile_refresh_interval` | ms throttle on drag-redraw. |
| `drag_corner` / `drag_warp_cursor` | Resize-by-corner / warp cursor with drag. |

## Monitors — `monitorrule`

```
monitorrule=name:^eDP-1$,width:1920,height:1080,refresh:60,x:0,y:0,scale:1
```
Fields: `name` (regex), `make`, `model`, `serial`, `width`, `height`, `refresh`,
`x`, `y`, `scale`, `rr` (transform), `vrr` (adaptive sync). Globals: `vrr`,
`allow_tearing`, `force_tearing`.

## Rules — `windowrule` / `layerrule` / `tagrule`

`windowrule` matches by `appid`/`title`/`id` (regex) and applies: `isfloating`,
`isfullscreen`, `isfakefullscreen`, `isnoborder`, `isnoshadow`, `isnoradius`,
`isnoanimation`, `nofadein`, `nofadeout`, `nofocus`, `isoverlay`,
`isglobal`/`isunglobal`, `isterm`, `noswallow`, `noblur`, `isnamedscratchpad`,
`isopensilent`, `istagsilent`, `open_as_floating`, `no_force_center`,
`no_hide`, `force_tiled_state`, `force_fakemaximize`,
`indleinhibit_when_focus`, `allow_shortcuts_inhibit`, `tags`, `monitor`,
`width`, `height`, `offsetx`, `offsety`, `scroller_proportion`. `layerrule`
keys: `layer_name`, `noblur`, `noanim`, `noshadow`, animation overrides.
`tagrule`: `layout_name`, `mfact`, `nmaster`, `gaps`, `monitor`, per-tag
layout.

## Idle & Power

| Key | Values | Meaning |
|-----|--------|---------|
| `idle_timeout` | seconds (0=off) | Built-in idle timer. |
| `idle_action` | `off`/`suspend`/`hibernate` | What runs at timeout (`off`=DPMS blank). |
| `idlebind` | `SECONDS,dispatch,args` | Native per-action idle timers (multiple). |
| `idleinhibit_ignore_visible` | 0/1 | Honor idle inhibitors even when hidden. |
| `pre_idle_dim` | 0/1 | Spring-dim the backlight before full idle. |
| `pre_idle_dim_lead` | seconds | How long before `idle_timeout` dimming starts. |
| `pre_idle_dim_floor` | 1..100 | Brightness % to dim down to (input springs it back). |
| `idle_timeout_battery` | seconds | Shorter idle timeout on battery (0 = reuse `idle_timeout`). |
| `battery_fps` | 1..240 | Cap animation frame rate while on battery. |
| `battery_timer_slack_ms` | ms | Widen kernel timer slack on battery for deeper C-states. |

## Performance / QoS

| Key | Values | Meaning |
|-----|--------|---------|
| `syncobj_enable` | 0/1 | Explicit GPU sync (linux-drm-syncobj-v1). |
| `subpixel_rgb` | 0/1 | Force horizontal-RGB subpixel hint for LCD subpixel AA (RGB-stripe panels only). |
| `late_latch` | 0/1 | Defer each render to just before vblank to latch fresher input. Bypassed when tearing. |
| `latency_margin_us` | µs | Safety budget reserved before vblank when `late_latch=1`. |
| `cpu_dma_latency_us` | µs / -1 | Hold a low CPU DMA-wakeup-latency request (shallower C-states). -1 = off. |
| `focus_qos` | 0/1 | Renice + ioprio focused vs background process group. |
| `focus_qos_bg_nice` | 1..19 | Background niceness (needs `CAP_SYS_NICE` to restore). |
| `tag_suspend_hidden` | 0/1 | Send xdg-suspended to hidden-tag windows (0 avoids white-flash). |
| `debug_frametime` | 0/1 | Log per-frame timing budget to stderr (for tuning). |

## Clipboard history (built-in)

RAM-only ring of past `text/plain` selections, flushed on every compositor
restart. The picker is a small popup at the bottom-centre of the focused
monitor; navigate with Up/Down/PgUp/PgDn/Home/End, Enter pastes, Escape
cancels. No external clipboard manager required.

| Key | Values | Meaning |
|-----|--------|---------|
| `clipboard_history` | 0/1 | Master switch. |
| `clipboard_history_max_entries` | 1..1000 | Ring size. |
| `clipboard_history_max_bytes` | 1024..67108864 | Per-entry byte cap; bigger selections are dropped. |

Dispatches: `toggle_clipboard_history` (open/close), plus
`clipboard_history_select_next` / `_select_prev` / `_pick` / `_cancel` for
explicit binds if you don't want the popup's built-in key handling.

## Environment & autostart

| Key | Form | Meaning |
|-----|------|---------|
| `env` | `env=NAME,VALUE` | Set an environment variable. |
| `exec` | `exec=cmd` | Run on every config reload. |
| `exec-once` | `exec-once=cmd` | Run once at startup. |

## Bindings

| Form | Meaning |
|------|---------|
| `bind=MODS,KEY,dispatch,args` | Keyboard binding. |
| `mousebind=MODS,btn_left/right/middle,dispatch,args` | Mouse binding (bare `NONE` on left/right rejected). |
| `axisbind=MODS,UP/DOWN,dispatch` | Scroll-wheel binding. |
| `gesturebind=MODS,dir,fingers,dispatch,args` | Touchpad swipe gesture (3+ fingers). |
| `switchbind=fold,dispatch,args` | Laptop lid/switch binding. |
| `globalkeybinding` / `globalcolor` | Global (always-active) bindings. |

Dispatch names map to the actions in `src/dispatch/bind_define.h` (e.g. `spawn`,
`spawn_shell`, `killclient`, `togglefloating`, `view`, `tag`, `focusstack`,
`focusdir`, `moveresize`, `setlayout`, `swipe_layout_dir`, `exchange_client`,
`toggleoverview`, `reload_config`, …).

### Gesture conventions

The default config wires touchpad swipes as:

- **3-finger left / right / up** → `swipe_layout_dir` — auto-switch to the
  layout that places the master area on that side (`tile` / `right_tile` /
  `vertical_tile`) and promote the focused client to master.
- **3-finger down** → `minimized` — minimize the focused client.
- **4-finger any direction** → `exchange_client` — swap the focused client
  with the tiled neighbor on that side; works in every layout.

Minimized clients reappear inside the overview and are restored on
left-click; clients you don't pick during an overview pass are
re-minimized on exit.
