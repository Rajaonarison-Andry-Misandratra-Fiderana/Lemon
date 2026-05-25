<div align="center">
  <img src="assets/lemon-logo.svg" width="160" alt="Lemon"/>

  # Lemon

  **Low-latency Wayland tiling compositor in C.**

  <sub>wlroots 0.19 · scenefx 0.4 · spring physics · trackpad gestures · ~100 MB RSS</sub>

  ---
</div>

## What is Lemon

Wayland tiling compositor forked from
[mangowm](https://github.com/DreamMaoMao/mangowm) (dwl / wlroots derivative).
Two obsessions: lowest possible **input-to-photon latency** and lowest possible
**idle cost**. Premium feel without effects spam. Whole compositor lives in a
single translation unit so the compiler can inline across the entire codebase.

## Highlights

- **Spring physics** — every move / resize / tile / overview / workspace
  slide. Interruptible, sub-pixel, self-terminating. Sub-stepped at low
  refresh.
- **Trackpad gestures** — first-class. 3-finger directional swipes pick a
  layout and promote master (down = minimize). 4-finger horizontal swipes
  swap focus with the neighbour in any layout. 4-finger vertical drives the
  swayosd volume bar.
- **Alt+Tab cycler** — live scene-snapshot thumbnails, numbered badges,
  `Mod+digit` direct jump, Escape dismiss.
- **Clipboard history** — text **and** image (PNG thumbnail preview, JPEG /
  WEBP / GIF / TIFF / BMP accepted). Non-blocking paste drain.
- **Overview** — grid of every window with dim backdrop, click to pick.
- **Latency-first** — `SCHED_RR`, `mlockall`, `posix_spawn`, late-latch
  deadline, render tiers, asymmetric render-time EMA.
- **True idle** — damage-gated commits, 4 Hz idle-notify cap, idle desktop
  at 0 % CPU.
- **No background daemons** — idle action (DPMS / suspend / hibernate),
  focus QoS, AC notify, clipboard all inside the compositor.
- **Per-monitor client index** — multi-output cost is per-monitor, not
  `O(N × M)`.
- **Optional opt-in effects** — backdrop blur, drop shadow. Off by default.

## Install

Dependencies (Arch names; map to your distro): `wlroots>=0.19`,
`scenefx>=0.4.1`, `wayland>=1.23.1`, `wayland-protocols`, `libinput>=1.27.1`,
`xkbcommon`, `libpcre2-8`, `cairo`, `pangocairo`, plus `meson` + `ninja`.
Optional: `xorg-xwayland` + `xcb-util-wm` for XWayland, `jemalloc` for the
allocator.

```bash
meson setup build -Dprefix=/usr
ninja -C build
sudo ninja -C build install
```

Meson options worth knowing: `native`, `lto`, `jemalloc`, `xwayland`, `pgo`,
`asan`. PGO is a two-pass build (`-Dpgo=generate` → run 5–10 min → `-Dpgo=use`
→ rebuild) for the lowest-latency binary.

For unprivileged users to get realtime priority and locked memory, drop a
`limits.d` entry granting your group `rtprio 10` and `memlock 524288`.

`assets/lemon.desktop` is installed to `share/wayland-sessions` so display
managers (gdm / sddm / greetd / ly) pick up **Lemon** as a session choice.

## Run

Inside an existing Wayland or X session, nested:

```bash
WAYLAND_DISPLAY=wayland-1 build/lemon -c assets/lemon.conf
```

Unset `WAYLAND_DISPLAY` for a real DRM session.

## Configure

`~/.config/lemon/lemon.conf` (fallback `/etc/lemon/lemon.conf`),
`key=value`, `#` for comments, **hot-reloads on save**.

- Annotated example — [`assets/lemon.conf`](assets/lemon.conf)
- Full reference — [`docs/configuration.md`](docs/configuration.md)

## Why Lemon

Condensed; the full rationale lives in
[`docs/why-lemon-is-different.md`](docs/why-lemon-is-different.md).

- **Single translation unit.** Whole-program inlining, fast builds.
- **Spring physics on every geometry change.** No keyframe cheap feel.
- **Trackpad gestures are a primary input.** Three- and four-finger swipes
  control layout, focus and volume directly &mdash; not a thin shell over
  `wlr_pointer_gestures_v1`.
- **The idle compositor truly idles.**
- **Render tiers + per-monitor index.** Frame budget where it is visible.
- **Memory discipline.** Tuned malloc, periodic trim, single-pixel buffer.
  Long sessions do not bloat.

## Status

Personal compositor; daily-driven, kept fast and crash-free. **No CI, no
test suite.** Validation = clean build + manual smoke test in a nested
session.

## Credits

Forked from [mangowm](https://github.com/DreamMaoMao/mangowm) (from
[dwl](https://codeberg.org/dwl/dwl)). Built on
[wlroots](https://gitlab.freedesktop.org/wlroots/wlroots) and
[scenefx](https://github.com/wlrfx/scenefx). License inherits from dwl —
see `LICENSE` files.
