#include "wlr/util/log.h"
/* Compute the client's content size (current animation geom minus borders). */
void client_actual_size(Client *c, int32_t *width, int32_t *height) {
	*width = c->animation.current.width - 2 * (int32_t)c->bw;

	*height = c->animation.current.height - 2 * (int32_t)c->bw;
}

/* Resize a scene rect, clamping width and height to non-negative values. */
void set_rect_size(struct wlr_scene_rect *rect, int32_t width, int32_t height) {
	wlr_scene_rect_set_size(rect, GEZERO(width), GEZERO(height));
}

/* Return true if the monitor's current layout stacks clients horizontally (tile/deck). */
bool is_horizontal_stack_layout(Monitor *m) {

	if (m->pertag->curtag &&
		(m->pertag->ltidxs[m->pertag->curtag]->id == TILE ||
		 m->pertag->ltidxs[m->pertag->curtag]->id == DECK))
		return true;

	return false;
}

/* Return true if the monitor's current layout is the right-tile horizontal stack. */
bool is_horizontal_right_stack_layout(Monitor *m) {

	if (m->pertag->curtag &&
		(m->pertag->ltidxs[m->pertag->curtag]->id == RIGHT_TILE))
		return true;

	return false;
}

/* Pick a forced slide-in direction for tiled clients based on layout and master configuration. */
int32_t is_special_animation_rule(Client *c) {

	if (is_scroller_layout(c->mon) && !c->isfloating) {
		return DOWN;
	} else if (c->mon->visible_tiling_clients == 1 && !c->isfloating) {
		return DOWN;
	} else if (c->mon->visible_tiling_clients == 2 && !c->isfloating &&
			   !config.new_is_master && is_horizontal_stack_layout(c->mon)) {
		return RIGHT;
	} else if (!c->isfloating && config.new_is_master &&
			   is_horizontal_stack_layout(c->mon)) {
		return LEFT;
	} else if (c->mon->visible_tiling_clients == 2 && !c->isfloating &&
			   !config.new_is_master &&
			   is_horizontal_right_stack_layout(c->mon)) {
		return LEFT;
	} else if (!c->isfloating && config.new_is_master &&
			   is_horizontal_right_stack_layout(c->mon)) {
		return RIGHT;
	} else {
		return UNDIR;
	}
}

/* Compute the client's initial geometry for its open animation (fade/zoom/slide). */
void set_client_open_animation(Client *c, struct wlr_box geo) {
	int32_t slide_direction;
	int32_t horizontal, horizontal_value;
	int32_t vertical, vertical_value;
	int32_t special_direction;
	int32_t center_x, center_y;

	int32_t open_t = anim_type_effective(c->animation_type_open,
	                                     config.animation_type_open);
	if (open_t == ANIM_TYPE_FADE) {
		c->animainit_geom.width = geo.width;
		c->animainit_geom.height = geo.height;
		c->animainit_geom.x = geo.x;
		c->animainit_geom.y = geo.y;
		return;
	} else if (open_t == ANIM_TYPE_ZOOM) {
		c->animainit_geom.width = geo.width * config.zoom_initial_ratio;
		c->animainit_geom.height = geo.height * config.zoom_initial_ratio;
		c->animainit_geom.x = geo.x + (geo.width - c->animainit_geom.width) / 2;
		c->animainit_geom.y =
			geo.y + (geo.height - c->animainit_geom.height) / 2;
		return;
	} else {
		special_direction = is_special_animation_rule(c);
		center_x = c->geom.x + c->geom.width / 2;
		center_y = c->geom.y + c->geom.height / 2;
		if (special_direction == UNDIR) {
			horizontal = c->mon->w.x + c->mon->w.width - center_x <
								 center_x - c->mon->w.x
							 ? RIGHT
							 : LEFT;
			horizontal_value = horizontal == LEFT
								   ? center_x - c->mon->w.x
								   : c->mon->w.x + c->mon->w.width - center_x;
			vertical = c->mon->w.y + c->mon->w.height - center_y <
							   center_y - c->mon->w.y
						   ? DOWN
						   : UP;
			vertical_value = vertical == UP
								 ? center_y - c->mon->w.y
								 : c->mon->w.y + c->mon->w.height - center_y;
			slide_direction =
				horizontal_value < vertical_value ? horizontal : vertical;
		} else {
			slide_direction = special_direction;
		}
		c->animainit_geom.width = c->geom.width;
		c->animainit_geom.height = c->geom.height;
		switch (slide_direction) {
		case UP:
			c->animainit_geom.x = c->geom.x;
			c->animainit_geom.y = c->mon->m.y - c->geom.height;
			break;
		case DOWN:
			c->animainit_geom.x = c->geom.x;
			c->animainit_geom.y =
				c->geom.y + c->mon->m.height - (c->geom.y - c->mon->m.y);
			break;
		case LEFT:
			c->animainit_geom.x = c->mon->m.x - c->geom.width;
			c->animainit_geom.y = c->geom.y;
			break;
		case RIGHT:
			c->animainit_geom.x =
				c->geom.x + c->mon->m.width - (c->geom.x - c->mon->m.x);
			c->animainit_geom.y = c->geom.y;
			break;
		default:
			c->animainit_geom.x = c->geom.x;
			c->animainit_geom.y = 0 - c->geom.height;
		}
	}
}

/* Per-buffer callback that resizes snapshot buffers to the target close-animation size. */
void snap_scene_buffer_apply_effect(struct wlr_scene_buffer *buffer, int32_t sx,
									int32_t sy, void *data) {
	BufferData *buffer_data = (BufferData *)data;
	wlr_scene_buffer_set_dest_size(buffer, buffer_data->width,
								   buffer_data->height);
}

