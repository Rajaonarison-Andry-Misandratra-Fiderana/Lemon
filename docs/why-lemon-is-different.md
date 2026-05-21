# Why Lemon is different

Lemon is a Wayland tiling compositor in C, forked from mangowm (a dwl/wlroots
derivative). It is built on **wlroots 0.19** and **scenefx 0.4**. The fork
trades breadth for two obsessions: **lowest possible latency** and **lowest
possible resource use**, without giving up a premium animated feel. The things
below are deliberate and mostly unusual among wlroots compositors.

## 1. Single translation unit

`src/lemon.c` is the *only* `.c` compiled into the binary (plus `util.c` and
generated protocol sources). Almost every other `src/**.h` contains **function
definitions** included exactly once. The whole compositor is one translation
unit → the compiler sees everything → aggressive whole-program inlining and
devirtualization, and a fast build. Most compositors are dozens of separate
objects; Lemon is one.

## 2. Spring-physics animations, not keyframes

Window geometry (move, resize, tiling, overview, workspace slide) is driven by a
**damped harmonic oscillator** (Hooke's law + friction), integrated with
semi-implicit Euler in `src/animation/spring.h`. It is:

- **Interruptible** — change focus or layout mid-animation and the trajectory
  bends from the *current velocity* instead of snapping or restarting.
- **Sub-pixel** — float position + per-axis velocity, snapped to integer pixels
  only when it settles (crisp text, no shimmer).
- **Self-terminating** — once within a position *and* velocity epsilon it stops
  scheduling frames, so an idle desktop is genuinely 0% CPU.

Open/close keep a bezier curve (they need a 0..1 progress for zoom/fade). A
separate, faster spring is used for workspace switches.

## 3. Idle compositor truly sleeps

The per-monitor render loop only commits the scene when there is real damage (or
a pending screen-capture). No damage → no GPU commit, no wakeup. Combined with
the self-terminating springs and a 4 Hz cap on idle-notify, a static screen does
almost no work. On battery, animation frame scheduling is throttled to ~60 fps.

## 4. Latency-first process & scheduling

- The compositor requests **SCHED_RR realtime** scheduling and `mlockall()`s its
  working set so it is never paged out or preempted by a busy client.
- Children launch via **`posix_spawn`** (clone/vfork path), not `fork()`, so
  spawning an app doesn't copy-on-write the compositor's whole page table.
- xdg-desktop-portal is warm-pinged and the cursor theme preloaded at startup so
  the first GTK/Qt client pays no cold-start.
- Optional explicit GPU sync (`linux-drm-syncobj-v1`) lets the GPU signal
  completion instead of the compositor blocking.

## 5. Focus-aware system QoS

With `focus_qos`, on every focus change Lemon renices and re-ioprios the focused
window's process group up and the one losing focus down — a runaway background
app can't make the foreground stutter, with no external daemon (ananicy, etc.).

## 6. Render tiers

Each client is classed FOCUS / VISIBLE / OCCLUDED / HIDDEN. Occluded clients
animate at 30 Hz, hidden ones not at all. The compositor spends frame budget only
where it is visible.

## 7. Native idle without a daemon

A built-in idle timer (`idle_timeout` + `idle_action`) blanks the screen
(DPMS), suspends, or hibernates — and `idlebind` gives arbitrary timed actions —
all inside the compositor, respecting idle inhibitors (video players), with no
swayidle process.

## 8. Surface geometry cache

An LRU `app_id → (w,h)` map persisted to disk lets the first configure for a
known app be sent at the right size, skipping a resize round-trip on launch.

## 9. Tuned, tiny build

Release builds use `-O3` plus opportunistic GCC IPA passes, section GC, and a
fast-math island for the animation curve hot path; optional two-pass PGO and
jemalloc. The goal is a small, low-latency binary — not a feature checklist.

## What Lemon deliberately is *not*

- **No blur, no drop shadow** (kept off for latency/power; opacity, corner
  radius, and animations stay).
- **No huge config DSL or scripting runtime** — plain `key=value`, hot-reloaded.
- **No test suite / CI in-tree** — validation is a clean compile + a manual
  smoke test in a nested session.
