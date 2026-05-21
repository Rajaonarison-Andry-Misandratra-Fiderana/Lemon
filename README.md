<div align="center">
  <img src="assets/lemon-transparency-256.png" width="128" alt="Lemon"/>

  # Lemon
</div>

## What is Lemon

Lemon is a low-latency, low-memory Wayland tiling compositor written in C
(wlroots 0.19 + scenefx 0.4), forked from mangowm / dwl. It targets the lowest
possible latency and resource use while keeping a premium, spring-animated feel.

## Features

- **Spring-physics animations** — move, resize, tile, overview and workspace
  slides use a damped-harmonic-oscillator spring; fully interruptible, pixel
  crisp on settle.
- **scenefx effects** — per-window opacity fades, rounded corners (no blur, no
  shadow).
- **Kinetic motion blur** — fast-moving windows fade slightly, sharp on stop.
- **True idle sleep** — commits only on damage; self-terminating animations →
  0% CPU when idle.
- **Native idle & power** — built-in idle action (DPMS/suspend/hibernate),
  timed `idlebind`s, and smooth pre-idle backlight dimming.
- **Focus-aware QoS** — renices/ioprios the focused app up, background down.
- **Touchpad gestures** — multi-finger swipes, live 4-finger volume OSD.
- **Latency-first** — realtime scheduling, `mlockall`, `posix_spawn`, explicit
  GPU sync, render tiers, single-pixel-buffer, capped malloc arenas (~100 MB).

## Build

```bash
meson setup build -Dprefix=/usr
ninja -C build
sudo ninja -C build install
```

Run nested for development:

```bash
WAYLAND_DISPLAY=wayland-1 build/lemon -c assets/lemon.conf
```

## Configure

Config lives at `~/.config/lemon/lemon.conf` (fallback `/etc/lemon/lemon.conf`)
and **hot-reloads on save**. Format is `key=value`, `#` for comments.

- Annotated example: [`assets/lemon.conf`](assets/lemon.conf)
- Full reference: [`docs/configuration.md`](docs/configuration.md)
- Design rationale: [`docs/why-lemon-is-different.md`](docs/why-lemon-is-different.md)
