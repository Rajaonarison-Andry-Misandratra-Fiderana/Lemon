# Lemon documentation

Lemon is a low-latency, low-memory Wayland tiling compositor in C (wlroots 0.19
+ scenefx 0.4), forked from mangowm / dwl.

- **[configuration.md](configuration.md)** — every `lemon.conf` option, grouped
  by section (general, animations, spring physics, layout, appearance, input,
  monitors, rules, idle/power, QoS, bindings).
- **[why-lemon-is-different.md](why-lemon-is-different.md)** — what sets Lemon
  apart from other compositors (single TU, spring animations, true idle sleep,
  latency-first scheduling, focus QoS, render tiers, …).

## Quick start

```bash
meson setup build -Dprefix=/usr
ninja -C build
sudo ninja -C build install
```

Run nested for development:

```bash
WAYLAND_DISPLAY=wayland-1 build/lemon -c assets/lemon.conf
```

Config is read from `~/.config/lemon/lemon.conf` (fallback `/etc/lemon/`) and
hot-reloads on save. The annotated reference config is `assets/lemon.conf`.
