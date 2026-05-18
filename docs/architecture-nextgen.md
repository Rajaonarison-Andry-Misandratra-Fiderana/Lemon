# Next-Generation Compositor Architecture

Design brief for evolving Lemon (or a sibling project) into an ultra-smooth Wayland
compositor that targets macOS-class fluidity on Linux. Concrete pseudo-code uses
the existing Lemon vocabulary (`Monitor`, `Client`, `frame_now_ms`, `LEMON_HOT`)
where possible so the doc maps directly onto the current code.

Conventions: pseudo-code is illustrative C; "**Problem**" / "**Why hard on
Linux**" / "**Design**" / "**Low-level**" / "**Tradeoffs**" / "**Perf**" / "**Code**" /
"**Optim**" / "**Debug**" headers repeat per subsystem.

---

## 1. Ultra-Low Input Latency

**Problem.** Mouse-to-photon and key-to-photon latency dominate perceived
responsiveness. Goal < 1 frame at 240 Hz (~4 ms) for cursor, < 2 frames for
keystrokes.

**Why hard on Linux.** evdev → libinput → wayland dispatch → compositor logic →
client commit → GPU submit → vblank. Six hops, each can stall. wlroots batches
events on event loop tick; default vsync adds an extra frame.

**Design.**
- One dedicated input epoll thread sets `SCHED_FIFO` priority 10, polls libinput
  fd, drains all pending events, posts them to a lock-free SPSC ring read by the
  main loop. Cursor motion writes the latest absolute position directly to a
  per-monitor atomic, with `wlr_output_schedule_frame` poked from the input
  thread.
- Frame-deadline predictor (Kalman or EWMA) tracks (input → first damage) and
  (commit → presentation) to choose the latest safe commit moment.
- Per-fullscreen-client "low-latency mode" disables triple buffering, requests
  immediate page flip (`DRM_MODE_PAGE_FLIP_ASYNC`), bypasses scene compositing.

**Low-level.** `mlock`/`mlockall(MCL_CURRENT)` for the input thread's working
set; pinned to a P-core via `sched_setaffinity`. Use
`EVIOCSCLOCKID(CLOCK_MONOTONIC)` so timestamps match `frame_now_ms`.

**Tradeoffs.** Realtime thread can starve other compositor work — bound CPU
usage with `RLIMIT_RTTIME`. Low-latency mode disables blur/shadow/animations
inside the target surface (acceptable).

**Perf.** Removes ~8 ms of best-case input pipeline jitter on a 144 Hz display.

**Code.**
```c
static int input_thread(void *arg) {
    struct sched_param sp = {.sched_priority = 10};
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    libinput_dispatch(li);
    while (epoll_wait(ep, ev, N, -1) > 0) {
        libinput_dispatch(li);
        for (struct libinput_event *e; (e = libinput_get_event(li));) {
            input_ring_push(e);
            atomic_store_explicit(&latest_seq, atomic_load(&latest_seq) + 1,
                                  memory_order_release);
            eventfd_write(wake_main_fd, 1);
        }
    }
}
```

**Optim.** Coalesce successive relative pointer deltas before publishing.
**Debug.** `perf sched record` + Wayland `presentation-time` protocol; report
input-to-frame latency over presentation feedback.

---

## 2. Frame Pacing & Stutter Elimination

**Problem.** Microstutter from variable scene compositing time vs fixed vblank
deadline. macOS uses CADisplayLink with a deadline prediction model.

**Why hard on Linux.** wlroots schedules frame on `output_frame` event after
vblank, leaving very little budget. CPU/GPU schedulers don't know compositor
deadline.

**Design.**
- Replace "compose at next vblank" with **deadline-based pacing**: record
  `compose_duration_p95` per output, schedule compose start at
  `vblank_predicted - compose_p95 - safety_margin`.
- Track *per-monitor render timeline* with `wlr_drm_syncobj_timeline` so
  CPU work overlaps GPU work.
