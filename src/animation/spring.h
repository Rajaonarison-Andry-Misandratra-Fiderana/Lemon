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

/* Advance all four axes one step toward target[]. Returns true only when every
   axis has settled; the caller then snaps to the integer target and stops. */
static inline bool spring_box_step(double pos[SPRING_AXES],
								   double vel[SPRING_AXES],
								   const double target[SPRING_AXES], double dt,
								   double m, double k, double c) {
	bool settled = true;
	for (int32_t i = 0; i < SPRING_AXES; i++) {
		pos[i] = spring_axis_step(pos[i], &vel[i], target[i], dt, m, k, c);
		if (!spring_axis_settled(pos[i], vel[i], target[i]))
			settled = false;
	}
	return settled;
}