/* Per-buffer callback that scales surface buffers and applies corner radius during animation. */
void scene_buffer_apply_effect(struct wlr_scene_buffer *buffer, int32_t sx,
							   int32_t sy, void *data) {
	BufferData *buffer_data = (BufferData *)data;

	if (buffer_data->should_scale && buffer_data->height_scale < 1 &&
		buffer_data->width_scale < 1) {
		buffer_data->should_scale = false;
	}

	if (buffer_data->should_scale && buffer_data->height_scale == 1 &&
		buffer_data->width_scale < 1) {
		buffer_data->should_scale = false;
	}

	if (buffer_data->should_scale && buffer_data->height_scale < 1 &&
		buffer_data->width_scale == 1) {
		buffer_data->should_scale = false;
	}

	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(buffer);

	if (scene_surface == NULL)
		return;

	struct wlr_surface *surface = scene_surface->surface;

	if (buffer_data->should_scale) {

		int32_t surface_width = surface->current.width;
		int32_t surface_height = surface->current.height;

		surface_width = buffer_data->width_scale < 1
							? surface_width
							: buffer_data->width_scale * surface_width;
		surface_height = buffer_data->height_scale < 1
							 ? surface_height
							 : buffer_data->height_scale * surface_height;

		if (surface_width > buffer_data->width &&
			wlr_subsurface_try_from_wlr_surface(surface) == NULL) {
			surface_width = buffer_data->width;
		}

		if (surface_height > buffer_data->height &&
			wlr_subsurface_try_from_wlr_surface(surface) == NULL) {
			surface_height = buffer_data->height;
		}

		if (surface_width > buffer_data->width &&
			wlr_subsurface_try_from_wlr_surface(surface) != NULL) {
			return;
		}

		if (surface_height > buffer_data->height &&
			wlr_subsurface_try_from_wlr_surface(surface) != NULL) {
			return;
		}

		if (surface_height > 0 && surface_width > 0) {
			wlr_scene_buffer_set_dest_size(buffer, surface_width,
										   surface_height);
		}
	}

	(void)buffer_data;
}

/* Apply scaling to every buffer in the client's surface tree. */
void buffer_set_effect(Client *c, BufferData data) {

	if (!c || c->iskilling)
		return;

	if (c->animation.tagouting || c->animation.tagouted ||
		c->animation.tagining) {
		data.should_scale = false;
	}

	if (c == grabc)
		data.should_scale = false;

	wlr_scene_node_for_each_buffer(&c->scene_surface->node,
								   scene_buffer_apply_effect, &data);
}

/* Show or hide the dwindle split-direction indicator bars for the client. */
void apply_split_border(Client *c, bool hit_no_border) {

	if (c->iskilling || !c->mon || !client_surface(c)->mapped)
		return;

	const Layout *layout = c->mon->pertag->ltidxs[c->mon->pertag->curtag];

	if (hit_no_border || !ISTILED(c) || layout->id != DWINDLE ||
		!config.dwindle_manual_split) {
		if (c->splitindicator[0] && c->splitindicator[0]->node.enabled)
			wlr_scene_node_set_enabled(&c->splitindicator[0]->node, false);
		if (c->splitindicator[1] && c->splitindicator[1]->node.enabled)
			wlr_scene_node_set_enabled(&c->splitindicator[1]->node, false);
		return;
	} else {

		DwindleNode **root =
			&c->mon->pertag->dwindle_root[c->mon->pertag->curtag];
		DwindleNode *dnode = dwindle_find_leaf(*root, c);

		/* Lazy-allocate the split indicator rects on first dwindle entry. */
		for (int32_t si = 0; si < 2; si++) {
			if (!c->splitindicator[si]) {
				c->splitindicator[si] = wlr_scene_rect_create(
					c->scene, 0, 0,
					c->isurgent ? config.urgentcolor : config.splitcolor);
				c->splitindicator[si]->node.data = c;
				wlr_scene_node_lower_to_bottom(&c->splitindicator[si]->node);
				wlr_scene_node_set_enabled(&c->splitindicator[si]->node, false);
			}
		}

		if (!dnode) {
			wlr_scene_node_set_enabled(&c->splitindicator[0]->node, false);
			wlr_scene_node_set_enabled(&c->splitindicator[1]->node, false);
			return;
		} else {
			if (dnode->custom_leaf_split_h) {
				wlr_scene_node_set_enabled(&c->splitindicator[0]->node, false);
				wlr_scene_node_set_enabled(&c->splitindicator[1]->node, true);
			} else {
				wlr_scene_node_set_enabled(&c->splitindicator[0]->node, true);
				wlr_scene_node_set_enabled(&c->splitindicator[1]->node, false);
			}
		}
	}

	struct wlr_box fullgeom = c->animation.current;

	int32_t bw = (int32_t)c->bw;

	int32_t right_offset, bottom_offset, left_offset, top_offset;

	if (c == grabc) {
		right_offset = 0;
		bottom_offset = 0;
		left_offset = 0;
		top_offset = 0;
	} else {
		right_offset =
			GEZERO(c->animation.current.x + c->animation.current.width -
				   c->mon->m.x - c->mon->m.width);
		bottom_offset =
			GEZERO(c->animation.current.y + c->animation.current.height -
				   c->mon->m.y - c->mon->m.height);

		left_offset = GEZERO(c->mon->m.x - c->animation.current.x);
		top_offset = GEZERO(c->mon->m.y - c->animation.current.y);
	}

	int32_t border_down_width =
		GEZERO(fullgeom.width - 2 * config.border_radius -
			   GEZERO((left_offset + right_offset) - config.border_radius));
	int32_t border_down_height =
		GEZERO(bw - bottom_offset - GEZERO(top_offset + bw - fullgeom.height));

	int32_t border_right_width =
		GEZERO(bw - right_offset - GEZERO(left_offset + bw - fullgeom.width));
	int32_t border_right_height =
		GEZERO(fullgeom.height - 2 * config.border_radius -
			   GEZERO((top_offset + bottom_offset) - config.border_radius));

	int32_t border_down_x = GEZERO(config.border_radius +
								   GEZERO(left_offset - config.border_radius));
	int32_t border_down_y = GEZERO(fullgeom.height - bw) +
							GEZERO(top_offset + bw - fullgeom.height);

	int32_t border_right_x =
		GEZERO(fullgeom.width - bw) + GEZERO(left_offset + bw - fullgeom.width);
	int32_t border_right_y = GEZERO(config.border_radius +
									GEZERO(top_offset - config.border_radius));

	set_rect_size(c->splitindicator[0], border_down_width, border_down_height);
	set_rect_size(c->splitindicator[1], border_right_width,
				  border_right_height);
	wlr_scene_node_set_position(&c->splitindicator[0]->node, border_down_x,
								border_down_y);
	wlr_scene_node_set_position(&c->splitindicator[1]->node, border_right_x,
								border_right_y);
}

