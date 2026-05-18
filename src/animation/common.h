/* Return the bezier control-point array configured for the given animation type. */
static inline double *animation_curve_for_type(int32_t type) {
	switch (type) {
	case OPEN:       return config.animation_curve_open;
	case TAG:        return config.animation_curve_tag;
	case CLOSE:      return config.animation_curve_close;
	case FOCUS:      return config.animation_curve_focus;
	case OPAFADEIN:  return config.animation_curve_opafadein;
	case OPAFADEOUT: return config.animation_curve_opafadeout;
	case MOVE:
	default:         return config.animation_curve_move;
	}
}

/* Evaluate the cubic-bezier curve of the given type at parameter t and return the (x,y) point. */
struct dvec2 calculate_animation_curve_at(double t, int32_t type) {
	struct dvec2 point;
	double *animation_curve = animation_curve_for_type(type);

	const double omt = 1.0 - t;
	const double t2 = t * t;
	const double t3 = t2 * t;
	const double a = 3.0 * t * omt * omt;
	const double b = 3.0 * t2 * omt;

	point.x = a * animation_curve[0] + b * animation_curve[2] + t3;
	point.y = a * animation_curve[1] + b * animation_curve[3] + t3;

	return point;
}

/* Precompute lookup tables of bezier samples for each animation curve at startup. */
void init_baked_points(void) {
	struct dvec2 **tables[] = {
		&baked_points_move, &baked_points_open, &baked_points_tag,
		&baked_points_close, &baked_points_focus,
		&baked_points_opafadein, &baked_points_opafadeout,
	};
	int32_t types[] = { MOVE, OPEN, TAG, CLOSE, FOCUS, OPAFADEIN, OPAFADEOUT };
	const double inv = 1.0 / (BAKED_POINTS_COUNT - 1);

	for (size_t k = 0; k < sizeof(types) / sizeof(types[0]); k++) {
		struct dvec2 *buf = calloc(BAKED_POINTS_COUNT, sizeof(*buf));
		*tables[k] = buf;
		for (int32_t i = 0; i < BAKED_POINTS_COUNT; i++) {
			buf[i] = calculate_animation_curve_at((double)i * inv, types[k]);
		}
	}
}

/* Return the precomputed baked-points table for the given animation type. */
static inline struct dvec2 *baked_points_for_type(int32_t type) {
	switch (type) {
	case OPEN:       return baked_points_open;
	case TAG:        return baked_points_tag;
	case CLOSE:      return baked_points_close;
	case FOCUS:      return baked_points_focus;
	case OPAFADEIN:  return baked_points_opafadein;
	case OPAFADEOUT: return baked_points_opafadeout;
	case MOVE:
	default:         return baked_points_move;
	}
}

/* Look up the eased y-value at progress t using binary search over the baked curve points. */
LEMON_HOT double find_animation_curve_at(double t, int32_t type) {
	struct dvec2 *baked_points = baked_points_for_type(type);
	if (t <= 0.0) return baked_points[0].y;
	if (t >= 1.0) return baked_points[BAKED_POINTS_COUNT - 1].y;

	int32_t down = 0;
	int32_t up = BAKED_POINTS_COUNT - 1;
	while (up - down > 1) {
		int32_t middle = (up + down) >> 1;
		if (baked_points[middle].x <= t) {
			down = middle;
		} else {
			up = middle;
		}
	}
	return baked_points[up].y;
}

/* Recursively snapshot a scene node subtree into a new tree, copying buffer and shadow state. */
static bool scene_node_snapshot(struct wlr_scene_node *node, int32_t lx,
								int32_t ly,
								struct wlr_scene_tree *snapshot_tree) {
	if (!node->enabled && node->type != WLR_SCENE_NODE_TREE) {
		return true;
	}

	lx += node->x;
	ly += node->y;

	struct wlr_scene_node *snapshot_node = NULL;
	switch (node->type) {
	case WLR_SCENE_NODE_TREE: {
		struct wlr_scene_tree *scene_tree = wlr_scene_tree_from_node(node);

		struct wlr_scene_node *child;
		wl_list_for_each(child, &scene_tree->children, link) {
			scene_node_snapshot(child, lx, ly, snapshot_tree);
		}
		break;
	}
	case WLR_SCENE_NODE_RECT:
		break;
	case WLR_SCENE_NODE_BUFFER: {
		struct wlr_scene_buffer *scene_buffer =
			wlr_scene_buffer_from_node(node);

		struct wlr_scene_buffer *snapshot_buffer =
			wlr_scene_buffer_create(snapshot_tree, NULL);
		if (snapshot_buffer == NULL) {
			return false;
		}
		snapshot_node = &snapshot_buffer->node;
		snapshot_buffer->node.data = scene_buffer->node.data;

		wlr_scene_buffer_set_dest_size(snapshot_buffer, scene_buffer->dst_width,
									   scene_buffer->dst_height);
		wlr_scene_buffer_set_opaque_region(snapshot_buffer,
										   &scene_buffer->opaque_region);
		wlr_scene_buffer_set_source_box(snapshot_buffer,
										&scene_buffer->src_box);
		wlr_scene_buffer_set_transform(snapshot_buffer,
									   scene_buffer->transform);
		wlr_scene_buffer_set_filter_mode(snapshot_buffer,
										 scene_buffer->filter_mode);

		snapshot_buffer->node.data = scene_buffer->node.data;

		struct wlr_scene_surface *scene_surface =
			wlr_scene_surface_try_from_buffer(scene_buffer);
		if (scene_surface != NULL && scene_surface->surface->buffer != NULL) {
			wlr_scene_buffer_set_buffer(snapshot_buffer,
										&scene_surface->surface->buffer->base);
		} else {
			wlr_scene_buffer_set_buffer(snapshot_buffer, scene_buffer->buffer);
		}
		break;
	}
	}

	if (snapshot_node != NULL) {
		wlr_scene_node_set_position(snapshot_node, lx, ly);
	}

	return true;
}

/* Create a detached snapshot scene tree of node under parent, for use in fade-out animations. */
struct wlr_scene_tree *wlr_scene_tree_snapshot(struct wlr_scene_node *node,
											   struct wlr_scene_tree *parent) {
	struct wlr_scene_tree *snapshot = wlr_scene_tree_create(parent);
	if (snapshot == NULL) {
		return NULL;
	}

	wlr_scene_node_set_enabled(&snapshot->node, false);

	if (!scene_node_snapshot(node, 0, 0, snapshot)) {
		wlr_scene_node_destroy(&snapshot->node);
		return NULL;
	}

	wlr_scene_node_set_enabled(&snapshot->node, true);

	return snapshot;
}

/* Schedule a new frame on every enabled monitor to drive animation ticks. */
void request_fresh_all_monitors(void) {
	Monitor *m = NULL;
	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled) {
			continue;
		}
		wlr_output_schedule_frame(m->wlr_output);
	}
}

/* Schedule a frame only on monitors where the client is currently rendered. */
void request_fresh_for_client(Client *c) {
	if (!c || !c->scene_surface) {
		request_fresh_all_monitors();
		return;
	}
	Monitor *m = NULL;
	bool any = false;
	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled)
			continue;
		if (m == c->mon || client_is_rendered_on_mon(c, m)) {
			wlr_output_schedule_frame(m->wlr_output);
			any = true;
		}
	}
	if (!any && c->mon && c->mon->wlr_output->enabled)
		wlr_output_schedule_frame(c->mon->wlr_output);
}