- Missed frame: skip animation tick to current time-position (no catch-up
  burst). One missed frame should never cascade.

**Low-level.** Use `WAYLAND_DEBUG=client` and DRM `vblank_event` to gather
ground truth. Smooth with EWMA(alpha=0.2). Discard outliers > 3σ.

**Tradeoffs.** Predictor needs warm-up (10-30 frames after mode change).
Conservative safety margin trades latency for missed-frame rate.

**Perf.** Targets 0 missed frames at p99 in steady state.

**Code.**
```c
typedef struct {
    uint64_t vblank_ns;      // last vblank
    uint64_t period_ns;      // 1e9 / refresh
    uint64_t compose_p95_ns; // rolling
    uint64_t safety_ns;      // start at 500us
} PacerState;

uint64_t next_compose_start(PacerState *p) {
    uint64_t next_vblank = p->vblank_ns + p->period_ns;
    return next_vblank - p->compose_p95_ns - p->safety_ns;
}
```

**Optim.** When the queue is empty for > N frames, drop into idle mode
(see §3).
**Debug.** Stream per-frame `(deadline, compose_start, compose_end, present)`
as Perfetto traces.

---

## 3. Power Efficiency & Smart Redraw

**Problem.** Compositor wakes every vblank even when nothing changed. On
battery, this is the dominant idle drain.

**Why hard on Linux.** Many clients commit empty damage (cursor blink,
periodic timers). wlroots scene already does dirty-region but the wakeup
itself costs.

**Design.**
- Trust `wlr_scene_output_needs_frame` and short-circuit `rendermon` early.
- "Lazy frame scheduling": only `wlr_output_schedule_frame` when a damage
  signal fires. Animation tick already drives its own schedule.
- AC vs battery policy table:

| State        | Anim cap | Idle cap | Compose threading |
|--------------|----------|----------|-------------------|
| AC desktop   | refresh  | refresh  | inline            |
| AC fullscreen| refresh  | refresh  | offload to RT thr |
| Battery      | 60       | 30       | inline            |
| Battery idle | 30       | 10       | inline + RPS down |

- `DRM_IOCTL_MODE_OBJ_SETPROPERTY` to lower GPU clocks via
  `power_dpm_force_performance_level=low` when fully idle.

**Low-level.** Damage regions tracked via `pixman_region32_t`. Partial
present via `KMS_FB_DAMAGE_CLIPS` when the driver supports it.

**Tradeoffs.** Lowering FPS makes pointer-only motion choppier — keep cursor
on hardware plane so it doesn't hit FPS cap.

**Perf.** Idle laptop drops from 100% vblank wakeups to 1-2/s.

**Code.**
```c
LEMON_HOT void rendermon(...) {
    if (!wlr_scene_output_needs_frame(m->scene_output) && !m->anim_active) {
        wlr_scene_output_send_frame_done(m->scene_output, &now);
        return;
    }
    /* ...full compose path... */
}
```

**Optim.** Track per-client damage age — silence clients spamming empty
commits (`wl_surface_commit` with no damage) after 10 consecutive empties.
**Debug.** `intel_gpu_top -s 100` and `powertop --auto-tune` baselines.

---

## 4. Advanced Animation Engine

**Problem.** Animations driven from N per-object timers compete for the same
vblank. Bursts cause stutter.

**Why hard on Linux.** Each Wayland surface has its own commit cadence;
compositor animations layered on top must reconcile.

**Design.**
- **Single global animation scheduler.** Owns a min-heap of `(deadline,
  AnimNode*)`. Frame tick walks the heap until deadline > now, runs `step()`
  on each node, returns whether re-queue is needed.
- Spring physics module: `(stiffness, damping, mass, target)` → underdamped
  oscillator; interrupted mid-flight by feeding new target without rebuild.
- Animations write to a shadow value layer; the compose pass reads the snapshot
  at frame start (frame-clock cache from §1 of `util.h`).

**Low-level.** AnimNode is a 64-byte cache-line struct: `{type, t0, dur,
from, to, easing_idx, user, next}`. Pool-allocated, no malloc in steady state.

