#pragma once

/* Battery + power-management helpers extracted from lemon.c.
   Polls /sys/class/power_supply for AC adapter state, applies kernel
   timer slack and PM_QOS cpu_dma_latency knobs, schedules deferred
   battery-mode frames, and runs a periodic client-hibernation pass
   that sends xdg suspended to unfocused clients. Single-TU header
   included exactly once from lemon.c. */

/* Return true when all detected AC adapters report offline (system on battery). */
static bool detect_on_battery(void) {
	DIR *d = opendir("/sys/class/power_supply");
	if (!d)
		return false;
	bool found_ac = false;
	bool ac_online = false;
	struct dirent *e;
	while ((e = readdir(d))) {
		if (strncmp(e->d_name, "AC", 2) != 0 &&
		    strncmp(e->d_name, "ADP", 3) != 0)
			continue;
		char path[320];
		snprintf(path, sizeof(path), "/sys/class/power_supply/%s/online",
		         e->d_name);
		FILE *fp = fopen(path, "re");
		if (!fp)
			continue;
		int v = 0;
		if (fscanf(fp, "%d", &v) == 1) {
			found_ac = true;
			if (v)
				ac_online = true;
		}
		fclose(fp);
	}
	closedir(d);
	if (!found_ac)
		return false;
	return !ac_online;
}

/* Widen the kernel timer slack on battery so the scheduler coalesces wakeups
   and the CPU reaches deeper C-states; restore the tight default on AC. */
static void apply_battery_timer_slack(bool battery) {
	unsigned long ns = battery && config.battery_timer_slack_ms > 0
						   ? (unsigned long)config.battery_timer_slack_ms * 1000000UL
						   : 50000UL; /* kernel default ~50 us */
	prctl(PR_SET_TIMERSLACK, ns, 0, 0, 0);
}

/* PM_QOS request via /dev/cpu_dma_latency: bounds the max DMA wakeup latency
   the kernel may impose. Holding the fd open with a low value pins the CPUs
   in shallow C-states, cutting IRQ-to-userspace wakeup from ~100us to ~1us
   on idle cores - felt as steadier input/frame timing. Costs battery, so we
   relax to a higher tolerance on battery. -1 disables the request entirely. */
static int cpu_dma_latency_fd = -1;
static void apply_cpu_dma_latency(bool battery) {
	if (config.cpu_dma_latency_us < 0) {
		if (cpu_dma_latency_fd >= 0) {
			close(cpu_dma_latency_fd);
			cpu_dma_latency_fd = -1;
		}
		return;
	}
	if (cpu_dma_latency_fd < 0) {
		cpu_dma_latency_fd =
			open("/dev/cpu_dma_latency", O_WRONLY | O_CLOEXEC);
		if (cpu_dma_latency_fd < 0)
			return; /* not available (no permission, no kernel support) */
	}
	int32_t v = config.cpu_dma_latency_us;
	if (battery)
		v += 1000; /* allow deeper C-states on battery */
	if (write(cpu_dma_latency_fd, &v, sizeof(v)) != (ssize_t)sizeof(v)) {
		/* Write failure leaves the previous value (or unset). Not fatal. */
	}
}

/* Re-read AC state periodically; reschedule self. On an AC<->battery
   transition, widen/restore the timer slack and re-arm idle timers so the
   battery idle timeout takes effect immediately. Also nudges glibc to release
   freed heap pages back to the kernel - long sessions accumulate freed-but-
   resident memory otherwise. */
static int battery_poll_callback(void *data) {
	(void)data;
	bool was_on_battery = on_battery;
	on_battery = detect_on_battery();
	if (on_battery != was_on_battery) {
		apply_battery_timer_slack(on_battery);
		apply_cpu_dma_latency(on_battery);
		reset_idle_timers();
		if (config.ac_notify) {
			/* -e/--transient + the swaync x-canonical-private-synchronous
			   hint asks every common notification daemon (swaync,
			   mako, dunst) to skip the persistent history -- AC plug
			   events are pure ephemerals, no point archiving them. */
			Arg a = {.v = on_battery
							 ? "notify-send -e -t 3000 -i battery "
							   "-h string:x-canonical-private-synchronous:"
							   "lemon-ac 'Charger Disconnected'"
							 : "notify-send -e -t 3000 -i ac-adapter "
							   "-h string:x-canonical-private-synchronous:"
							   "lemon-ac 'Charger Connected'"};
			spawn_shell(&a);
		}
	}
	malloc_trim(0);
	wl_event_source_timer_update(battery_poll_source, 10000);
	return 0;
}

/* Check whether any idle inhibitor's surface belongs to this client.
   Inhibited clients (typical case: a media player while playing) are
   never sent xdg suspended by the idle hibernation scanner. */
static bool client_has_idle_inhibitor(Client *c) {
	if (!c || !idle_inhibit_mgr || !client_surface(c))
		return false;
	struct wlr_idle_inhibitor_v1 *inh = NULL;
	wl_list_for_each(inh, &idle_inhibit_mgr->inhibitors, link) {
		Client *ic = NULL;
		toplevel_from_wlr_surface(inh->surface, &ic, NULL);
		if (ic == c)
			return true;
	}
	return false;
}

/* Periodic scan: send xdg suspended to every xdg client that has been
   unfocused for more than client_hibernate_idle_secs seconds and is
   not holding an idle inhibitor. Most toolkits stop animating /
   rendering on suspended -- the window keeps its buffer (no flash on
   focus return) but drops to ~0% CPU. */
static struct wl_event_source *client_hibernate_source = NULL;
static int client_hibernate_scan_callback(void *data) {
	(void)data;
	int32_t threshold = config.client_hibernate_idle_secs;
	if (threshold > 0) {
		uint32_t now = (uint32_t)get_now_in_ms();
		uint32_t cutoff = (uint32_t)threshold * 1000u;
		Client *c = NULL;
		wl_list_for_each(c, &clients, link) {
			if (!c || c->iskilling || client_is_x11(c) ||
				client_is_unmanaged(c))
				continue;
			if (selmon && selmon->sel == c)
				continue;
			if (c->isminimized)
				continue;
			if (client_has_idle_inhibitor(c))
				continue;
			bool idle = (now - c->last_active_ms) > cutoff;
			if (idle && !c->suspended_sent)
				client_set_suspended(c, true);
			else if (!idle && c->suspended_sent)
				client_set_suspended(c, false);
		}
	}
	wl_event_source_timer_update(client_hibernate_source, 15000);
	return 0;
}

/* Deferred frame schedule used to cap animation FPS on battery. */
static int battery_frame_throttle_callback(void *data) {
	Monitor *m = data;
	if (m->wlr_output && m->wlr_output->enabled && allow_frame_scheduling)
		wlr_output_schedule_frame(m->wlr_output);
	return 0;
}
