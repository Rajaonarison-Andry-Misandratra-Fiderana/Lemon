/* Damped harmonic oscillator (Hooke's law + viscous friction) integrated with
   semi-implicit (symplectic) Euler. Pure, allocation-free, axis-independent.
   Used to drive interruptible window geometry animations: the target may
   change mid-flight and the preserved velocity naturally bends the path. */

#define SPRING_AXES 4 /* x, y, width, height */

/* Settle thresholds: an axis stops once it is both near its target and slow.
   Position in pixels, velocity in pixels/second. */
#define SPRING_EPS_POS 0.5
#define SPRING_EPS_VEL 1.0

/* Largest integration step (seconds). Clamping dt after a stall (vsync gap,
   resume from sleep) keeps the explicit integrator from exploding. */
#define SPRING_MAX_DT 0.05

/* Velocity (px/s) at which kinetic motion blur reaches full strength. */
#define MOTION_BLUR_REF_SPEED 2500.0

/* One semi-implicit Euler step of a single damped-spring axis. Updates *vel in
   place and returns the new position. m=mass, k=tension, c=friction, dt=sec. */
static inline double spring_axis_step(double pos, double *vel, double target,
									  double dt, double m, double k, double c) {
	double accel = (-k * (pos - target) - c * (*vel)) / m;
	*vel += accel * dt; /* update velocity first (symplectic) */
	return pos + (*vel) * dt;
}

/* True when an axis is close enough to its target and slow enough to stop. */
static inline bool spring_axis_settled(double pos, double vel, double target) {
	return fabs(pos - target) < SPRING_EPS_POS && fabs(vel) < SPRING_EPS_VEL;
}

/* Sub-step ceiling (seconds). At dt larger than this, the explicit
   integrator's accuracy degrades visibly (overshoot + jitter under
   high tension). Splitting the frame's dt into N = ceil(dt / target)
   sub-steps keeps the solution close to the continuous one without
   bumping the visible refresh rate. */
#define SPRING_SUBSTEP_DT 0.008 /* ~125 Hz internal */

/* Advance all four axes one step toward target[]. Returns true only when every
   axis has settled; the caller then snaps to the integer target and stops.
   Sub-steps internally when dt exceeds SPRING_SUBSTEP_DT so frame-rate
   throttling (battery mode, VRR low-refresh) doesn't bend the spring
   curve. */
static inline bool spring_box_step(double pos[SPRING_AXES],
								   double vel[SPRING_AXES],
								   const double target[SPRING_AXES], double dt,
								   double m, double k, double c) {
	int32_t n = 1;
	if (dt > SPRING_SUBSTEP_DT) {
		n = (int32_t)(dt / SPRING_SUBSTEP_DT) + 1;
		/* Hard ceiling: pathological dt after stall would otherwise burn
		   CPU running hundreds of micro-steps. SPRING_MAX_DT already
		   clamps dt to 50 ms upstream; 8 sub-steps cover that range. */
		if (n > 8)
			n = 8;
	}
	double sub_dt = dt / n;
	bool settled = false;
	for (int32_t step = 0; step < n; step++) {
		settled = true;
		for (int32_t i = 0; i < SPRING_AXES; i++) {
			pos[i] =
				spring_axis_step(pos[i], &vel[i], target[i], sub_dt, m, k, c);
			if (!spring_axis_settled(pos[i], vel[i], target[i]))
				settled = false;
		}
		if (settled)
			break;
	}
	return settled;
}