/* Lay out the client's border rectangle and corner radius matching the current animation frame. */
void apply_border(Client *c) {
	if (!c || c->iskilling || !client_surface(c)->mapped)
		return;

	bool hit_no_border = check_hit_no_border(c);

	apply_split_border(c, hit_no_border);

	if (hit_no_border && config.smartgaps) {
		c->bw = 0;
		c->fake_no_border = true;
	} else if (hit_no_border && !config.smartgaps) {
		wlr_scene_rect_set_size(c->border, 0, 0);
		wlr_scene_node_set_position(&c->scene_surface->node, c->bw, c->bw);
		c->fake_no_border = true;
		return;
	} else if (!c->isfullscreen && VISIBLEON(c, c->mon)) {
		c->bw = c->isnoborder ? 0 : config.borderpx;
		c->fake_no_border = false;
	}

	struct wlr_box clip_box = c->animation.current;

	int32_t bw = (int32_t)c->bw;

	int32_t right_offset, bottom_offset, left_offset, top_offset;

	if (c == grabc) {
		right_offset = 0;
		bottom_offset = 0;
		left_offset = 0;
		top_offset = 0;
	} else {
		right_offset =
			GEZERO(c->animation.current.x + c->animation.current.width -
				   c->mon->m.x - c->mon->m.width);
		bottom_offset =
			GEZERO(c->animation.current.y + c->animation.current.height -
				   c->mon->m.y - c->mon->m.height);

		left_offset = GEZERO(c->mon->m.x - c->animation.current.x);
		top_offset = GEZERO(c->mon->m.y - c->animation.current.y);
	}

	int32_t inner_surface_width = GEZERO(clip_box.width - 2 * bw);
	int32_t inner_surface_height = GEZERO(clip_box.height - 2 * bw);

	int32_t inner_surface_x = GEZERO(bw - left_offset);
	int32_t inner_surface_y = GEZERO(bw - top_offset);

	int32_t rect_x = left_offset;
	int32_t rect_y = top_offset;

	int32_t rect_width =
		GEZERO(c->animation.current.width - left_offset - right_offset);
	int32_t rect_height =
		GEZERO(c->animation.current.height - top_offset - bottom_offset);

	if (left_offset > c->bw)
		inner_surface_width =
			inner_surface_width - left_offset + (int32_t)c->bw;

	if (top_offset > c->bw)
		inner_surface_height =
			inner_surface_height - top_offset + (int32_t)c->bw;

	if (right_offset > 0) {
		inner_surface_width =
			MIN(clip_box.width, inner_surface_width + right_offset);
	}

	if (bottom_offset > 0) {
		inner_surface_height =
			MIN(clip_box.height, inner_surface_height + bottom_offset);
	}

	(void)inner_surface_x; (void)inner_surface_y;
	(void)inner_surface_width; (void)inner_surface_height;

	wlr_scene_node_set_position(&c->scene_surface->node, c->bw, c->bw);
	wlr_scene_rect_set_size(c->border, rect_width, rect_height);
	wlr_scene_node_set_position(&c->border->node, rect_x, rect_y);
}

/* Shrink the clip box and toggle visibility for clients sliding off-screen during tag transitions. */
struct ivec2 clip_to_hide(Client *c, struct wlr_box *clip_box) {
	int32_t offsetx = 0, offsety = 0, offsetw = 0, offseth = 0;
	struct ivec2 offset = {0, 0, 0, 0};

	if (!ISSCROLLTILED(c) && !c->animation.tagining && !c->animation.tagouted &&
		!c->animation.tagouting)
		return offset;

	int32_t bottom_out_offset =
		GEZERO(c->animation.current.y + c->animation.current.height -
			   c->mon->m.y - c->mon->m.height);
	int32_t right_out_offset =
		GEZERO(c->animation.current.x + c->animation.current.width -
			   c->mon->m.x - c->mon->m.width);
	int32_t left_out_offset = GEZERO(c->mon->m.x - c->animation.current.x);
	int32_t top_out_offset = GEZERO(c->mon->m.y - c->animation.current.y);

	int32_t bw = (int32_t)c->bw;

	if (ISSCROLLTILED(c) || c->animation.tagining || c->animation.tagouted ||
		c->animation.tagouting) {
		if (left_out_offset > 0) {
			offsetx = GEZERO(left_out_offset - bw);
			clip_box->x = clip_box->x + offsetx;
			clip_box->width = clip_box->width - offsetx;
		} else if (right_out_offset > 0) {
			offsetw = GEZERO(right_out_offset - bw);
			clip_box->width = clip_box->width - offsetw;
		}

		if (top_out_offset > 0) {
			offsety = GEZERO(top_out_offset - bw);
			clip_box->y = clip_box->y + offsety;
			clip_box->height = clip_box->height - offsety;
		} else if (bottom_out_offset > 0) {
			offseth = GEZERO(bottom_out_offset - bw);
			clip_box->height = clip_box->height - offseth;
		}
	}

	offset.x = offsetx;
	offset.y = offsety;
	offset.width = offsetw;
	offset.height = offseth;

	if ((clip_box->width + bw <= 0 || clip_box->height + bw <= 0) &&
		(ISSCROLLTILED(c) || c->animation.tagouting || c->animation.tagining)) {
		c->is_clip_to_hide = true;
		wlr_scene_node_set_enabled(&c->scene->node, false);
	} else if (c->is_clip_to_hide && VISIBLEON(c, c->mon)) {
		c->is_clip_to_hide = false;
		wlr_scene_node_set_enabled(&c->scene->node, true);
	}

	return offset;
}

