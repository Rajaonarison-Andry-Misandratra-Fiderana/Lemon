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
| `src/lemon.c` | Entry point, macros, types, main event loop, wlroots wiring, all top-level wayland listeners. ~7k lines. |
| `src/common/util.{c,h}` | `die`, `ecalloc`, `regex_match` (with LRU cache), frame-clock cache (`frame_clock_begin/end`, `frame_now_ms`), string helpers. The only `.c` besides `lemon.c`. |
| `src/config/parse_config.h` | Config-file parser. ~4k lines. Reads `~/.config/lemon/lemon.conf`, validates, hot-reloads on file change. |
| `src/config/preset.h` | Default values used when a config key is absent. |
| `src/client/client.h` | `Client` struct operations: focus, geometry, state flags, tag mask. |
| `src/layout/layout.h` | `Layout` registry — symbol, name, arrange-fn for each tiling algorithm. |
| `src/layout/arrange.h` | Top-level arrange dispatcher + per-tag layout selection. |
| `src/layout/{scroll,dwindle,vertical,horizontal}.h` | Individual layout algorithms. |
| `src/animation/{client,layer,tag,common}.h` | Keyframe interpolation per object kind. Driven by frame clock in `util.h`. |
| `src/fetch/{client,monitor,common,fetch}.h` | Read-only queries — iterate clients/monitors with filters. |
| `src/dispatch/bind_declare.h` | Forward decls of every keybind action (`int32_t name(const Arg *)`). |
| `src/dispatch/bind_define.h` | Bodies of those actions. Adding a keybind = decl here + def here + map in config. |
| `src/ext-protocol/dwl-ipc.h` | dwl-ipc-unstable-v2 server impl (state broadcasts to `mmsg`). |
| `src/ext-protocol/ext-workspace.h` + `wlr_ext_workspace_v1.{c,h}` | ext-workspace-v1 protocol. |
| `src/ext-protocol/foreign-toplevel.h` | wlr-foreign-toplevel-management. |
| `src/ext-protocol/tearing.h` | tearing-control-v1. |
| `src/ext-protocol/text-input.h` | text-input v2/v3 for IMEs. |
| `src/ext-protocol/all.h` | Single include that pulls in the rest. |
| `src/data/static_keymap.h` | Hard-coded fallback keymap. |
| `mmsg/mmsg.c` | CLI client speaking dwl-ipc. Separate binary. |
| `protocols/` | XML protocol definitions; consumed by `wayland-scanner` in `protocols/meson.build`. |
| `assets/lemon.conf` | Annotated example config — the canonical reference for config option names. |
| `assets/lemon.desktop` | Wayland session entry (installed to `share/wayland-sessions`). |
| `assets/lemon-portals.conf` | xdg-desktop-portal preference. |

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
- **No blur, no shadow**: the compositor never enables backdrop blur or drop shadow scene nodes. Animations, opacity fades, corner radius and borders are all kept. Don't reintroduce `wlr_scene_optimized_blur`, `wlr_scene_shadow`, `config.blur*`, `config.shadows*`, `noblur`, or `noshadow` without explicit user request.
- **Battery awareness**: global `on_battery` (polled every 10s from `/sys/class/power_supply/AC*/online`). When true, `rendermon` throttles animation frame scheduling to `BATTERY_ANIM_INTERVAL_MS` (16 ms ≈ 60 fps) via a per-monitor deferred timer.
- **Per-monitor render loop**: `rendermon` iterates only clients whose `c->mon == m`. Animation start uses `request_fresh_for_client(c)` to wake only monitors where the client is currently rendered.
- **App-launch latency**: `spawn` / `spawn_shell` use `posix_spawn` instead of `fork+exec` to skip the full page-table COW of the compositor's address space. Don't reintroduce `fork()` for child launching. Cursor theme is preloaded at scales 1 and 2 in `setup`. xdg-desktop-portal is warm-pinged at the end of `run` so the first GTK/Qt client does not pay the cold-start. Children are reset to `SCHED_OTHER` nice 0 via `POSIX_SPAWN_SETSCHEDULER` so they never inherit the compositor's realtime class.
- **Realtime scheduling**: at `main()`, the compositor attempts `sched_setscheduler(0, SCHED_RR, prio=1)` and falls back to `setpriority(-10)` on failure. `mlockall(MCL_CURRENT|MCL_ONFAULT)` pins the working set. Requires `/etc/security/limits.d/` rtprio + memlock entries for unprivileged users (typically `@video - rtprio 10`, `@video - memlock 524288`).
- **Idle-notify throttling**: `wlr_idle_notifier_v1_notify_activity` is rate-limited to 4 Hz in `motionnotify` to avoid D-Bus flood under high-poll mice. Keep this throttle when extending; do not add a per-event notify.
- **Surface geometry cache**: `src/common/surface_cache.h` maintains an LRU 256-entry `app_id → (w,h)` map persisted to `$XDG_CACHE_HOME/lemon/surfaces.db`. Used at xdg initial_commit to send the first configure at the right size and skip a resize round-trip.
- **Render tiers**: each `Client` carries `render_tier` (FOCUS/VISIBLE/OCCLUDED/HIDDEN). Recomputed in `focusclient` and after `arrange`. `client_draw_frame` throttles OCCLUDED to 30 Hz and skips HIDDEN entirely.
- **Renderer selection**: `LEMON_RENDERER` env var picks the backend.
  - unset (default) → plain `wlr_renderer_autocreate`. Honours `WLR_RENDERER=vulkan|gles2` if set, otherwise wlroots picks (Vulkan when Mesa Vulkan drivers are available, GLES2 fallback). Geometry animations and plain window borders work; corner radius, per-buffer opacity and snapshot-fadeout effects are unused by design.
  - `fx` → scenefx GLES2 `fx_renderer` (opt-in). Re-enables corner radius, per-buffer opacity, and the close-anim snapshot fadeout. Use only if those visuals are wanted at the cost of Vulkan.
  - `vulkan` → sets `WLR_RENDERER=vulkan` before autocreate.
  - `gles2` → forces plain GLES2 backend.
  scenefx 0.4 has no Vulkan path; the default keeps Vulkan available at the cost of scenefx-only effects that this fork does not rely on.

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

## Out of scope

- **NixOS support is removed.** Do not reintroduce `flake.nix`, `nix/`, or `docs/nix-options.md` without an explicit user request.
- **Guix channel file (`lemonwm.scm`) is removed.** Same — leave it out unless asked.
- **No CI / no tests** in-tree currently. Don't fabricate one.
- **Do not add a `CHANGELOG.md`** unless asked; git log is the source of truth.

## Gotchas

- The build dir `build/` is gitignored but present on disk after configuration. Don't accidentally edit files there — they're generated.
- The `wlr-layer-shell-unstable-v1-protocol.h` is generated; if you grep and only find the include, look in `protocols/` for the XML.
- `XWAYLAND` is a compile-time gate (`-DXWAYLAND` set when `xcb` + `xcb-icccm` are found). Code under `#ifdef XWAYLAND` won't compile on systems without those deps.
- Config sysconfdir is hard-coded to `/etc` in `meson.build` via `-DSYSCONFDIR="/etc"`. The default user config gets installed under `/etc/lemon/`.
- `frame_clock_*` is *not* thread-safe and not re-entrant; it's deliberately scoped to the main loop's render path.
