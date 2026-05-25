<div align="center">
  <img src="assets/lemon-logo.svg" width="180" alt="Lemon"/>

  # Lemon

  **A low-latency Wayland tiling compositor in C.**

  <sub>wlroots 0.19 · scenefx 0.4 · ~100 MB resident · spring-physics animations</sub>

  ---
</div>

## Table of contents

- [What is Lemon](#what-is-lemon)
- [Highlights](#highlights)
- [Install](#install)
  - [Dependencies](#dependencies)
  - [Build from source](#build-from-source)
  - [PGO build (lowest latency)](#pgo-build-lowest-latency)
  - [Wayland session entry](#wayland-session-entry)
- [Run](#run)
- [Configure](#configure)
- [Why Lemon](#why-lemon)
- [Documentation](#documentation)
- [Status](#status)
- [Credits and license](#credits-and-license)

## What is Lemon

Lemon is a Wayland tiling compositor forked from
[mangowm](https://github.com/DreamMaoMao/mangowm) (a dwl / wlroots derivative).
It trades feature breadth for two obsessions:

1. **Lowest possible input-to-photon latency** &mdash; realtime scheduling,
   late-latch deadlines, `mlockall`, `posix_spawn`, render tiers.
2. **Lowest possible idle cost** &mdash; commits only on real damage,
   self-terminating spring animations, capped malloc arenas; an idle desktop
   sits at 0 % CPU.

It looks and feels premium &mdash; spring-driven move/resize/tile, kinetic motion
blur, smooth tag slides, native overview &mdash; but it is closer to dwl in
philosophy than to a kitchen-sink compositor. The whole compositor lives in a
single translation unit so the compiler can inline and devirtualise across
the entire codebase.

## Highlights

<table>
<tr>
<td valign="top" width="50%">

**Animation**
- Damped harmonic spring on every geometry change &mdash; interruptible,
  sub-pixel, self-terminating.
- Sub-stepped integrator stays accurate at 30&nbsp;Hz battery throttle / VRR
  low-refresh.
- Pure translations skip buffer downscale for pixel-crisp move.
- Kinetic motion blur fades fast-moving windows, snaps crisp on stop.
- Bezier curves for open/close zoom/fade.

</td>
<td valign="top" width="50%">

**Latency**
- `SCHED_RR` realtime priority + `mlockall(MCL_CURRENT|MCL_ONFAULT)`.
- Late-latch deadline timer: render fires just before vblank, fed by an
  asymmetric EMA of render duration.
- `posix_spawn` for every child &mdash; no COW of the compositor's page table.
- Cursor theme preloaded at scale 1 + 2; xdg-desktop-portal warm-pinged at
  startup.
- Optional explicit GPU sync (`linux-drm-syncobj-v1`).

</td>
</tr>
<tr>
<td valign="top">

**Window management**
- Tiling layouts: tile, dwindle, scroller, vertical, horizontal, grid.
- Overview grid across all tags with dim backdrop and click-to-pick.
- Alt+Tab cycler with live scene snapshots, numbered badges and
  `Mod+digit` direct jump.
- Per-tag layout memory + per-workspace gaps.
- 3- and 4-finger touchpad swipes for layout pick and window swap.

</td>
<td valign="top">

**System integration**
- Built-in idle action (DPMS / suspend / hibernate) with idle inhibitor
  awareness &mdash; no swayidle.
- Focus-aware QoS: renices and re-ioprios the focused process group up,
  background down.
- Native AC plug/unplug toast.
- Clipboard history with text **and** image entries (PNG thumbnail
  preview, JPEG/WEBP/etc. accepted), async paste drain.
- ext-workspace-v1, wlr-foreign-toplevel, tearing-control-v1, text-input
  v2/v3.

</td>
</tr>
<tr>
<td valign="top">

**Rendering**
- scenefx GLES2 renderer with per-window opacity and corner radius.
- Per-monitor client index &mdash; multi-output cost is per-monitor, not
  global.
- Render tiers (FOCUS / VISIBLE / OCCLUDED / HIDDEN) throttle invisible
  work.
- Output commit gated by scene damage and legacy screencopy state.
- Optional backdrop blur and drop shadow (off by default; opt-in via
  `blur=1` / `shadows=1`).

</td>
<td valign="top">

**Resource discipline**
- ~100 MB resident target. glibc malloc tuned (`M_ARENA_MAX=2`,
  low trim/mmap thresholds), periodic `malloc_trim`.
- `single-pixel-buffer-v1` so solid-colour surfaces cost 1 pixel.
- Surface geometry cache (`$XDG_CACHE_HOME/lemon/surfaces.db`) skips a
  resize round-trip on first configure for known apps.
- PCRE2 LRU regex cache (32 entries) with JIT.
- Optional two-pass PGO build and jemalloc.

</td>
</tr>
</table>

## Install

### Dependencies

Runtime / build dependencies (Arch package names &mdash; map to your distro):

| Required | Optional |
|----------|----------|
| `wlroots>=0.19` | `xorg-xwayland` (XWayland) |
| `scenefx>=0.4.1` | `xcb-util-wm` (XWayland) |
| `wayland>=1.23.1` | `jemalloc` (lower-fragmentation allocator) |
| `wayland-protocols` | |
| `libinput>=1.27.1` | |
| `xkbcommon` | |
| `libpcre2-8` | |
| `cairo` | |
| `pangocairo` | |
| `meson` + `ninja` (build) | |

### Build from source

```bash
git clone https://github.com/<your-fork>/Lemon
cd Lemon

meson setup build -Dprefix=/usr
ninja -C build
sudo ninja -C build install
```

Release builds use `-O3 -fno-plt -fno-semantic-interposition -fvisibility=hidden
-ffunction-sections -fdata-sections -fmerge-all-constants`, opportunistic GCC
IPA passes (`-fipa-pta`, `-fdevirtualize-speculatively`,
`-fpredictive-commoning`, `-ftree-vectorize`), and a fast-math island for the
animation curve hot path. The linker drops dead sections and uses
`-z,now,relro,noseparate-code`.

Useful Meson options (`meson configure build` to list them):

| Option | What it does |
|--------|--------------|
| `native` | `-march=native -mtune=native` |
| `lto` | Link-time optimisation |
| `jemalloc` | Replace glibc malloc with jemalloc |
| `xwayland` | Build XWayland support (requires xcb + xcb-icccm) |
| `pgo` | `generate` to profile, `use` to consume |
| `asan` | AddressSanitizer build (dev only) |

### PGO build (lowest latency)

Two-pass profile-guided build produces the lowest-latency binary:

```bash
# Pass 1: instrumented build
meson setup build-pgo --buildtype=release -Dpgo=generate
ninja -C build-pgo

# Use it normally for 5-10 minutes (real workload: tile, alt-tab, switch tags),
# then exit cleanly.
build-pgo/lemon

# Pass 2: rebuild with collected profile
meson configure build-pgo -Dpgo=use
ninja -C build-pgo
```

For unprivileged users to get realtime priority and locked memory, add a
limits file (`/etc/security/limits.d/99-lemon.conf`):

```
@video - rtprio   10
@video - memlock  524288
```

### Wayland session entry

`assets/lemon.desktop` is installed to `share/wayland-sessions` so display
managers (gdm, sddm, greetd, ly) pick it up. Log out, choose **Lemon** from the
session selector at the login screen.

## Run

Inside an existing Wayland or X session, run nested for development:

```bash
WAYLAND_DISPLAY=wayland-1 build/lemon -c assets/lemon.conf
```

Without `WAYLAND_DISPLAY` set, Lemon starts on the DRM backend (a real
session).

## Configure

Config lives at `~/.config/lemon/lemon.conf` (fallback `/etc/lemon/lemon.conf`)
and **hot-reloads on save**. Format is `key=value`, `#` for comments. Sections
are commented in the example file.

- Annotated example &mdash; [`assets/lemon.conf`](assets/lemon.conf).
- Full reference of every option &mdash; [`docs/configuration.md`](docs/configuration.md).

A minimal keybinding looks like:

```ini
bind = SUPER+Return,spawn,foot
bind = SUPER+Tab,window_cycler_next
bind = SUPER+SHIFT+Tab,window_cycler_prev
bind = SUPER+1,view,1
```

## Why Lemon

A condensed list. The full design rationale is
[`docs/why-lemon-is-different.md`](docs/why-lemon-is-different.md).

- **Single translation unit.** One `.c`, header-only modules included once.
  Whole-program inlining, fast builds, no link-time abstraction tax.
- **Spring physics for every geometry change.** Interruptible, sub-pixel,
  self-terminating. No keyframe interpolation cheap-feel.
- **The idle compositor truly idles.** Damage gating, self-terminating
  springs, 4 Hz idle-notify cap &mdash; static screen, 0 % CPU.
- **No background daemons.** Idle action, focus QoS, AC notify, clipboard
  history are all inside the compositor.
- **Render tiers.** Occluded clients animate at 30 Hz; hidden ones not at
  all. Frame budget goes where it is visible.
- **Per-monitor client index.** Multi-output rendering cost is per-monitor,
  not `O(N_clients * N_outputs)`.
- **Memory discipline.** Tuned glibc malloc, periodic trim, single-pixel
  buffer for solids. The session does not bloat over the day.
- **Premium feel without effects spam.** Opacity, corner radius, motion
  blur and animations always on; blur + shadow opt-in.

## Documentation

| Document | Purpose |
|----------|---------|
| [`docs/configuration.md`](docs/configuration.md) | Every `lemon.conf` key, grouped by section. |
| [`docs/why-lemon-is-different.md`](docs/why-lemon-is-different.md) | Design rationale and what sets Lemon apart. |
| [`CLAUDE.md`](CLAUDE.md) | Codebase map and conventions (also a primer for any AI assistant). |
| [`assets/lemon.conf`](assets/lemon.conf) | Annotated default configuration. |

## Status

Lemon is a personal compositor first. It is used daily as the author's
primary session, kept fast and crash-free, but it has **no automated tests
or CI**. Validation = clean build plus a manual smoke test in a nested
session. Issues and reproducible bug reports are welcome.

NixOS support and the Guix channel file have been removed; do not expect
them.

## Credits and license

- Forked from [mangowm](https://github.com/DreamMaoMao/mangowm), itself
  derived from [dwl](https://codeberg.org/dwl/dwl).
- Built on [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots) and
  [scenefx](https://github.com/wlrfx/scenefx).

License inherits from dwl &mdash; see `LICENSE` files.