/* Compute and display the drag-and-drop target area highlight for a client under the cursor. */
void client_set_drop_area(Client *c) {
	bool first_draw = false;
	int32_t drop_direction = UNDIR;

	if (!c || !c->mon)
		return;

	/* Fast path: dropping disabled and rect never allocated. */
	if (!c->enable_drop_area_draw && !c->droparea)
		return;

	if (!c->enable_drop_area_draw && c->droparea && c->droparea->node.enabled) {
		wlr_scene_node_lower_to_bottom(&c->droparea->node);
		wlr_scene_node_set_enabled(&c->droparea->node, false);
		return;
	}

	if (c->enable_drop_area_draw && !c->droparea) {
		c->droparea = wlr_scene_rect_create(c->scene, 0, 0, config.dropcolor);
		wlr_scene_node_lower_to_bottom(&c->droparea->node);
		wlr_scene_node_set_position(&c->droparea->node, 0, 0);
	}

	if (c->enable_drop_area_draw && c->droparea &&
	    !c->droparea->node.enabled) {
		wlr_scene_node_raise_to_top(&c->droparea->node);
		wlr_scene_node_set_enabled(&c->droparea->node, true);
		first_draw = true;
	}

	int32_t bw = (int32_t)c->bw;
	int32_t client_width = c->geom.width - 2 * bw;
	int32_t client_height = c->geom.height - 2 * bw;

	double rel_x = cursor->x - c->geom.x - bw;
	double rel_y = cursor->y - c->geom.y - bw;

	struct wlr_box drop_box;

	const Layout *cur_layout = c->mon->pertag->ltidxs[c->mon->pertag->curtag];
	bool dwindle_familiar =
		cur_layout->id == DWINDLE && config.dwindle_drop_simple_split;

	uint32_t nmaster = c->mon->pertag->nmasters[c->mon->pertag->curtag];

	bool should_swap =
		(cur_layout->id == DECK || cur_layout->id == VERTICAL_DECK ||
		 cur_layout->id == MONOCLE || cur_layout->id == GRID ||
		 cur_layout->id == FAIR || cur_layout->id == VERTICAL_FAIR ||
		 cur_layout->id == VERTICAL_GRID) ||
		((cur_layout->id == TILE || cur_layout->id == VERTICAL_TILE ||
		  cur_layout->id == CENTER_TILE || cur_layout->id == RIGHT_TILE) &&
		 nmaster == 1 && c->ismaster);

	if (dwindle_familiar) {
		bool split_h = c->geom.width >= c->geom.height;
		float ratio = config.dwindle_split_ratio;
		if (split_h) {
			if (rel_x < client_width * 0.5) {
				drop_direction = LEFT;
				drop_box.x = bw;
				drop_box.y = bw;
				drop_box.width = (int32_t)(client_width * ratio);
				drop_box.height = client_height;
			} else {
				drop_direction = RIGHT;
				drop_box.x = bw + (int32_t)(client_width * ratio);
				drop_box.y = bw;
				drop_box.width = client_width - (int32_t)(client_width * ratio);
				drop_box.height = client_height;
			}
		} else {
			if (rel_y < client_height * 0.5) {
				drop_direction = UP;
				drop_box.x = bw;
				drop_box.y = bw;
				drop_box.width = client_width;
				drop_box.height = (int32_t)(client_height * ratio);
			} else {
				drop_direction = DOWN;
				drop_box.x = bw;
				drop_box.y = bw + (int32_t)(client_height * ratio);
				drop_box.width = client_width;
				drop_box.height =
					client_height - (int32_t)(client_height * ratio);
			}
		}
	} else if (should_swap) {
		drop_box.x = bw;
		drop_box.y = bw;
		drop_box.width = client_width;
		drop_box.height = client_height;
		drop_direction = UNDIR;
	} else if (cur_layout->id == TILE || cur_layout->id == DECK ||
			   cur_layout->id == CENTER_TILE || cur_layout->id == RIGHT_TILE) {
		if (rel_y < client_height * 0.5) {
			drop_direction = UP;
			drop_box.x = bw;
			drop_box.y = bw;
			drop_box.width = client_width;
			drop_box.height = client_height / 2;
		} else {
			drop_direction = DOWN;
			drop_box.x = bw;
			drop_box.y = bw + client_height / 2;
			drop_box.width = client_width;
			drop_box.height = client_height / 2;
		}
	} else if (cur_layout->id == VERTICAL_TILE ||
			   cur_layout->id == VERTICAL_DECK) {
		if (rel_x < client_width * 0.5) {
			drop_direction = LEFT;
			drop_box.x = bw;
			drop_box.y = bw;
			drop_box.width = client_width / 2;
			drop_box.height = client_height;
		} else {
			drop_direction = RIGHT;
			drop_box.x = bw + client_width / 2;
			drop_box.y = bw;
			drop_box.width = client_width / 2;
			drop_box.height = client_height;
		}
	} else {
		double dist_left = rel_x;
		double dist_right = client_width - rel_x;
		double dist_top = rel_y;
		double dist_bottom = client_height - rel_y;

		if (dist_left <= dist_right && dist_left <= dist_top &&
			dist_left <= dist_bottom) {
			drop_direction = LEFT;
			drop_box.x = bw;
			drop_box.y = bw;
			drop_box.width = client_width / 2;
			drop_box.height = client_height;
		} else if (dist_right <= dist_top && dist_right <= dist_bottom) {
			drop_direction = RIGHT;
			drop_box.x = bw + client_width / 2;
			drop_box.y = bw;
			drop_box.width = client_width / 2;
			drop_box.height = client_height;
		} else if (dist_top <= dist_bottom) {
			drop_direction = UP;
			drop_box.x = bw;
			drop_box.y = bw;
			drop_box.width = client_width;
			drop_box.height = client_height / 2;
		} else {
			drop_direction = DOWN;
			drop_box.x = bw;
			drop_box.y = bw + client_height / 2;
			drop_box.width = client_width;
			drop_box.height = client_height / 2;
		}
	}

	if (!first_draw && c->drop_direction == drop_direction) {
		return;
	}
	c->drop_direction = drop_direction;

	wlr_scene_node_set_position(&c->droparea->node, drop_box.x, drop_box.y);
	wlr_scene_rect_set_size(c->droparea, drop_box.width, drop_box.height);
}

