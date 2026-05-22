#include <wlr/types/wlr_content_type_v1.h>

struct wlr_content_type_manager_v1 *content_type_mgr;

/* Cache the client's current content-type hint on c->content_type. Cheap, run
   on surface commit so the tearing/animation paths read a plain field. */
static void client_refresh_content_type(Client *c) {
	if (!c || !content_type_mgr)
		return;
	struct wlr_surface *s = client_surface(c);
	c->content_type = s ? (int32_t)wlr_surface_get_content_type_v1(
							  content_type_mgr, s)
						: WP_CONTENT_TYPE_V1_TYPE_NONE;
}
