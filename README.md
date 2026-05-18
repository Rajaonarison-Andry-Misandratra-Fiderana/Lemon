# Lemon

A fast, lightweight Wayland tiling window manager / compositor.

Lemon is a fork of [mangowm](https://github.com/DreamMaoMao/mangowm), itself built on
[dwl](https://codeberg.org/dwl/dwl/), [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots)
and [scenefx](https://github.com/wlrfx/scenefx). The fork is tuned for **maximum
responsiveness, minimum latency and longest battery life**:

- Single translation unit + LTO + opportunistic GCC IPA passes + optional PGO
- PCRE2 with JIT-compiled patterns (window/layer rules)
- Inlined frame-clock cache: one `clock_gettime` per frame instead of per animated client
- `LEMON_HOT` / `LEMON_COLD` / `LEMON_LIKELY` / `LEMON_UNLIKELY` on render and input paths
- Per-monitor render loop (clients only ticked on `c->mon`) and per-client wakeups
- Battery-aware adaptive FPS: animations cap at ~60 Hz on battery, full refresh on AC
- `setpriority(-10)` at startup for snappier input under load
- Blur and drop shadows are **not** rendered, keeping GPU work to the strict minimum

## Features

- **Tiling layouts** — scroller, master-stack, monocle, grid, deck, dwindle, horizontal, vertical. Layouts are per-tag.
- **Tags, not workspaces** — multiple tags can be visible at the same time on a monitor.
- **Animations** — open / close / move / tag switch, with per-target easing. Battery-aware FPS cap.
- **Visual effects** — rounded corners, opacity fades, animated borders. No blur, no drop shadow (intentional, keeps GPU idle).
- **XWayland** — first-class support for legacy X11 clients.
- **IPC** — `dwl-ipc-unstable-v2` server + `mmsg` CLI client for scripting and status bars.
- **Hot-reload config** — single text file, reloads without restart.
- **Window states** — floating, fullscreen, maximize, minimize, scratchpad, overlay, swallow.
- **Scratchpads** — both Sway-style and named.
- **Input methods** — text-input v2 / v3 (Fcitx5, IBus).
- **Tearing control** — per-window or fullscreen-only.

## Quick install

Distribution packages are listed in [`docs/installation.md`](docs/installation.md): Arch (AUR), Fedora (Terra), Gentoo (GURU), AerynOS, PikaOS.

## Build from source

Runtime dependencies:

```
wayland           wayland-protocols   libinput
libdrm            libxkbcommon        pixman
libdisplay-info   libliftoff          hwdata
seatd             pcre2               xorg-xwayland
libxcb            xcb-icccm
```

Plus **wlroots 0.19** and **scenefx 0.4** — both must be installed separately if your
distro does not ship recent enough versions. See the build instructions in
[`docs/installation.md`](docs/installation.md#building-from-source).

Then:

```bash
meson setup build -Dprefix=/usr
ninja -C build
sudo ninja -C build install
```

### Optional build flags

| Option | Effect |
|--------|--------|
| `-Dnative=true` | `-march=native -mtune=native` |
| `-Dlto=true` | `-flto=thin` link-time optimisation (on by default) |
| `-Djemalloc=true` | Link against jemalloc (faster allocator) |
| `-Dpgo=generate` | First pass: instrument the build to collect profile data |
| `-Dpgo=use` | Second pass: consume the collected profile data |
| `-Dpgo_dir=PATH` | Override the directory where PGO profiles are written/read |
| `-Dasan=true` | AddressSanitizer for debugging |
| `--buildtype=debug` | `-O0 -g`, skips release flags |

#### Profile-Guided Optimization (PGO)

For the fastest possible build, do a two-pass PGO compile after exercising the
compositor with a realistic workload:

```bash
# 1. Instrumented build
meson setup build-pgo --buildtype=release -Dpgo=generate
ninja -C build-pgo

# 2. Run lemon for a few minutes doing typical tasks, then exit cleanly
build-pgo/lemon

# 3. Re-link with the collected profile
meson configure build-pgo -Dpgo=use
ninja -C build-pgo
```

`build-pgo/lemon` is now optimised against your actual usage patterns.

## First run

```bash
mkdir -p ~/.config/lemon
cp /etc/lemon/lemon.conf ~/.config/lemon/lemon.conf
lemon
```

Or with an explicit path:

```bash
lemon -c /path/to/lemon.conf
```

Default keybinds (full reference in [`docs/bindings/keys.md`](docs/bindings/keys.md)):

| Keys | Action |
|------|--------|
| `Alt`+`Return` | Terminal (`foot`) |
| `Alt`+`Space` | Launcher (`rofi`) |
| `Alt`+`Q` | Kill focused client |
| `Super`+`M` | Quit lemon |
| `Super`+`F` | Toggle fullscreen |
| `Alt`+arrows | Move focus |
| `Ctrl`+`1..9` | Switch to tag |
| `Alt`+`1..9` | Move client to tag |

## Configuration

Single file at `~/.config/lemon/lemon.conf`. The example shipped in `assets/lemon.conf`
covers every option. Edits take effect without restarting the compositor.

Topic guides:

- [`docs/configuration/`](docs/configuration/) — basics, input, monitors, portals
- [`docs/window-management/`](docs/window-management/) — layouts, rules, scratchpads
- [`docs/visuals/`](docs/visuals/) — animations, effects, theming, status bar
- [`docs/bindings/`](docs/bindings/) — keys and mouse gestures

## IPC — `mmsg`

`mmsg` is a small CLI that speaks the `dwl-ipc-unstable-v2` protocol. It can read
state (tags, layout, focused client, monitor info), watch events as a stream, and
dispatch internal commands.

```bash
mmsg -g -t          # current tags
mmsg -s -t 2+       # add tag 2 to current view
mmsg -w -Oct        # watch outputs/clients/tags
mmsg -d killclient
```

Full reference: [`docs/ipc.md`](docs/ipc.md).

## Project layout

```
src/lemon.c              main compositor — single TU including all headers below
src/common/util.{c,h}    helpers: die, ecalloc, regex cache, frame clock, strings
src/config/              parser + default values for lemon.conf
src/client/              Client struct + ops
src/layout/              tiling algorithms (one header per layout)
src/animation/           keyframe interpolation per object kind
src/fetch/               read-only queries (clients, monitors)
src/dispatch/            keybinding action table
src/ext-protocol/        extra Wayland protocols (workspace, foreign-toplevel, dwl-ipc, tearing, text-input)
mmsg/mmsg.c              IPC CLI client
protocols/               XML protocol definitions for wayland-scanner
assets/                  desktop entry, portal config, example lemon.conf
docs/                    documentation
```

Note: most files under `src/*/` are `.h` headers that contain function definitions
and are included once from `src/lemon.c`. This is intentional — the project compiles
as a single translation unit for fast builds and better whole-program optimisation.

## Credits

- [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots) — Wayland protocol implementation
- [dwl](https://codeberg.org/dwl/dwl) — base compositor
- [mangowm](https://github.com/DreamMaoMao/mangowm) — direct upstream
- [scenefx](https://github.com/wlrfx/scenefx) — visual effects library
- [mwc](https://github.com/nikoloc/mwc) — animation reference
- [sway](https://github.com/swaywm/sway) — reference compositor

## License

See `LICENSE.dwm` for the original dwm/dwl portions and the source headers for
upstream attribution.