/* Apply the current animation clip box, border, shadow and surface scaling to the client. */
LEMON_HOT void client_apply_clip(Client *c, float factor) {

	if (c->iskilling || !client_surface(c)->mapped)
		return;

	struct wlr_box clip_box;
	bool should_render_client_surface = false;
	struct ivec2 offset;
	BufferData buffer_data;

	if (!config.animations) {
		c->animation.running = false;
		c->need_output_flush = false;
		c->animainit_geom = c->current = c->pending = c->animation.current =
			c->geom;

		client_get_clip(c, &clip_box);

		offset = clip_to_hide(c, &clip_box);

		apply_border(c);

		if (clip_box.width <= 0 || clip_box.height <= 0) {
			return;
		}

		wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip_box);
		buffer_set_effect(c, (BufferData){1.0f, 1.0f, clip_box.width,
										  clip_box.height, true});
		return;
	}

	int32_t width, height;
	client_actual_size(c, &width, &height);

	struct wlr_box geometry;
	client_get_geometry(c, &geometry);
	clip_box = (struct wlr_box){
		.x = geometry.x,
		.y = geometry.y,
		.width = width,
		.height = height,
	};

	if (client_is_x11(c)) {
		clip_box.x = 0;
		clip_box.y = 0;
	}

	offset = clip_to_hide(c, &clip_box);

	apply_border(c);

	if (clip_box.width <= 0 || clip_box.height <= 0) {
		should_render_client_surface = false;
		wlr_scene_node_set_enabled(&c->scene_surface->node, false);
	} else {
		should_render_client_surface = true;
		wlr_scene_node_set_enabled(&c->scene_surface->node, true);
	}

	if (!should_render_client_surface) {
		return;
	}

	wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip_box);

	int32_t acutal_surface_width = geometry.width - offset.x - offset.width;
	int32_t acutal_surface_height = geometry.height - offset.y - offset.height;

	if (acutal_surface_width <= 0 || acutal_surface_height <= 0)
		return;

	buffer_data.should_scale = true;
	buffer_data.width = clip_box.width;
	buffer_data.height = clip_box.height;

	if (factor == 1.0) {
		buffer_data.width_scale = 1.0;
		buffer_data.height_scale = 1.0;
	} else {
		buffer_data.width_scale =
			(float)buffer_data.width / acutal_surface_width;
		buffer_data.height_scale =
			(float)buffer_data.height / acutal_surface_height;
	}

	buffer_set_effect(c, buffer_data);
}

/* Advance one frame of a closing client's fade-out animation and destroy it when finished. */
LEMON_HOT void fadeout_client_animation_next_tick(Client *c) {
	if (!c)
		return;

	BufferData buffer_data;

	int32_t passed_time = frame_now_ms() - c->animation.time_started;
	double animation_passed =
		c->animation.duration
			? (double)passed_time / (double)c->animation.duration
			: 1.0;

	int32_t type = c->animation.action;
	double factor = find_animation_curve_at(animation_passed, type);

	int32_t width = c->animation.initial.width +
					(c->current.width - c->animation.initial.width) * factor;
	int32_t height = c->animation.initial.height +
					 (c->current.height - c->animation.initial.height) * factor;

	int32_t x = c->animation.initial.x +
				(c->current.x - c->animation.initial.x) * factor;
	int32_t y = c->animation.initial.y +
				(c->current.y - c->animation.initial.y) * factor;

	wlr_scene_node_set_position(&c->scene->node, x, y);

	c->animation.current = (struct wlr_box){
		.x = x,
		.y = y,
		.width = width,
		.height = height,
	};

	double opacity_eased_progress =
		find_animation_curve_at(animation_passed, OPAFADEOUT);

	double percent = config.fadeout_begin_opacity -
					 (opacity_eased_progress * config.fadeout_begin_opacity);

	double opacity = MAX(percent, 0);

	if (config.animation_fade_out && !c->nofadeout)
		wlr_scene_node_for_each_buffer(&c->scene->node,
									   scene_buffer_apply_opacity, &opacity);

	if (anim_type_effective(c->animation_type_close,
	                        config.animation_type_close) == ANIM_TYPE_ZOOM) {

		buffer_data.width = width;
		buffer_data.height = height;
		buffer_data.width_scale = animation_passed;
		buffer_data.height_scale = animation_passed;

		wlr_scene_node_for_each_buffer(
			&c->scene->node, snap_scene_buffer_apply_effect, &buffer_data);
	}

	if (animation_passed >= 1.0) {
		wl_list_remove(&c->fadeout_link);
		wlr_scene_node_destroy(&c->scene->node);
		free(c);
		c = NULL;
	}
}