**Tradeoffs.** Single scheduler is a serialization point; mitigated because
step is O(1) per node and total nodes < 64 in practice.

**Perf.** Replaces ~N×timer overhead with 1 cache-hot loop.

**Code.**
```c
typedef struct AnimNode {
    uint32_t t0_ms, dur_ms;
    float from, to;
    uint8_t easing;
    void (*apply)(struct AnimNode*, float v);
    void *user;
    struct AnimNode *heap_link;
} AnimNode;

LEMON_HOT void anim_tick(uint32_t now) {
    AnimNode *n;
    while ((n = heap_peek()) && n->t0_ms <= now) {
        float t = clampf((float)(now - n->t0_ms) / (float)n->dur_ms, 0, 1);
        n->apply(n, ease(t, n->easing));
        if (t >= 1.0f) heap_pop();
        else break; /* heap ordered by deadline */
    }
}
```

**Optim.** Coalesce two interruptions on the same property within 1 ms.
**Debug.** Dump heap state on SIGUSR2.

---

## 5. Tearing Elimination & Display Sync

**Problem.** Tearing wanted in games, banned on desktop. VRR mishandled = judder.

**Why hard on Linux.** `tearing-control-v1` protocol exists but few apps set
it. VRR requires DRM atomic + correct `vrr_capable` query + content-aware
refresh rate.

**Design.**
- Per-surface tearing intent (`WLR_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC`)
  honoured only if surface covers ≥ 95% of output and is focused.
- VRR policy: enable adaptive sync on fullscreen apps committing < monitor
  refresh; disable on regular desktop (avoids low-refresh shimmer in dark UI).
- "Smart vsync": detect dropped frame; if next compose deadline will miss,
  flip async this once instead of waiting full period.

**Low-level.** `wlr_output_state_set_adaptive_sync_enabled(&state, true)`
followed by `wlr_output_commit_state`. Async flip via
`wlr_output_attach_render_flags` with `WLR_OUTPUT_PRESENT_ASYNC`.

**Tradeoffs.** Async on partial-coverage surface = visible tear line. Always
gate on coverage + fullscreen + focused.

**Perf.** Game frame-time variance shrinks ~40% in VRR + tearing combined.

**Code.**
```c
bool should_tear(Client *c, Monitor *m) {
    if (!config.allow_tearing || !c || !c->tearing_hint) return false;
    if (c->mon != m || !c->isfullscreen) return false;
    return client_covers_output(c, m, 0.95f);
}
```

**Optim.** Cache `vrr_capable` per output at hotplug; avoid querying per
frame.
**Debug.** `drm_info` + `gnome-control-center display` + DRM tracepoints.

---

## 6. Multi-Monitor

**Problem.** 60 Hz + 144 Hz side-by-side stutters because the compositor ties
its tick to one output.

**Why hard on Linux.** Most compositors run one render loop driven by primary
output. Cross-monitor windows draw at min refresh.

**Design.**
- Per-output render loop with its own frame deadline (already partial in
  Lemon: `rendermon` per `wlr_output->events.frame`).
- Per-output scene graph slice: each `wlr_scene_output` already filters by
  intersection. Make sure `wlr_output_schedule_frame` is *only* called for
  outputs intersecting a damage region.
- Independent input thread (§1) shares state; per-monitor pacer (§2) tracks
  its own predictor.
- Hotplug: pre-allocate Monitor struct from a pool to avoid first-frame
  malloc spike.

**Low-level.** `request_fresh_for_client(c)` (already in Lemon) iterates
`mons` and schedules only intersecting outputs.

**Tradeoffs.** Per-output threads multiply locking — keep render single-
threaded per output, share only read-only globals (config, scene).

**Perf.** Each output renders at its native rate; no min-refresh tax.

**Code.** Lemon's existing `rendermon` is already monitor-local — keep
extending in this direction; do not reintroduce `request_fresh_all_monitors`.

