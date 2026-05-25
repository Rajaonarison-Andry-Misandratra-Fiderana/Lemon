# Lemon documentation

Lemon is a low-latency Wayland tiling compositor in C, built on wlroots 0.19
and scenefx 0.4 and forked from mangowm / dwl. Single translation unit,
spring-physics geometry, trackpad gestures as first-class input, 0 % CPU when
idle.

## Contents

- **[configuration.md](configuration.md)** — every `lemon.conf` option, grouped
  by section: general, animations, spring physics, layout, appearance, input,
  monitors, rules, idle / power, focus QoS, cycler, clipboard, bindings.
- **[why-lemon-is-different.md](why-lemon-is-different.md)** — design rationale:
  single TU, spring physics, trackpad gestures, idle sleep, latency-first
  scheduling, render tiers, per-monitor index, alt-tab cycler, image clipboard.

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

- Alt+Tab cycler: numbered badges, `Mod+digit` jump, buffer-direct thumbnail
  fallback.
- Image clipboard history (PNG / JPEG / WEBP / GIF / TIFF / BMP) with async
  event-loop paste drain.
- Per-monitor client index for `O(per_monitor)` multi-output rendering cost.
- Sub-stepped spring + asymmetric render-time EMA.
- `cycler_modifier=alt|super` for hardware without a working Super key.