/* Advance one frame of a client's running geometry animation and finalize when complete. */
LEMON_HOT void client_animation_next_tick(Client *c) {
	Client *pointer_c = NULL;
	double sx = 0, sy = 0;
	struct wlr_surface *surface = NULL;

	int32_t type = c->animation.action == NONE ? MOVE : c->animation.action;

	/* Spring physics drives plain geometry moves/resizes/tiling. Open, close
	   and tag transitions keep the curve model (they need a 0..1 progress for
	   their fade/zoom/slide and offset logic). */
	bool use_spring = config.animation_spring && !c->animation.tagining &&
					  !c->animation.tagouting && type != OPEN && type != CLOSE;

	double animation_passed;
	double factor;
	int32_t x, y, width, height;

	if (use_spring) {
		uint32_t now = frame_now_ms();
		double dt = c->animation.last_tick_ms
						? (double)(now - c->animation.last_tick_ms) / 1000.0
						: 1.0 / 60.0;
		if (dt > SPRING_MAX_DT)
			dt = SPRING_MAX_DT;
		if (dt <= 0.0)
			dt = 1.0 / 1000.0;
		c->animation.last_tick_ms = now;

		if (!c->animation.spring_init) {
			c->animation.vis[0] = c->animation.initial.x;
			c->animation.vis[1] = c->animation.initial.y;
			c->animation.vis[2] = c->animation.initial.width;
			c->animation.vis[3] = c->animation.initial.height;
			c->animation.vel[0] = c->animation.vel[1] = 0.0;
			c->animation.vel[2] = c->animation.vel[3] = 0.0;
			c->animation.spring_init = true;
		}

		const double target[4] = {
			(double)c->current.x, (double)c->current.y,
			(double)c->current.width, (double)c->current.height,
		};
		bool settled = spring_box_step(
			c->animation.vis, c->animation.vel, target, dt,
			config.animation_spring_mass, config.animation_spring_tension,
			config.animation_spring_friction);

		if (settled) {
			x = c->current.x;
			y = c->current.y;
			width = c->current.width;
			height = c->current.height;
		} else {
			x = (int32_t)lround(c->animation.vis[0]);
			y = (int32_t)lround(c->animation.vis[1]);
			width = (int32_t)lround(c->animation.vis[2]);
			height = (int32_t)lround(c->animation.vis[3]);
		}
		animation_passed = settled ? 1.0 : 0.0;
		factor = 1.0;
	} else {
		int32_t passed_time = frame_now_ms() - c->animation.time_started;
		animation_passed =
			c->animation.duration
				? (double)passed_time / (double)c->animation.duration
				: 1.0;
		factor = find_animation_curve_at(animation_passed, type);

		width = c->animation.initial.width +
				(c->current.width - c->animation.initial.width) * factor;
		height = c->animation.initial.height +
				 (c->current.height - c->animation.initial.height) * factor;
		x = c->animation.initial.x +
			(c->current.x - c->animation.initial.x) * factor;
		y = c->animation.initial.y +
			(c->current.y - c->animation.initial.y) * factor;
	}

	wlr_scene_node_set_position(&c->scene->node, x, y);
	c->animation.current = (struct wlr_box){
		.x = x,
		.y = y,
		.width = width,
		.height = height,
	};

	c->is_pending_open_animation = false;

	client_apply_clip(c, factor);

	if (animation_passed >= 1.0) {

		c->animation.action = MOVE;

		c->animation.tagining = false;
		c->animation.running = false;
		c->animation.spring_init = false;
		c->animation.last_tick_ms = 0;

		if (c->animation.tagouting) {
			c->animation.tagouting = false;
			wlr_scene_node_set_enabled(&c->scene->node, false);
			if (config.tag_suspend_hidden)
				client_set_suspended(c, true);
			c->animation.tagouted = true;
			c->animation.current = c->geom;
		}

		xytonode(cursor->x, cursor->y, NULL, &pointer_c, NULL, &sx, &sy);

		surface =
			pointer_c && pointer_c == c ? client_surface(pointer_c) : NULL;

		if (surface && pointer_c == selmon->sel && !selmon->isoverview) {
			wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		}

		c->need_output_flush = false;
	}
}

/* Create a snapshot-based fade-out client for a closing window and queue it for animation. */
void init_fadeout_client(Client *c) {

	if (!c->mon || client_is_unmanaged(c))
		return;

	if (!c->scene) {
		return;
	}

	if (anim_type_effective(c->animation_type_close,
	                        config.animation_type_close) == ANIM_TYPE_NONE) {
		return;
	}

	Client *fadeout_client = ecalloc(1, sizeof(*fadeout_client));

	wlr_scene_node_set_enabled(&c->scene->node, true);
	client_set_border_color(c, config.bordercolor);
	fadeout_client->scene =
		wlr_scene_tree_snapshot(&c->scene->node, layers[LyrFadeOut]);
	wlr_scene_node_set_enabled(&c->scene->node, false);

	if (!fadeout_client->scene) {
		free(fadeout_client);
		return;
	}

	fadeout_client->animation.duration = config.animation_duration_close;
	fadeout_client->geom = fadeout_client->current =
		fadeout_client->animainit_geom = fadeout_client->animation.initial =
			c->animation.current;
	fadeout_client->mon = c->mon;
	fadeout_client->animation_type_close = c->animation_type_close;
	fadeout_client->animation.action = CLOSE;
	fadeout_client->bw = c->bw;
	fadeout_client->nofadeout = c->nofadeout;

	fadeout_client->animation.initial.x = 0;
	fadeout_client->animation.initial.y = 0;

	int32_t close_t = anim_type_effective(c->animation_type_close,
	                                      config.animation_type_close);
	if (close_t == ANIM_TYPE_FADE) {
		fadeout_client->current.x = 0;
		fadeout_client->current.y = 0;
		fadeout_client->current.width = 0;
		fadeout_client->current.height = 0;
	} else if (close_t == ANIM_TYPE_SLIDE) {
		fadeout_client->current.y =
			c->geom.y + c->geom.height / 2 > c->mon->m.y + c->mon->m.height / 2
				? c->mon->m.height -
					  (c->animation.current.y - c->mon->m.y)
				: c->mon->m.y - c->geom.height;
		fadeout_client->current.x = 0;
	} else {
		fadeout_client->current.y =
			(fadeout_client->geom.height -
			 fadeout_client->geom.height * config.zoom_end_ratio) /
			2;
		fadeout_client->current.x =
			(fadeout_client->geom.width -
			 fadeout_client->geom.width * config.zoom_end_ratio) /
			2;
		fadeout_client->current.width =
			fadeout_client->geom.width * config.zoom_end_ratio;
		fadeout_client->current.height =
			fadeout_client->geom.height * config.zoom_end_ratio;
	}

	fadeout_client->animation.time_started = get_now_in_ms();
	wlr_scene_node_set_enabled(&fadeout_client->scene->node, true);
	wl_list_insert(&fadeout_clients, &fadeout_client->fadeout_link);

	request_fresh_for_client(c);
}

/* Promote pending geometry to current and start the animation if one is queued. */
void client_commit(Client *c) {
	c->current = c->pending;

	if (c->animation.should_animate) {
		if (!c->animation.running) {
			c->animation.current = c->animainit_geom;
			/* Fresh start: re-seed the spring from the initial geometry. While
			   already running, vis/vel are kept so a retarget mid-flight bends
			   the path instead of jumping (fully interruptible). */
			c->animation.spring_init = false;
			c->animation.last_tick_ms = 0;
		}

		c->animation.initial = c->animainit_geom;
		c->animation.time_started = get_now_in_ms();

		c->animation.running = true;
		c->animation.should_animate = false;
	}

	request_fresh_for_client(c);
}

