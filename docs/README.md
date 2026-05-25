# Lemon documentation

Lemon is a low-latency Wayland tiling compositor in C, built on wlroots 0.19 and
scenefx 0.4 and forked from mangowm / dwl. The compositor is a single
translation unit, all spring-physics geometry animations are interruptible and
self-terminating, and an idle desktop sits at 0 % CPU.

## Contents

- **[configuration.md](configuration.md)** — every `lemon.conf` option, grouped
  by section: general, animations, spring physics, layout, appearance, input,
  monitors, rules, idle / power, focus QoS, cycler, clipboard, bindings.
- **[why-lemon-is-different.md](why-lemon-is-different.md)** — design rationale.
  Single TU, spring physics, true idle sleep, latency-first scheduling, focus
  QoS, render tiers, per-monitor client index, adaptive late-latch, image
  clipboard, alt-tab cycler with definitive thumbnail fallback.

## Quick start

```bash
meson setup build -Dprefix=/usr
ninja -C build
sudo ninja -C build install
```

Nested development session (inside an existing Wayland or X login):

```bash
WAYLAND_DISPLAY=wayland-1 build/lemon -c assets/lemon.conf
```

Config is read from `~/.config/lemon/lemon.conf` (fallback `/etc/lemon/`) and
hot-reloads on save. The annotated reference config is `assets/lemon.conf`.

## Recent additions

- Per-monitor client index — multi-output rendering cost is per-monitor.
- Sub-stepped spring integrator — animation accuracy preserved at 30 Hz / VRR
  low-refresh.
- Asymmetric render-time EMA — late-latch deadline reacts to spikes fast and
  decays slowly.
- Alt+Tab cycler with numbered badges, `Mod+digit` jump and a buffer-direct
  thumbnail fallback (every mapped client gets a tile, even when scene-tree
  snapshot returns no anchor).
- Clipboard history with image (PNG / JPEG / WEBP / GIF / TIFF / BMP) entries
  and a non-blocking event-loop paste drain.
- `cycler_modifier=alt|super` config knob for hardware without a working
  Super key.

See [`why-lemon-is-different.md`](why-lemon-is-different.md) sections 11–15 for
the detailed rationale.
