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

### Spring physics (geometry: move / resize / tile / overview / tag)

| Key | Values | Meaning |
|-----|--------|---------|
| `animation_spring` | 0/1 | Use spring physics for geometry (else bezier). |
| `animation_spring_mass` | float | Spring mass `m` (heavier = slower). |
| `animation_spring_tension` | float | Stiffness `k` (higher = faster). |
| `animation_spring_friction` | float | Damping `c`. `c ≈ 2·√(k·m)` = no overshoot; lower = bounce. Clamped ≥1. |
| `animation_spring_tag_tension` | float | Separate, usually faster tension for workspace switch. |
| `animation_spring_tag_friction` | float | Damping for the tag spring. |

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

### Scroller layout

`scroller_structs`, `scroller_default_proportion`, `scroller_default_proportion_single`,
`scroller_focus_center`, `scroller_prefer_center`, `scroller_prefer_overspread`,
`scroller_ignore_proportion_single`, `scroller_proportion_preset` (comma list),
`edge_scroller_pointer_focus`.

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
| `touchpad_4f_osd` | 0/1 — vertical 4-finger swipe → live volume OSD. |
| `touchpad_4f_step` | px of travel per swayosd step (smaller = more sensitive). |
| `enable_floating_snap` / `snap_distance` | Edge snapping for floats. |
| `enable_hotarea` / `hotarea_corner` / `hotarea_size` | Hot-corner overview trigger. |

## Monitors — `monitorrule`

```
monitorrule=name:^eDP-1$,width:1920,height:1080,refresh:60,x:0,y:0,scale:1
```
Fields: `name` (regex), `make`, `model`, `serial`, `width`, `height`, `refresh`,
`x`, `y`, `scale`, `rr` (transform), `vrr` (adaptive sync). Globals: `vrr`,
`allow_tearing`, `force_tearing`.

## Rules — `windowrule` / `layerrule` / `tagrule`

`windowrule` matches by `appid`/`title`/`id` (regex) and applies: `isfloating`,
`isfullscreen`, `isfakefullscreen`, `isnoborder`, `isnoradius`, `isnoanimation`,
`nofadein`, `nofadeout`, `nofocus`, `isoverlay`, `isglobal`/`isunglobal`,
`isterm`, `noswallow`, `isnamedscratchpad`, `isopensilent`, `istagsilent`,
`open_as_floating`, `no_force_center`, `no_hide`, `force_tiled_state`,
`force_fakemaximize`, `indleinhibit_when_focus`, `tags`, `monitor`, `width`,
`height`, `offsetx`, `offsety`, `scroller_proportion`. `layerrule` keys:
`layer_name`, animation overrides. `tagrule`: `layout_name`, `mfact`, `nmaster`,
`gaps`, `monitor`, per-tag layout.

## Idle & Power

| Key | Values | Meaning |
|-----|--------|---------|
| `idle_timeout` | seconds (0=off) | Built-in idle timer. |
| `idle_action` | `off`/`suspend`/`hibernate` | What runs at timeout (`off`=DPMS blank). |
| `idlebind` | `SECONDS,dispatch,args` | Native per-action idle timers (multiple). |
| `idleinhibit_ignore_visible` | 0/1 | Honor idle inhibitors even when hidden. |

## Performance / QoS

| Key | Values | Meaning |
|-----|--------|---------|
| `syncobj_enable` | 0/1 | Explicit GPU sync (linux-drm-syncobj-v1). |
| `focus_qos` | 0/1 | Renice + ioprio focused vs background process group. |
| `focus_qos_bg_nice` | 1..19 | Background niceness (needs `CAP_SYS_NICE` to restore). |
| `tag_suspend_hidden` | 0/1 | Send xdg-suspended to hidden-tag windows (0 avoids white-flash). |

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
`focusdir`, `moveresize`, `setlayout`, `toggleoverview`, `reload_config`, …).