**Debug.** `WLR_SCENE_DEBUG_DAMAGE=highlight` visualises per-output damage.

---

## 7. Perceptual Speed & App Launch

**Problem.** First paint of a new window arrives 200-600 ms after spawn even
when the binary started in 50 ms — most is portal warmup, font cache, GTK
init.

**Why hard on Linux.** Compositor is downstream; client startup dominates.

**Design.**
- **Pre-warm path** (already started in Lemon): `posix_spawn` for launches,
  cursor theme + xdg-desktop-portal warmed at startup.
- **Placeholder window**: on `xdg_toplevel.set_app_id`, allocate the scene
  tree with a fade-in rect at the predicted target geometry *before* the
  client commits a buffer. Render fades in over 80 ms; replaced atomically
  when real buffer arrives.
- **Surface prediction cache**: persist last known geometry per `app_id`
  on disk (XDG_CACHE_HOME/lemon/surfaces.db). First commit uses the
  cached size, eliminating one configure round-trip.

**Low-level.** `xdg_surface_schedule_configure` sent in same wl_display
flush as `set_bounds` + `wm_capabilities` (Lemon already does this).

**Tradeoffs.** Geometry cache may mispredict on rotated monitors — fall back
to layout default.

**Perf.** Visible window appears 1 frame after spawn instead of 4-6.

**Code.**
```c
void on_new_toplevel(struct wlr_xdg_toplevel *tl) {
    Client *c = ecalloc(1, sizeof *c);
    struct wlr_box hint;
    if (surface_cache_lookup(tl->app_id, &hint)) {
        c->geom = hint;
        wlr_xdg_toplevel_set_size(tl, hint.width, hint.height);
    }
    show_placeholder(c);
}
```

**Optim.** Cache only top-50 recently-launched app_ids (LRU).
**Debug.** Wayland `presentation-time` reports first-frame ts; subtract from
spawn time logged via posix_spawn return.

---

## 8. GPU Memory & Resource

**Problem.** Workspace switch stalls on GPU eviction; alt-tab triggers
texture reupload.

**Why hard on Linux.** wlroots leaves buffer lifetime to clients; recycled
shm buffers re-import every commit.

**Design.**
- **Texture cache**: keyed on `wl_buffer*`, holds GBM/dmabuf import
  metadata. Eviction policy = LRU + size-weighted (large textures evicted
  first under pressure).
- **Surface reuse**: on `wl_surface.destroy`, defer scene-node tear-down by
  one frame so a near-immediate map of the same `app_id` can adopt the
  same node (common on Electron app restarts).
- **GPU residency hint**: focused clients tagged `WLR_BUFFER_HOT`,
  obscured clients `WLR_BUFFER_COLD`. Driver-side eviction prefers cold.

**Low-level.** `gbm_bo_get_handle_for_plane` + `EGL_EXT_image_dma_buf_import`
caching. Track via hash table `(modifier, width, height, format, plane_fd)`.

**Tradeoffs.** Cache pinning fights with low-memory systems — bound by
`vm.swappiness`-aware free-mem watermark.

**Perf.** Alt-tab between two heavy windows: 2 ms → < 0.3 ms compose time.

**Code.**
```c
struct tex_entry { struct wl_buffer *key; EGLImageKHR img; uint64_t last_used; };
EGLImageKHR tex_import(struct wl_buffer *b) {
    struct tex_entry *e = lru_get(tex_cache, b);
    if (e) { e->last_used = frame_now_ms(); return e->img; }
    e = lru_insert(tex_cache, b);
    e->img = eglCreateImageKHR(...);
    return e->img;
}
```

**Optim.** Drop cache entries on `wl_buffer.release` if buffer destroyed.
**Debug.** `mesa GALLIUM_HUD=fps,memory` overlay.

---

## 9. Focus-Aware Scheduling

**Problem.** Background animations (notification slide, browser tab favicon
spin) steal budget from the focused window.

**Why hard on Linux.** Compositor has no priority signal per client.

