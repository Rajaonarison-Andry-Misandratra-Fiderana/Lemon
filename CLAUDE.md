# CLAUDE.md

Guidance for Claude (and other AI assistants) working in this repository.

## What this is

Lemon is a Wayland tiling compositor written in C. It is a fork of
[mangowm](https://github.com/DreamMaoMao/mangowm), which itself derives from
`dwl`. Built on **wlroots 0.19** and **scenefx 0.4**. The fork prioritises low
latency and short build time.

## Build & run

```bash
meson setup build -Dprefix=/usr      # configure (once, or with --reconfigure)
ninja -C build                       # compile
sudo ninja -C build install          # install to /usr
build/lemon                          # run uninstalled (nested wayland session)
```

Useful options live in `meson_options.txt`: `native`, `lto`, `jemalloc`, `asan`,
`xwayland`, `pgo`, `pgo_dir`. Release builds use `-O3 -fno-plt
-fno-semantic-interposition -fvisibility=hidden -ffunction-sections
-fdata-sections -fmerge-all-constants` plus opportunistic GCC IPA passes
(`-fipa-pta`, `-fdevirtualize-speculatively`, `-fpredictive-commoning`,
`-ftree-vectorize`) and `-fno-trapping-math -fno-math-errno
-fno-signed-zeros` for the animation curve hot path. Linker uses
`-Wl,--gc-sections -Wl,--hash-style=gnu -Wl,--build-id=sha1
-Wl,-z,now,relro,noseparate-code`. Debug builds use `-O0 -g`.

**Two-pass PGO** for the lowest-latency build:

```bash
meson setup build-pgo --buildtype=release -Dpgo=generate
ninja -C build-pgo
build-pgo/lemon                  # run normally for 5–10 minutes, exit
meson configure build-pgo -Dpgo=use
ninja -C build-pgo               # produces the PGO-optimized binary
```

Run from inside an existing Wayland or X session for development:

```bash
WAYLAND_DISPLAY=wayland-1 build/lemon -c assets/lemon.conf
```

There is **no test suite**. Validation = clean compile + manual smoke test in a
nested session.

## Single-TU architecture (important)

`src/lemon.c` is the **only** `.c` file built into the `lemon` binary (plus
`src/common/util.c` and the generated wayland protocol sources). Almost every
other file under `src/` is a `.h` that contains *function definitions* and is
included exactly once from `lemon.c`. Editing those headers is editing the
implementation, not declaring an API.

Why: single translation unit → whole-program optimisation and fast builds.
Side effect: order of `#include` inside `lemon.c` matters, and adding a new
file means adding an `#include` to `lemon.c`.

`mmsg/mmsg.c` is built as a separate small binary; it does not link against
the compositor.

## Code map

| Path | Role |
|------|------|
| `src/lemon.c` | Entry point, types, macros, globals, forward declarations, `setup()` / `run()` / `cleanup()` / `main()`. ~4.2k lines after the split below; rest of the compositor lives in single-TU headers included exactly once from here. |
| `src/common/util.{c,h}` | `die`, `ecalloc`, `regex_match` (PCRE2 LRU 32 + JIT), frame-clock cache (`frame_clock_begin/end`, `frame_now_ms`), string helpers. The only `.c` besides `lemon.c`. |
| `src/common/surface_cache.h` | LRU 256-entry `app_id → (w,h)` persisted to `$XDG_CACHE_HOME/lemon/surfaces.db`. |
| `src/config/parse_config.h` | Config-file parser. ~4k lines. Reads `~/.config/lemon/lemon.conf`, validates, hot-reloads on save. |
| `src/config/preset.h` | Default values when a config key is absent. |
| `src/client/client.h` | `Client` struct operations: focus, geometry, state flags, tag mask. |
| `src/client/rules.h` | `applyrules` — pick monitor / tags / size, run swallow handshake, hand off to `setmon` / `setfullscreen`. |
| `src/client/states.h` | `setmaximizescreen`, `setfakefullscreen`, `setfullscreen` — state transitions + scene-tree reparent. |
| `src/layout/layout.h` | `Layout` registry — symbol, name, arrange-fn per tiling algorithm. |
| `src/layout/arrange.h` | Top-level arrange dispatcher + per-tag layout selection. |
| `src/layout/{scroll,dwindle,vertical,horizontal}.h` | Individual layout algorithms. |
| `src/layout/layers.h` | `arrangelayer(s)` (exclusive-zone shrink pass), `apply_window_snap`, `focuslayer`, `reset_exclusive_layers_focus`. |
| `src/animation/{client,layer,tag,common}.h` | Per-object animation. Driven by frame clock in `util.h`. Geometry uses spring physics; open/close still use the bezier model. |
| `src/animation/spring.h` | Damped-harmonic-oscillator integrator (semi-implicit Euler, allocation-free). Sub-steps when wall-clock `dt > 8 ms` so battery/VRR low-refresh stays accurate. |
| `src/input/motion.h` | `motionabsolute` / `motionnotify` / `motionrelative` + `resize_floating_window`. |
| `src/input/pointer.h` | `axisnotify`, `ongesture`, swipe / pinch / hold gesture listeners. |
| `src/input/button.h` | `buttonpress` — cycler / clipboard click intercept, mouse-binding dispatch, drag/resize release with the fling velocity hand-off. |
| `src/input/keypress.h` | `keypress` + `keypressmod`. Cycler / clipboard key intercepts, configured cycler-modifier release commit, digit jump, Escape dismiss, Alt+Q kill, normal keybinding pass. |
| `src/input/cursor.h` | `idle_notify_throttled` (250 ms gate), `handlecursoractivity`, `hidecursor`, `keep_idle_inhibit`. |
| `src/input/devices.h` | libinput accel + tap config, `createpointer`, `createswitch`, `createpointerconstraint`, shared `destroyinputdevice`. |
| `src/output/render.h` | Per-monitor frame path: `rendermon`, `do_rendermon`, `render_layer_surfaces`, `render_fadeouts`, `render_clients`, `render_overview_dim`, `schedule_next_frame`, `render_deadline_callback`, asymmetric render-time EMA. |
| `src/output/outputmgr.h` | `outputmgrapply` / `outputmgrapplyortest` / `outputmgrtest` (wlr_output_management_v1). |
| `src/output/updatemons.h` | `updatemons` — output-layout reconfigure: tear down disabled, auto-layout, drag floating clients, arrange. |
| `src/lifecycle/client.h` | `setmon` (keeps `Monitor.clients` / `Client.mon_link` consistent) and `unmapnotify`. |
| `src/lifecycle/focus.h` | `focusclient`. |
| `src/lifecycle/xdg.h` | `createnotify`, `mapnotify`, `destroynotify`, `fullscreennotify`, `maximizenotify`, `minimizenotify`, `unminimize`, `set_minimized`. |
| `src/lifecycle/listeners.h` | `commitnotify`, popup helpers, `createdecoration`, `createidleinhibitor`, `createkeyboard`, `createkeyboardgroup`, `createlayersurface`, `createlocksurface`, `apply_rule_to_state`, `monitor_matches_rule`, `enable_adaptive_sync`. |
| `src/lifecycle/destroy.h` | Cursor `cursorconstrain` / `cursorframe` / `cursorwarptohint` + small destroy listeners (drag icon, idle inhibitor, layer node, session lock + lock surface, pointer constraint, keyboard group). |
| `src/lifecycle/monitor.h` | `createmon`, `cleanupmon`, `closemon`. |
| `src/lifecycle/xwayland.h` | Whole block wrapped in `#ifdef XWAYLAND`: `activatex11`, `configurex11`, `createnotifyx11`, `commitx11`, `associatex11`, `dissociatex11`, `sethints`, `xwaylandready`, `setgeometrynotify`, `fix_xwayland_unmanaged_coordinate`, `synckeymap`. |
| `src/idle.h` | `idle_action_callback`, pre-idle backlight dimming (`predim_anim_tick`, `predim_lead_callback`, `predim_restore_and_rearm`), `idle_timer_callback`, `reset_idle_timers`, `setup_idle_timers`, `destroy_idle_timers`. |
| `src/power.h` | `detect_on_battery`, `apply_battery_timer_slack`, `apply_cpu_dma_latency`, `battery_poll_callback`, `client_has_idle_inhibitor`, `client_hibernate_scan_callback`, `battery_frame_throttle_callback`. |
| `src/clipboard/clipboard.h` | Built-in clipboard history (RAM-only ring) + bottom-centre picker popup. Captures text **and** image MIMEs (PNG / JPEG / WEBP / GIF / TIFF / BMP) via the `setsel` listener, decodes PNG thumbnails lazily, pastes back via a `ClipPasteSource` whose write path is non-blocking with an event-loop `ClipPasteFlight` drain so large payloads do not deadlock the main loop. |
| `src/dispatch/bind_declare.h` | Forward decls of every keybind action. |
| `src/dispatch/bind_define.h` | Bodies of those actions. |
| `src/dispatch/cycler.h` | Alt+Tab cycler: resizes the real client windows into a grid (same algorithm as the overview layout), overlays selection / hover borders + numbered badges (1..9) anchored on each grid cell, supports `Mod+digit` direct jump, BTN_LEFT drag swaps two cells (also reorders the global `clients` wl_list so tile layouts pick up the new order on restore). |
| `src/fetch/{client,monitor,common,fetch}.h` | Read-only queries — iterate clients/monitors with filters. |
| `src/ext-protocol/dwl-ipc.h` | dwl-ipc-unstable-v2 server impl (state broadcasts to `mmsg`). |
| `src/ext-protocol/ext-workspace.h` + `wlr_ext_workspace_v1.{c,h}` | ext-workspace-v1 protocol. |
| `src/ext-protocol/foreign-toplevel.h` | wlr-foreign-toplevel-management. |
| `src/ext-protocol/tearing.h` | tearing-control-v1. |
| `src/ext-protocol/text-input.h` | text-input v2/v3 for IMEs. |
| `src/ext-protocol/all.h` | Single include that pulls in the rest. |
| `src/data/static_keymap.h` | Hard-coded fallback keymap. |
| `mmsg/mmsg.c` | CLI client speaking dwl-ipc. Separate binary. |
| `protocols/` | XML protocol definitions; consumed by `wayland-scanner` in `protocols/meson.build`. |
| `assets/lemon.conf` | Annotated example config — canonical config reference. |
| `assets/lemon.desktop` | Wayland session entry (installed to `share/wayland-sessions`). |
| `assets/lemon-portals.conf` | xdg-desktop-portal preference. |
| `assets/lemon-logo.svg` | Flat lemon-with-sunglasses brand mark. |

## Conventions

- **Comments**: one short English `/* ... */` line above every function definition, stating its purpose. No inline per-line noise. No multi-paragraph docstrings.
- **Function signatures**: actions invoked by keybinds return `int32_t` and take `const Arg *`. See `src/dispatch/bind_declare.h`.
- **Listeners**: attached via `LISTEN(signal, listener_field, handler)` and `LISTEN_STATIC(signal, handler)` macros in `lemon.c`.
- **Predicate macros**: `ISTILED`, `ISSCROLLTILED`, `VISIBLEON`, `ISFULLSCREEN`, `INSIDEMON` — defined at the top of `lemon.c`. Prefer these over re-deriving the checks.
- **Geometry**: clients use `geom` (`struct wlr_box`) for current, `prev_geom` for pre-floating restore.
- **Tags**: a tag set is a bitmask in `unsigned int`. `TAGMASK` = `(1 << LENGTH(tags)) - 1`.
- **Frame clock**: inside a render frame, call `frame_now_ms()` instead of `get_now_in_ms()` to avoid a syscall per animated client. `frame_clock_begin/end` bracket the frame in `lemon.c`. Both `frame_now_ms` and `frame_clock_now_timespec` are inlined in `util.h`.
- **String helpers**: `string_printf` (allocates), `join_strings`, `join_strings_with_suffix` — in `util.c`.
- **Allocator**: `ecalloc` (calloc-or-die) is the standard; raw `malloc` only when failure is recoverable.
- **Regex**: `regex_match(pattern, str)` — PCRE2 with LRU cache (32 entries) **and JIT compile** in `util.c`. Matches pass `PCRE2_NO_UTF_CHECK`.
- **Hot/cold attributes**: use `LEMON_HOT` / `LEMON_COLD` (defined in `util.h`) on per-frame render/input handlers and on cold error/setup paths; use `LEMON_LIKELY` / `LEMON_UNLIKELY` for the dominant-vs-rare branch hint.
- **Blur/shadow code present but disabled by default**: `wlr_scene_optimized_blur`, `wlr_scene_shadow`, `config.blur*`, `config.shadows*`, per-rule `noblur`/`noshadow`/`isnoshadow`, and the `LyrBlur` layer are all wired up, but every gating default (`config.blur`, `config.blur_layer`, `config.shadows`, `config.layer_shadows`) is `0` so no blur/shadow scene nodes are created at runtime. Users opt in via config; do not flip the defaults to on without explicit request.
- **Spring animations**: window geometry (move/resize/tile/overview/tag) is driven by a damped-harmonic-oscillator spring in `src/animation/spring.h` (semi-implicit Euler, zero alloc). Per-client state lives in `dwl_animation` (`vis[4]`, `vel[4]`, `spring_init`, `last_tick_ms`); target is `c->current`. Fully interruptible — `client_commit` retargets without resetting velocity while running. Settle (within pos+vel epsilon) snaps to the integer target and clears `running` so the compositor sleeps. Friction is clamped `>=1` so it always settles. Open/close keep the bezier curve model (they need a 0..1 progress for zoom/fade). Config: `animation_spring`, `animation_spring_{mass,tension,friction}`, and a separate `animation_spring_tag_{tension,friction}` for faster workspace switches.
- **Config-driven extras** (all in `parse_config.h` struct + preset + parse + clamp, documented in `assets/lemon.conf`): `idlebind=SECONDS,dispatch,args` (native idle timers) and built-in `idle_timeout`/`idle_action` (off=DPMS / suspend / hibernate); `focus_qos`/`focus_qos_bg_nice` (renice+ioprio focused vs background process group, needs `CAP_SYS_NICE` to raise); `tag_suspend_hidden` (default 0 — suspending hidden-tag windows blanked some apps → white flash on switch); `touchpad_4f_osd`/`touchpad_4f_step` (vertical 4-finger swipe → continuous volume via swayosd, handled live in `swipe_update`).
- **Battery awareness**: global `on_battery` (polled every 10s from `/sys/class/power_supply/AC*/online`). When true, `rendermon` throttles animation frame scheduling to `BATTERY_ANIM_INTERVAL_MS` (16 ms ≈ 60 fps) via a per-monitor deferred timer.
- **Per-monitor render loop**: `rendermon` (split into `render_layer_surfaces`/`render_fadeouts`/`render_clients`/`schedule_next_frame` in `src/output/render.h`) iterates the per-monitor index `m->clients` (linked via `Client.mon_link`, maintained by `setmon` in `src/lifecycle/client.h`) instead of filtering the global list — multi-output cost is `O(per_monitor)`, not `O(N_clients × N_outputs)`. Animation start uses `request_fresh_for_client(c)` to wake only monitors where the client is currently rendered. The scene commit is gated by `output_should_commit` (scene damage or a pending legacy `wlr_screencopy_v1` capture). The render-time EMA driving the late-latch deadline is asymmetric: 1/2 weight up (react fast to spikes), 1/16 weight down (decay slowly).
- **Idle-notify throttle is centralised**: every input path (`motionnotify`, `axisnotify`, `buttonpress`, `keypress`) calls `idle_notify_throttled(false)` from `src/input/cursor.h`; the helper enforces a single 250 ms gate. Don't add a per-event notify outside that gate.
- **Trackpad gestures are first-class**: `swipe_update` in `src/input/pointer.h` directly drives the layout pick / window swap / swayosd volume bar without going through `wlr_pointer_gestures_v1` passthrough. Keep that.
- **Alt+Tab cycler resizes real windows**: `src/dispatch/cycler.h`'s `window_cycler_build` snapshots each visible client's geometry, sets `c->geom` to its grid cell and calls `resize(c, …, 0)` so the spring animation moves the live window into place. The overlay on `LyrFadeOut` is *only* selection + hover borders and number badges — no thumbnail compositing. The dim backdrop lives on `LyrBottom` so real windows draw on top of it. `window_cycler_destroy` restores every backed-up geom/state and triggers `arrange(selmon, false, false)` so the new client order (after any drag-swap) tiles correctly. Don't reintroduce snapshot-based thumbnails: real windows give pixel-perfect previews with zero scaling cost.
- **Alt+Tab drag swaps tile order**: pressing BTN_LEFT on a cell + dragging past the 8px threshold hoists the dragged client to `LyrTop`, follows the cursor via `resize()`, and on release swaps two cells. The swap also pulls the corresponding `Client` entries inside the global `clients` wl_list, so the post-restore `arrange()` lays out tiles in the new order. Pure click (no drag) commits the picked cell like before.
- **Clipboard supports image entries**: `ClipEntry` carries a heap-owned `mime` (`text/plain;charset=utf-8`, `image/png`, etc.). The paste source only advertises the captured MIME, so binary bytes do not leak into a text input. `clip_paste_send` is non-blocking with a `ClipPasteFlight` event-loop drain — large images do not deadlock the main loop. Locked-RSS guard: `mlockall(MCL_CURRENT|MCL_ONFAULT)` pins clipboard buffers, so the default ring is `clipboard_history_max_entries=20 × clipboard_history_max_bytes=8 MiB` (worst-case 160 MiB pinned). Bumping either of those eats locked RAM.
- **App-launch latency**: `spawn` / `spawn_shell` use `posix_spawn` instead of `fork+exec` to skip the full page-table COW of the compositor's address space. Don't reintroduce `fork()` for child launching. Cursor theme is preloaded at scales 1 and 2 in `setup`. xdg-desktop-portal is warm-pinged at the end of `run` so the first GTK/Qt client does not pay the cold-start. Children are reset to `SCHED_OTHER` nice 0 via `POSIX_SPAWN_SETSCHEDULER` so they never inherit the compositor's realtime class.
- **Realtime scheduling**: at `main()`, the compositor attempts `sched_setscheduler(0, SCHED_RR, prio=1)` and falls back to `setpriority(-10)` on failure. `mlockall(MCL_CURRENT|MCL_ONFAULT)` pins the working set. Requires `/etc/security/limits.d/` rtprio + memlock entries for unprivileged users (typically `@video - rtprio 10`, `@video - memlock 524288`).
- **Idle-notify throttling**: `wlr_idle_notifier_v1_notify_activity` is rate-limited to 4 Hz in `motionnotify` to avoid D-Bus flood under high-poll mice. Keep this throttle when extending; do not add a per-event notify.
- **Surface geometry cache**: `src/common/surface_cache.h` maintains an LRU 256-entry `app_id → (w,h)` map persisted to `$XDG_CACHE_HOME/lemon/surfaces.db`. Used at xdg initial_commit to send the first configure at the right size and skip a resize round-trip.
- **Render tiers**: each `Client` carries `render_tier` (FOCUS/VISIBLE/OCCLUDED/HIDDEN). Recomputed in `focusclient` and after `arrange`. `client_draw_frame` throttles OCCLUDED to 30 Hz and skips HIDDEN entirely.
- **Renderer**: scenefx GLES2 `fx_renderer`. The scenefx scene tree asserts `wlr_renderer_is_fx()` at runtime, so pairing it with a plain wlroots renderer (e.g. Vulkan) aborts the compositor. Switching to Vulkan requires either a scenefx Vulkan port upstream or replacing `scenefx/types/wlr_scene.h` with vanilla `wlr/types/wlr_scene.h` (a sizeable refactor that loses corner radius / per-buffer opacity / snapshot fadeout). Don't reintroduce a `LEMON_RENDERER` env switch without dropping the scenefx scene tree first.

## Common tasks

### Add a keybind action

1. Declare in `src/dispatch/bind_declare.h`: `int32_t myaction(const Arg *arg);`
2. Define in `src/dispatch/bind_define.h` with a `/* ... */` utility comment.
3. Reference by name from the config (`bind = MOD+KEY,myaction,arg`). The parser in `src/config/parse_config.h` maps strings to function pointers.

### Add a layout

1. New header `src/layout/<name>.h` exposing `static void <name>(Monitor *m);`
2. Forward-declare in `src/layout/layout.h`, add an enum entry and a row in `layouts[]`.
3. Include from `src/lemon.c` (order matters — after `layout.h`).

### Add a config option

1. Extend the relevant struct in `src/config/parse_config.h`.
2. Add the default in `src/config/preset.h`.
3. Add a parse case in `parse_config.h`'s key dispatch.
4. Document in `assets/lemon.conf` so users see it.

### Add a Wayland protocol

1. Drop the XML in `protocols/` and wire it into `protocols/meson.build`.
2. Implement in a new `src/ext-protocol/<name>.h` (or a `.c` if it's large enough to warrant separate compilation; current pattern is header-only).
3. Include from `src/lemon.c`, instantiate, wire listeners.

### Add a new module / split an existing one

1. Pick a directory matching its responsibility (`input/`, `output/`, `lifecycle/`, `client/`, `layout/`, `idle.h`, `power.h`, ...).
2. Create `src/<dir>/<name>.h` with `#pragma once` and function definitions (no declarations-only).
3. `#include "<dir>/<name>.h"` exactly once from `lemon.c`, **after** the forward declarations + globals + types it needs. Order matters: a callee included earlier must have its dependencies forward-declared at the top of `lemon.c`.
4. If you move a function that another file calls before its new include site, add a `static` forward declaration to `lemon.c`'s forward-decl block.

## Out of scope

- **NixOS support is removed.** Do not reintroduce `flake.nix`, `nix/`, or `docs/nix-options.md` without an explicit user request.
- **Guix channel file (`lemonwm.scm`) is removed.** Same — leave it out unless asked.
- **There is no test suite.** A minimal `.github/workflows/build.yml` exists that only validates the build matrix; do not extend it into a unit/integration framework without an explicit request.
- **Do not add a `CHANGELOG.md`** unless asked; git log is the source of truth.

## Gotchas

- The build dir `build/` is gitignored but present on disk after configuration. Don't accidentally edit files there — they're generated.
- The `wlr-layer-shell-unstable-v1-protocol.h` is generated; if you grep and only find the include, look in `protocols/` for the XML.
- `XWAYLAND` is a compile-time gate (`-DXWAYLAND` set when `xcb` + `xcb-icccm` are found). Code under `#ifdef XWAYLAND` won't compile on systems without those deps.
- Config sysconfdir is hard-coded to `/etc` in `meson.build` via `-DSYSCONFDIR="/etc"`. The default user config gets installed under `/etc/lemon/`.
- `frame_clock_*` is *not* thread-safe and not re-entrant; it's deliberately scoped to the main loop's render path.