/* Decide whether the client should animate to its pending state and then commit it. */
void client_set_pending_state(Client *c) {

	if (!c || c->iskilling)
		return;

	if (!config.animations) {
		c->animation.should_animate = false;
	} else if (config.animations && c->animation.tagining) {
		c->animation.should_animate = true;
	} else if (!config.animations || c == grabc ||
			   (!c->is_pending_open_animation &&
				wlr_box_equal(&c->current, &c->pending))) {
		c->animation.should_animate = false;
	} else {
		c->animation.should_animate = true;
	}

	if (c->animation.action == OPEN &&
	    anim_type_effective(c->animation_type_open,
	                        config.animation_type_open) == ANIM_TYPE_NONE) {
		c->animation.duration = 0;
	}

	if (c->istagswitching) {
		c->animation.duration = 0;
		c->istagswitching = 0;
	}

	if (start_drag_window) {
		c->animation.should_animate = false;
		c->animation.duration = 0;
	}

	if (c->isnoanimation) {
		c->animation.should_animate = false;
		c->animation.duration = 0;
	}

	client_commit(c);
	c->dirty = true;
}

/* Apply a new geometry to a client, picking the correct animation kind and duration. */
void resize(Client *c, struct wlr_box geo, int32_t interact) {

	if (!c || !c->mon || !client_surface(c)->mapped)
		return;

	struct wlr_box *bbox;
	struct wlr_box clip;

	if (!c->mon)
		return;

	c->need_output_flush = true;
	c->dirty = true;

	bbox = (interact || c->isfloating || c->isfullscreen) ? &sgeom : &c->mon->w;

	if (is_scroller_layout(c->mon) && (!c->isfloating || c == grabc)) {
		c->geom = geo;
		c->geom.width = MAX(1 + 2 * (int32_t)c->bw, c->geom.width);
		c->geom.height = MAX(1 + 2 * (int32_t)c->bw, c->geom.height);
	} else {
		c->geom = geo;
		applybounds(
			c,
			bbox);
	}

	if (!c->isnosizehint && !c->ismaximizescreen && !c->isfullscreen &&
		c->isfloating) {
		client_set_size_bound(c);
	}

	if (!c->is_pending_open_animation) {
		c->animation.begin_fade_in = false;
	}

	if (c->animation.action == OPEN && !c->animation.tagining &&
		!c->animation.tagouting && wlr_box_equal(&c->geom, &c->current)) {
		c->animation.action = c->animation.action;
	} else if (c->animation.tagouting) {
		c->animation.duration = config.animation_duration_tag;
		c->animation.action = TAG;
	} else if (c->animation.tagining) {
		c->animation.duration = config.animation_duration_tag;
		c->animation.action = TAG;
	} else if (c->is_pending_open_animation) {
		c->animation.duration = config.animation_duration_open;
		c->animation.action = OPEN;
	} else {
		c->animation.duration = config.animation_duration_move;
		c->animation.action = MOVE;
	}

	if (c->animation.tagouting) {
		c->animainit_geom = c->animation.current;
	} else if (c->animation.tagining) {
		c->animainit_geom.height = c->animation.current.height;
		c->animainit_geom.width = c->animation.current.width;
	} else if (c->is_pending_open_animation) {
		set_client_open_animation(c, c->geom);
	} else {
		c->animainit_geom = c->animation.current;
	}

	if (c->isnoborder || c->iskilling) {
		c->bw = 0;
	}

	bool hit_no_border = check_hit_no_border(c);
	if (hit_no_border && config.smartgaps) {
		c->bw = 0;
		c->fake_no_border = true;
	}

	c->configure_serial = client_set_size(c, c->geom.width - 2 * c->bw,
										  c->geom.height - 2 * c->bw);

	if (c->configure_serial != 0) {
		c->mon->resizing_count_pending++;
	}

	if (c == grabc) {
		c->animation.running = false;
		c->need_output_flush = false;

		c->animainit_geom = c->current = c->pending = c->animation.current =
			c->geom;
		wlr_scene_node_set_position(&c->scene->node, c->geom.x, c->geom.y);

		apply_border(c);
		client_get_clip(c, &clip);
		wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip);
		return;
	}

	if (!c->animation.tagouting && !c->iskilling) {
		c->pending = c->geom;
	}

	if (c->swallowedby && c->animation.action == OPEN) {
		c->animainit_geom = c->swallowedby->animation.current;
	}

	if (c->swallowing) {
		c->animainit_geom = c->geom;
	}

	if ((c->isglobal || c->isunglobal) && c->isfloating &&
		c->animation.action == TAG) {
		c->animainit_geom = c->geom;
	}

	if (c->scratchpad_switching_mon && c->isfloating) {
		c->animainit_geom = c->geom;
	}

	client_set_pending_state(c);

	setborder_color(c);
}

/* Drive one fade-out animation tick for a snapshot client; returns true while animating. */
LEMON_HOT bool client_draw_fadeout_frame(Client *c) {
	if (LEMON_UNLIKELY(!c))
		return false;

	fadeout_client_animation_next_tick(c);
	return true;
}

/* Start the focus-gained animation that transitions opacity and border color towards focused state. */
void client_set_focused_opacity_animation(Client *c) {
	float *border_color = get_border_color(c);
	wlr_scene_node_lower_to_bottom(&c->border->node);

	if (!config.animations) {
		setborder_color(c);
		return;
	}

	c->opacity_animation.duration = config.animation_duration_focus;
	memcpy(c->opacity_animation.target_border_color, border_color,
		   sizeof(c->opacity_animation.target_border_color));
	c->opacity_animation.target_opacity = c->focused_opacity;
	c->opacity_animation.time_started = get_now_in_ms();
	memcpy(c->opacity_animation.initial_border_color,
		   c->opacity_animation.current_border_color,
		   sizeof(c->opacity_animation.initial_border_color));
	c->opacity_animation.initial_opacity = c->opacity_animation.current_opacity;

	c->opacity_animation.running = true;
	c->focus_opacity_dirty = true;
}

/* Start the focus-lost animation that transitions opacity and border color towards unfocused state. */
void client_set_unfocused_opacity_animation(Client *c) {
	float *border_color = get_border_color(c);
	wlr_scene_node_raise_to_top(&c->border->node);
	if (!config.animations) {
		setborder_color(c);
		return;
	}

	c->opacity_animation.duration = config.animation_duration_focus;
	memcpy(c->opacity_animation.target_border_color, border_color,
		   sizeof(c->opacity_animation.target_border_color));

	c->opacity_animation.target_opacity = c->unfocused_opacity;
	c->opacity_animation.time_started = get_now_in_ms();

	memcpy(c->opacity_animation.initial_border_color,
		   c->opacity_animation.current_border_color,
		   sizeof(c->opacity_animation.initial_border_color));
	c->opacity_animation.initial_opacity = c->opacity_animation.current_opacity;

	c->opacity_animation.running = true;
	c->focus_opacity_dirty = true;
}