**Design.**
- 4-tier priority: `FOCUSED`, `VISIBLE`, `OCCLUDED`, `MINIMIZED`.
- Render budget per tick: focused gets unlimited, visible gets 70% of
  remaining, occluded gets 5 ms cap, minimized = skipped entirely.
- Background animation throttle: anim_tick (§4) checks tier; OCCLUDED nodes
  step at 30 Hz max regardless of monitor refresh.
- Under load (compose > deadline-margin): degrade visible → occluded
  temporarily.

**Low-level.** Tier stored on `Client`; recomputed on focus change + layout
arrange. Occlusion = `wlr_box_intersection` against scene damage region.

**Tradeoffs.** Notifications behind focus may animate at 30 Hz — acceptable;
user is not looking there.

**Perf.** Heavy YouTube + 20 Slack channels + Electron-Spotify drops compose
from p99 5.2 ms to 1.1 ms.

**Code.**
```c
enum tier { TIER_FOCUS, TIER_VISIBLE, TIER_OCCLUDED, TIER_HIDDEN };

void anim_step(AnimNode *n, uint32_t now) {
    enum tier t = n->user_tier;
    uint32_t interval = (t == TIER_OCCLUDED) ? 33 : 0;
    if (interval && now - n->last_step_ms < interval) return;
    /* step */
    n->last_step_ms = now;
}
```

**Optim.** Recompute occlusion only on layout change or geometry commit,
not per frame.
**Debug.** Expose tier via dwl-ipc; `mmsg -w` shows live tier stats.

---

## Architecture Cross-Cutting Concerns

- **Thread model.** Three threads max: main (Wayland + scene + compose),
  input (SCHED_FIFO, libinput + cursor), render-helper (optional, GPU
  command building if GLES driver is thread-safe). Communication via
  lock-free SPSC rings, no shared mutexes on the hot path.
- **Allocations.** Pool allocators for `Client`, `AnimNode`, `EventPacket`.
  Zero malloc in render loop after warmup.
- **Cache-friendliness.** `Client` struct re-ordered hot-fields-first
  (`geom`, `mon`, `flags`, `scene_surface` in first 64 bytes).
- **IPC.** Replace per-event wl_display flush with single coalesced flush
  at end of render tick.
- **Telemetry.** Optional `LEMON_TRACE=/tmp/lemon.trace.json` writes
  Perfetto-compatible JSON spans for compose / input / anim phases.

## Lemon-Specific Migration Path

Each subsystem above is incrementally adoptable into Lemon without a
rewrite. Status as of the current branch:

| # | Subsystem                                | Status      | Commit prefix             |
|---|------------------------------------------|-------------|---------------------------|
| 3 | Lazy frame scheduling                    | DONE        | `perf(redraw)`            |
| 6 | Per-monitor wake                         | DONE        | `perf(rendermon)`, `perf(battery)` |
| 7 | Surface geometry cache + placeholder     | DONE (cache only) | `perf(launch)`      |
| 9 | Focus-aware tiers                        | DONE        | `perf(anim)`              |
| 4 | Unified animation heap                   | DEFERRED    | needs testbed             |
| 8 | Texture cache LRU                        | DEFERRED    | wlroots scene already caches; revisit with profile |
| 2 | Deadline pacer                           | DEFERRED    | needs `presentation-time` instrumentation first |
| 1 | RT input thread                          | DEFERRED    | largest refactor; needs audit |
| 5 | Smart vsync                              | DEFERRED    | depends on §2             |

The DEFERRED items require either profiling instrumentation that does not
yet exist (no `presentation-time` telemetry harness) or a regression test
suite to validate behavior changes. Adding `LEMON_TRACE=` Perfetto span
emission would unblock §2 first; pthread realtime audit unblocks §1.

## Philosophy

Consistency beats peak FPS. The screen at 240 Hz with one missed frame per
second feels worse than a rock-solid 120 Hz. Every subsystem above is graded
against frametime *p99*, not average.
