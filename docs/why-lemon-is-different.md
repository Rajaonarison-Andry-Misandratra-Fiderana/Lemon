# Why Lemon is different

Wayland tiling compositor in C, forked from mangowm (dwl/wlroots). Built on
**wlroots 0.19** + **scenefx 0.4**. Trades breadth for lowest latency and
lowest resource use, without giving up a premium animated feel.

## 1. Single translation unit

`src/lemon.c` is the only `.c` compiled; almost every other `src/**.h` carries
function definitions and is included exactly once. The whole compositor is one
TU → whole-program inlining + fast builds.

## 2. Spring-physics geometry

Move / resize / tile / overview / workspace slide use a damped harmonic
oscillator (semi-implicit Euler, zero alloc). Interruptible, sub-pixel,
self-terminating. Sub-stepped when `dt > 8 ms` so battery throttle and VRR
low-refresh stay accurate. Open / close keep a bezier curve for the 0..1 fade.

## 3. Trackpad gestures as first-class input

Gestures are not a `wlr_pointer_gestures_v1` passthrough — Lemon owns them:

- **3-finger directional swipe** — picks the matching tiling layout and
  promotes the focused window to master. Down minimises.
- **4-finger horizontal swipe** — swaps the focused window with its
  neighbour in any layout.
- **4-finger vertical swipe** — drives the swayosd volume bar continuously
  (`touchpad_4f_osd` + `touchpad_4f_step`).

All resolved inside `swipe_update` so latency is one event-loop round-trip,
not a daemon hop.

## 4. Idle compositor truly sleeps

Damage-gated commits. No damage → no GPU commit, no wakeup. Combined with
self-terminating springs and a 4 Hz idle-notify cap, a static screen does
~nothing. Battery mode caps animation scheduling at ~60 fps.

## 5. Latency-first scheduling

`SCHED_RR` realtime, `mlockall(MCL_CURRENT|MCL_ONFAULT)`, `posix_spawn` for
every child (no page-table COW). Cursor theme + xdg-desktop-portal pre-warmed
at startup. Optional explicit GPU sync (`linux-drm-syncobj-v1`).

## 6. Adaptive late-latch deadline

A deadline timer defers each frame until just before vblank. The render-time
EMA driving it reacts fast to spikes (1/2 weight up) and decays slowly
(1/16 down), so a heavy frame never overshoots the next vblank and a single
light frame can't shrink the safety margin.

## 7. Per-monitor client index

`mon->clients` (linked via `Client.mon_link`, maintained by `setmon`). Render
cost per frame is `O(per_monitor)` instead of `O(N_clients × N_outputs)`.

## 8. Render tiers

FOCUS / VISIBLE / OCCLUDED / HIDDEN. Occluded clients animate at 30 Hz, hidden
ones not at all.

## 9. Focus-aware QoS

`focus_qos` renices + re-ioprios the focused process group up and the one
losing focus down on every focus change. No ananicy daemon.

## 10. Native idle action

`idle_timeout` + `idle_action` blanks (DPMS), suspends or hibernates, all
inside the compositor and respecting idle inhibitors. `idlebind` adds
arbitrary timed actions. No swayidle.

## 11. Alt+Tab cycler with definitive thumbnail fallback

Live scene-snapshot per client. When `wlr_scene_tree_snapshot` has no usable
buffer anchor (Electron with a tiny main surface, just-mapped client, fadeout
in progress) the cycler wraps `client_surface->buffer` directly via
`wlr_scene_buffer_create` so every mapped client gets a tile. Numbered badges
(1..9) overlay each thumbnail; `Mod+digit` jumps directly. `cycler_modifier`
chooses Alt or Super for hardware whose Super key is dead.

## 12. Clipboard history for text and images

Capture accepts `image/*` and `text/*` MIMEs. Image entries (PNG / JPEG /
WEBP / GIF / TIFF / BMP) live in RAM alongside text; the picker decodes the
currently selected PNG into an inline thumbnail. Paste source advertises only
the captured MIME so binary bytes never leak into a text field. Paste write
is non-blocking with an event-loop `ClipPasteFlight` drain, so an 8 MiB image
to a slow consumer no longer deadlocks the main loop.

## 13. Memory discipline

`~100 MB` resident target. glibc malloc tuned (`M_ARENA_MAX=2`, low trim and
mmap thresholds), periodic `malloc_trim`. `single-pixel-buffer-v1` advertised,
so solid-colour surfaces cost one pixel. Surface geometry cache persists
`app_id → (w, h)` so the first configure for a known app is the right size.

## 14. Tuned, tiny build

`-O3` plus opportunistic GCC IPA passes, section GC, fast-math island for the
spring/curve hot path. Optional two-pass PGO and jemalloc.

## What Lemon deliberately is not

- Blur and drop shadow are off by default (`blur=1` / `shadows=1` to opt in).
- No config DSL or scripting runtime — plain `key=value`, hot-reloaded.
- No CI, no in-tree tests — validation is a clean build + manual smoke test.