/* Advance focus/open opacity and border-color interpolation; returns true if a redraw is needed. */
LEMON_HOT bool client_apply_focus_opacity(Client *c) {

	float *border_color = get_border_color(c);

	if (LEMON_LIKELY(!c->opacity_animation.running &&
	    !(c->animation.running && c->animation.action == OPEN) &&
	    !c->isfullscreen && !c->focus_opacity_dirty)) {
		float target = (c == selmon->sel) ? c->focused_opacity : c->unfocused_opacity;
		if (c->opacity_animation.current_opacity == target &&
		    memcmp(c->opacity_animation.current_border_color, border_color,
		           sizeof(c->opacity_animation.current_border_color)) == 0) {
			return false;
		}
	}

	if (c->isfullscreen) {
		c->opacity_animation.running = false;
		client_set_opacity(c, 1);
		c->focus_opacity_dirty = false;
	} else if (c->animation.running && c->animation.action == OPEN) {
		c->opacity_animation.running = false;

		int32_t passed_time = frame_now_ms() - c->animation.time_started;
		double linear_progress =
			c->animation.duration
				? (double)passed_time / (double)c->animation.duration
				: 1.0;

		double opacity_eased_progress =
			find_animation_curve_at(linear_progress, OPAFADEIN);

		float percent = config.animation_fade_in && !c->nofadein
							? opacity_eased_progress
							: 1.0;
		float opacity =
			c == selmon->sel ? c->focused_opacity : c->unfocused_opacity;

		float target_opacity = percent * (1.0 - config.fadein_begin_opacity) +
							   config.fadein_begin_opacity;
		if (target_opacity > opacity) {
			target_opacity = opacity;
		}
		memcpy(c->opacity_animation.current_border_color,
			   c->opacity_animation.target_border_color,
			   sizeof(c->opacity_animation.current_border_color));
		c->opacity_animation.current_opacity = target_opacity;
		client_set_opacity(c, target_opacity);
		client_set_border_color(c, c->opacity_animation.target_border_color);
	} else if (config.animations && c->opacity_animation.running) {

		int32_t passed_time =
			frame_now_ms() - c->opacity_animation.time_started;
		double linear_progress =
			c->opacity_animation.duration
				? (double)passed_time / (double)c->opacity_animation.duration
				: 1.0;

		float eased_progress = find_animation_curve_at(linear_progress, FOCUS);

		c->opacity_animation.current_opacity =
			c->opacity_animation.initial_opacity +
			(c->opacity_animation.target_opacity -
			 c->opacity_animation.initial_opacity) *
				eased_progress;
		client_set_opacity(c, c->opacity_animation.current_opacity);

		for (int32_t i = 0; i < 4; i++) {
			c->opacity_animation.current_border_color[i] =
				c->opacity_animation.initial_border_color[i] +
				(c->opacity_animation.target_border_color[i] -
				 c->opacity_animation.initial_border_color[i]) *
					eased_progress;
		}
		client_set_border_color(c, c->opacity_animation.current_border_color);
		if (linear_progress >= 1.0f) {
			c->opacity_animation.running = false;
		} else {
			return true;
		}
	} else if (c == selmon->sel) {
		c->opacity_animation.running = false;
		c->opacity_animation.current_opacity = c->focused_opacity;
		memcpy(c->opacity_animation.current_border_color, border_color,
			   sizeof(c->opacity_animation.current_border_color));
		client_set_opacity(c, c->focused_opacity);
		c->focus_opacity_dirty = false;
	} else {
		c->opacity_animation.running = false;
		c->opacity_animation.current_opacity = c->unfocused_opacity;
		memcpy(c->opacity_animation.current_border_color, border_color,
			   sizeof(c->opacity_animation.current_border_color));
		client_set_opacity(c, c->unfocused_opacity);
		c->focus_opacity_dirty = false;
	}

	return false;
}

/* Run one render-frame tick for the client: advance geometry and opacity animations as needed. */
LEMON_HOT bool client_draw_frame(Client *c) {

	if (LEMON_UNLIKELY(!c || !client_surface(c)->mapped))
		return false;

	if (LEMON_LIKELY(!c->need_output_flush)) {
		return client_apply_focus_opacity(c);
	}

	/* Architecture brief §9: throttle by render tier. HIDDEN never ticks;
	   OCCLUDED caps at ~30 Hz; FOCUS/VISIBLE tick every frame.

	   IMPORTANT: tag transition animations (tagouting / tagining) flip the
	   client's visibility *via* the animation itself (set_arrange_hidden
	   relies on the tick to slide off-screen and then disable the scene).
	   If we short-circuit here we leave tagouting=true forever and the
	   scene node never gets disabled, which is what made windows appear
	   to follow the user across workspaces. The opening animation has the
	   same property (no_output_flush + animation.running). */
	bool in_tag_transition = c->animation.tagouting || c->animation.tagining ||
	                         c->animation.tagouted;
	uint32_t now_ms = frame_now_ms();
	if (c->render_tier == TIER_HIDDEN && !in_tag_transition &&
	    !c->animation.running) {
		c->animation.current = c->geom;
		wlr_scene_node_set_position(&c->scene->node, c->geom.x, c->geom.y);
		c->need_output_flush = false;
		return false;
	}
	if (c->render_tier == TIER_OCCLUDED && !in_tag_transition &&
	    !c->animation.running &&
	    now_ms - c->tier_last_anim_ms < TIER_OCCLUDED_INTERVAL_MS) {
		return false;
	}
	c->tier_last_anim_ms = now_ms;

	if (config.animations && c->animation.running) {
		client_animation_next_tick(c);
	} else {
		wlr_scene_node_set_position(&c->scene->node, c->pending.x,
									c->pending.y);
		c->animation.current = c->animainit_geom = c->animation.initial =
			c->pending = c->current = c->geom;
		client_apply_clip(c, 1.0);
		c->need_output_flush = false;
	}
	client_apply_focus_opacity(c);
	return true;
}
