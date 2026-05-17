#include "wlr_ext_workspace_v1.h"
#include "ext-workspace-v1-protocol.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_output.h>

#define EXT_WORKSPACE_V1_VERSION 1

struct wlr_ext_workspace_v1_group_output {
	struct wlr_output *output;
	struct wlr_ext_workspace_group_handle_v1 *group;
	struct wl_listener output_bind;
	struct wl_listener output_destroy;
	struct wl_list link;
};

struct wlr_ext_workspace_manager_v1_resource {
	struct wl_resource *resource;
	struct wlr_ext_workspace_manager_v1 *manager;
	struct wl_list requests;
	struct wl_list workspace_resources;
	struct wl_list group_resources;
	struct wl_list link;
};

struct wlr_ext_workspace_group_v1_resource {
	struct wl_resource *resource;
	struct wlr_ext_workspace_group_handle_v1 *group;
	struct wlr_ext_workspace_manager_v1_resource *manager;
	struct wl_list link;
	struct wl_list
		manager_resource_link;
};

struct wlr_ext_workspace_v1_resource {
	struct wl_resource *resource;
	struct wlr_ext_workspace_handle_v1 *workspace;
	struct wlr_ext_workspace_manager_v1_resource *manager;
	struct wl_list link;
	struct wl_list
		manager_resource_link;
};

static const struct ext_workspace_group_handle_v1_interface group_impl;

/* Type-checked accessor: get the group resource wrapper from a wl_resource. */
static struct wlr_ext_workspace_group_v1_resource *
group_resource_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(
		resource, &ext_workspace_group_handle_v1_interface, &group_impl));
	return wl_resource_get_user_data(resource);
}

static const struct ext_workspace_handle_v1_interface workspace_impl;

/* Type-checked accessor: get the workspace resource wrapper from a wl_resource. */
static struct wlr_ext_workspace_v1_resource *
workspace_resource_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &ext_workspace_handle_v1_interface,
								   &workspace_impl));
	return wl_resource_get_user_data(resource);
}

static const struct ext_workspace_manager_v1_interface manager_impl;

/* Type-checked accessor: get the manager resource wrapper from a wl_resource. */
static struct wlr_ext_workspace_manager_v1_resource *
manager_resource_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(
		resource, &ext_workspace_manager_v1_interface, &manager_impl));
	return wl_resource_get_user_data(resource);
}

/* Protocol request: client destroys its workspace handle resource. */
static void workspace_handle_destroy(struct wl_client *client,
									 struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

/* Protocol request: queue an activate request for this workspace, applied on next commit. */
static void workspace_handle_activate(struct wl_client *client,
									  struct wl_resource *workspace_resource) {
	struct wlr_ext_workspace_v1_resource *workspace_res =
		workspace_resource_from_resource(workspace_resource);
	if (!workspace_res) {
		return;
	}

	struct wlr_ext_workspace_v1_request *req = calloc(1, sizeof(*req));
	if (!req) {
		wl_resource_post_no_memory(workspace_resource);
		return;
	}
	req->type = WLR_EXT_WORKSPACE_V1_REQUEST_ACTIVATE;
	req->activate.workspace = workspace_res->workspace;
	wl_list_insert(workspace_res->manager->requests.prev, &req->link);
}

/* Protocol request: queue a deactivate request for this workspace. */
static void
workspace_handle_deactivate(struct wl_client *client,
							struct wl_resource *workspace_resource) {
	struct wlr_ext_workspace_v1_resource *workspace_res =
		workspace_resource_from_resource(workspace_resource);
	if (!workspace_res) {
		return;
	}

	struct wlr_ext_workspace_v1_request *req = calloc(1, sizeof(*req));
	if (!req) {
		wl_resource_post_no_memory(workspace_resource);
		return;
	}
	req->type = WLR_EXT_WORKSPACE_V1_REQUEST_DEACTIVATE;
	req->deactivate.workspace = workspace_res->workspace;
	wl_list_insert(workspace_res->manager->requests.prev, &req->link);
}

/* Protocol request: queue a request to assign this workspace to a different group. */
static void workspace_handle_assign(struct wl_client *client,
									struct wl_resource *workspace_resource,
									struct wl_resource *group_resource) {
	struct wlr_ext_workspace_v1_resource *workspace_res =
		workspace_resource_from_resource(workspace_resource);
	struct wlr_ext_workspace_group_v1_resource *group_res =
		group_resource_from_resource(group_resource);
	if (!workspace_res || !group_res) {
		return;
	}

	struct wlr_ext_workspace_v1_request *req = calloc(1, sizeof(*req));
	if (!req) {
		wl_resource_post_no_memory(workspace_resource);
		return;
	}
	req->type = WLR_EXT_WORKSPACE_V1_REQUEST_ASSIGN;
	req->assign.group = group_res->group;
	req->assign.workspace = workspace_res->workspace;
	wl_list_insert(workspace_res->manager->requests.prev, &req->link);
}

/* Protocol request: queue a request to remove this workspace. */
static void workspace_handle_remove(struct wl_client *client,
									struct wl_resource *workspace_resource) {
	struct wlr_ext_workspace_v1_resource *workspace_res =
		workspace_resource_from_resource(workspace_resource);
	if (!workspace_res) {
		return;
	}

	struct wlr_ext_workspace_v1_request *req = calloc(1, sizeof(*req));
	if (!req) {
		wl_resource_post_no_memory(workspace_resource);
		return;
	}
	req->type = WLR_EXT_WORKSPACE_V1_REQUEST_REMOVE;
	req->remove.workspace = workspace_res->workspace;
	wl_list_insert(workspace_res->manager->requests.prev, &req->link);
}

static const struct ext_workspace_handle_v1_interface workspace_impl = {
	.destroy = workspace_handle_destroy,
	.activate = workspace_handle_activate,
	.deactivate = workspace_handle_deactivate,
	.assign = workspace_handle_assign,
	.remove = workspace_handle_remove,
};

/* Protocol request: queue a request to create a new workspace with the given name in this group. */
static void group_handle_create_workspace(struct wl_client *client,
										  struct wl_resource *group_resource,
										  const char *name) {
	struct wlr_ext_workspace_group_v1_resource *group_res =
		group_resource_from_resource(group_resource);
	if (!group_res) {
		return;
	}

	struct wlr_ext_workspace_v1_request *req = calloc(1, sizeof(*req));
	if (!req) {
		wl_resource_post_no_memory(group_resource);
		return;
	}
	req->create_workspace.name = strdup(name);
	if (!req->create_workspace.name) {
		free(req);
		wl_resource_post_no_memory(group_resource);
		return;
	}
	req->type = WLR_EXT_WORKSPACE_V1_REQUEST_CREATE_WORKSPACE;
	req->create_workspace.group = group_res->group;
	wl_list_insert(group_res->manager->requests.prev, &req->link);
}

/* Protocol request: client destroys its group handle resource. */
static void group_handle_destroy(struct wl_client *client,
								 struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct ext_workspace_group_handle_v1_interface group_impl = {
	.create_workspace = group_handle_create_workspace,
	.destroy = group_handle_destroy,
};

/* Unlink a workspace resource wrapper from its lists and free it. */
static void destroy_workspace_resource(
	struct wlr_ext_workspace_v1_resource *workspace_res) {
	wl_list_remove(&workspace_res->link);
	wl_list_remove(&workspace_res->manager_resource_link);
	wl_resource_set_user_data(workspace_res->resource, NULL);
	free(workspace_res);
}

/* wl_resource destructor for workspace handle: free the wrapper if still attached. */
static void workspace_resource_destroy(struct wl_resource *resource) {
	struct wlr_ext_workspace_v1_resource *workspace_res =
		workspace_resource_from_resource(resource);
	if (workspace_res) {
		destroy_workspace_resource(workspace_res);
	}
}

/* Create a workspace handle resource for one client/manager binding and wire it into lists. */
static struct wlr_ext_workspace_v1_resource *create_workspace_resource(
	struct wlr_ext_workspace_handle_v1 *workspace,
	struct wlr_ext_workspace_manager_v1_resource *manager_res) {
	struct wlr_ext_workspace_v1_resource *workspace_res =
		calloc(1, sizeof(*workspace_res));
	if (!workspace_res) {
		return NULL;
	}

	struct wl_client *client = wl_resource_get_client(manager_res->resource);
	workspace_res->resource =
		wl_resource_create(client, &ext_workspace_handle_v1_interface,
						   wl_resource_get_version(manager_res->resource), 0);
	if (!workspace_res->resource) {
		free(workspace_res);
		return NULL;
	}
	wl_resource_set_implementation(workspace_res->resource, &workspace_impl,
								   workspace_res, workspace_resource_destroy);

	workspace_res->workspace = workspace;
	workspace_res->manager = manager_res;
	wl_list_insert(&workspace->resources, &workspace_res->link);
	wl_list_insert(&manager_res->workspace_resources,
				   &workspace_res->manager_resource_link);

	return workspace_res;
}

/* Unlink a group resource wrapper from its lists and free it. */
static void
destroy_group_resource(struct wlr_ext_workspace_group_v1_resource *group_res) {
	wl_list_remove(&group_res->link);
	wl_list_remove(&group_res->manager_resource_link);
	wl_resource_set_user_data(group_res->resource, NULL);
	free(group_res);
}

/* wl_resource destructor for group handle: free the wrapper if still attached. */
static void group_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_ext_workspace_group_v1_resource *group_res =
		group_resource_from_resource(resource);
	if (group_res) {
		destroy_group_resource(group_res);
	}
}

/* Create a group handle resource for one client/manager binding and wire it into lists. */
static struct wlr_ext_workspace_group_v1_resource *create_group_resource(
	struct wlr_ext_workspace_group_handle_v1 *group,
	struct wlr_ext_workspace_manager_v1_resource *manager_res) {
	struct wlr_ext_workspace_group_v1_resource *group_res =
		calloc(1, sizeof(*group_res));
	if (!group_res) {
		return NULL;
	}

	struct wl_client *client = wl_resource_get_client(manager_res->resource);
	uint32_t version = wl_resource_get_version(manager_res->resource);
	group_res->resource = wl_resource_create(
		client, &ext_workspace_group_handle_v1_interface, version, 0);
	if (group_res->resource == NULL) {
		free(group_res);
		return NULL;
	}
	wl_resource_set_implementation(group_res->resource, &group_impl, group_res,
								   group_handle_resource_destroy);

	group_res->group = group;
	group_res->manager = manager_res;
	wl_list_insert(&group->resources, &group_res->link);
	wl_list_insert(&manager_res->group_resources,
				   &group_res->manager_resource_link);

	return group_res;
}

/* Drain the pending request queue on a manager resource, freeing each request. */
static void
destroy_requests(struct wlr_ext_workspace_manager_v1_resource *manager_res) {
	struct wlr_ext_workspace_v1_request *req, *tmp;
	wl_list_for_each_safe(req, tmp, &manager_res->requests, link) {
		if (req->type == WLR_EXT_WORKSPACE_V1_REQUEST_CREATE_WORKSPACE) {
			free(req->create_workspace.name);
		}
		wl_list_remove(&req->link);
		free(req);
	}
}

/* Null out references to a destroyed group/workspace in any pending requests. */
static void
clear_requests_by(struct wlr_ext_workspace_manager_v1_resource *manager_res,
				  struct wlr_ext_workspace_group_handle_v1 *group,
				  struct wlr_ext_workspace_handle_v1 *workspace) {
	struct wlr_ext_workspace_v1_request *req, *tmp;
	wl_list_for_each_safe(req, tmp, &manager_res->requests, link) {
		switch (req->type) {
		case WLR_EXT_WORKSPACE_V1_REQUEST_CREATE_WORKSPACE:
			if (group && req->create_workspace.group == group) {
				req->create_workspace.group = NULL;
			}
			break;
		case WLR_EXT_WORKSPACE_V1_REQUEST_ACTIVATE:
			if (workspace && req->activate.workspace == workspace) {
				req->activate.workspace = NULL;
			}
			break;
		case WLR_EXT_WORKSPACE_V1_REQUEST_DEACTIVATE:
			if (workspace && req->deactivate.workspace == workspace) {
				req->deactivate.workspace = NULL;
			}
			break;
		case WLR_EXT_WORKSPACE_V1_REQUEST_ASSIGN:
			if (workspace && req->assign.workspace == workspace) {
				req->assign.workspace = NULL;
			}
			if (group && req->assign.group == group) {
				req->assign.group = NULL;
			}
			break;
		case WLR_EXT_WORKSPACE_V1_REQUEST_REMOVE:
			if (workspace && req->remove.workspace == workspace) {
				req->remove.workspace = NULL;
			}
			break;
		}
	}
}

/* Protocol request: commit — emit the queued requests via the manager's commit signal. */
static void manager_handle_commit(struct wl_client *client,
								  struct wl_resource *resource) {
	struct wlr_ext_workspace_manager_v1_resource *manager_res =
		manager_resource_from_resource(resource);
	if (!manager_res) {
		return;
	}

	struct wlr_ext_workspace_v1_commit_event commit_event = {
		.requests = &manager_res->requests,
	};
	wl_signal_emit_mutable(&manager_res->manager->events.commit, &commit_event);
	destroy_requests(manager_res);
}

/* Idle callback: send a 'done' event to every bound manager resource. */
static void handle_idle(void *data) {
	struct wlr_ext_workspace_manager_v1 *manager = data;

	struct wlr_ext_workspace_manager_v1_resource *manager_res;
	wl_list_for_each(manager_res, &manager->resources, link) {
		ext_workspace_manager_v1_send_done(manager_res->resource);
	}
	manager->idle_source = NULL;
}

/* Arm a single idle source so a 'done' event is sent after the current batch of changes. */
static void
manager_schedule_done(struct wlr_ext_workspace_manager_v1 *manager) {
	if (!manager->idle_source) {
		manager->idle_source =
			wl_event_loop_add_idle(manager->event_loop, handle_idle, manager);
	}
}

/* Send capabilities, coordinates, name, id and state for a workspace to one client resource. */
static void
workspace_send_details(struct wlr_ext_workspace_v1_resource *workspace_res) {
	struct wlr_ext_workspace_handle_v1 *workspace = workspace_res->workspace;
	struct wl_resource *resource = workspace_res->resource;

	ext_workspace_handle_v1_send_capabilities(resource, workspace->caps);
	if (workspace->coordinates.size > 0) {
		ext_workspace_handle_v1_send_coordinates(resource,
												 &workspace->coordinates);
	}
	if (workspace->name) {
		ext_workspace_handle_v1_send_name(resource, workspace->name);
	}
	if (workspace->id) {
		ext_workspace_handle_v1_send_id(resource, workspace->id);
	}
	ext_workspace_handle_v1_send_state(resource, workspace->state);
	manager_schedule_done(workspace->manager);
}

/* Protocol request: stop — send finished and destroy the manager resource. */
static void manager_handle_stop(struct wl_client *client,
								struct wl_resource *resource) {
	ext_workspace_manager_v1_send_finished(resource);
	wl_resource_destroy(resource);
}

static const struct ext_workspace_manager_v1_interface manager_impl = {
	.commit = manager_handle_commit,
	.stop = manager_handle_stop,
};

/* Tear down a manager resource: free pending requests and all child workspace/group resources. */
static void destroy_manager_resource(
	struct wlr_ext_workspace_manager_v1_resource *manager_res) {
	destroy_requests(manager_res);

	struct wlr_ext_workspace_v1_resource *workspace_res, *tmp2;
	wl_list_for_each_safe(workspace_res, tmp2,
						  &manager_res->workspace_resources,
						  manager_resource_link) {
		destroy_workspace_resource(workspace_res);
	}
	struct wlr_ext_workspace_group_v1_resource *group_res, *tmp3;
	wl_list_for_each_safe(group_res, tmp3, &manager_res->group_resources,
						  manager_resource_link) {
		destroy_group_resource(group_res);
	}

	wl_list_remove(&manager_res->link);
	wl_resource_set_user_data(manager_res->resource, NULL);
	free(manager_res);
}

/* wl_resource destructor for the manager: free the wrapper if still attached. */
static void manager_resource_destroy(struct wl_resource *resource) {
	struct wlr_ext_workspace_manager_v1_resource *manager_res =
		manager_resource_from_resource(resource);
	if (manager_res) {
		destroy_manager_resource(manager_res);
	}
}

/* Send capabilities and matching output_enter events for a group to one client resource. */
static void
group_send_details(struct wlr_ext_workspace_group_v1_resource *group_res) {
	struct wlr_ext_workspace_group_handle_v1 *group = group_res->group;
	struct wl_resource *resource = group_res->resource;
	struct wl_client *client = wl_resource_get_client(resource);

	ext_workspace_group_handle_v1_send_capabilities(resource, group->caps);

	struct wlr_ext_workspace_v1_group_output *group_output;
	wl_list_for_each(group_output, &group->outputs, link) {
		struct wl_resource *output_resource;
		wl_resource_for_each(output_resource,
							 &group_output->output->resources) {
			if (wl_resource_get_client(output_resource) == client) {
				ext_workspace_group_handle_v1_send_output_enter(
					resource, output_resource);
			}
		}
	}

	manager_schedule_done(group->manager);
}

/* Global bind: create a manager resource for the client and replay all groups/workspaces. */
static void manager_bind(struct wl_client *client, void *data, uint32_t version,
						 uint32_t id) {
	struct wlr_ext_workspace_manager_v1 *manager = data;

	struct wlr_ext_workspace_manager_v1_resource *manager_res =
		calloc(1, sizeof(*manager_res));
	if (!manager_res) {
		wl_client_post_no_memory(client);
		return;
	}

	manager_res->manager = manager;
	wl_list_init(&manager_res->requests);
	wl_list_init(&manager_res->workspace_resources);
	wl_list_init(&manager_res->group_resources);

	manager_res->resource = wl_resource_create(
		client, &ext_workspace_manager_v1_interface, version, id);
	if (!manager_res->resource) {
		free(manager_res);
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(manager_res->resource, &manager_impl,
								   manager_res, manager_resource_destroy);
	wl_list_insert(&manager->resources, &manager_res->link);

	struct wlr_ext_workspace_group_handle_v1 *group;
	wl_list_for_each(group, &manager->groups, link) {
		struct wlr_ext_workspace_group_v1_resource *group_res =
			create_group_resource(group, manager_res);
		if (!group_res) {
			wl_resource_post_no_memory(manager_res->resource);
			continue;
		}
		ext_workspace_manager_v1_send_workspace_group(manager_res->resource,
													  group_res->resource);
		group_send_details(group_res);
	}

	struct wlr_ext_workspace_handle_v1 *workspace;
	wl_list_for_each(workspace, &manager->workspaces, link) {
		struct wlr_ext_workspace_v1_resource *workspace_res =
			create_workspace_resource(workspace, manager_res);
		if (!workspace_res) {
			wl_resource_post_no_memory(manager_res->resource);
			continue;
		}
		ext_workspace_manager_v1_send_workspace(manager_res->resource,
												workspace_res->resource);
		workspace_send_details(workspace_res);

		if (!workspace->group) {
			continue;
		}
		struct wlr_ext_workspace_group_v1_resource *group_res;
		wl_list_for_each(group_res, &workspace->group->resources, link) {
			if (group_res->manager == manager_res) {
				ext_workspace_group_handle_v1_send_workspace_enter(
					group_res->resource, workspace_res->resource);
			}
		}
	}

	ext_workspace_manager_v1_send_done(manager_res->resource);
}

/* Listener: wl_display destroyed — fire destroy signal and free the manager and all children. */
static void manager_handle_display_destroy(struct wl_listener *listener,
										   void *data) {
	struct wlr_ext_workspace_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);

	wl_signal_emit_mutable(&manager->events.destroy, NULL);
	assert(wl_list_empty(&manager->events.commit.listener_list));
	assert(wl_list_empty(&manager->events.destroy.listener_list));

	struct wlr_ext_workspace_group_handle_v1 *group, *tmp;
	wl_list_for_each_safe(group, tmp, &manager->groups, link) {
		wlr_ext_workspace_group_handle_v1_destroy(group);
	}

	struct wlr_ext_workspace_handle_v1 *workspace, *tmp2;
	wl_list_for_each_safe(workspace, tmp2, &manager->workspaces, link) {
		wlr_ext_workspace_handle_v1_destroy(workspace);
	}

	struct wlr_ext_workspace_manager_v1_resource *manager_res, *tmp3;
	wl_list_for_each_safe(manager_res, tmp3, &manager->resources, link) {
		destroy_manager_resource(manager_res);
	}

	if (manager->idle_source) {
		wl_event_source_remove(manager->idle_source);
	}

	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);
	free(manager);
}

/* Create the ext-workspace-v1 global, init lists/signals and watch display destroy. */
struct wlr_ext_workspace_manager_v1 *
wlr_ext_workspace_manager_v1_create(struct wl_display *display,
									uint32_t version) {
	assert(version <= EXT_WORKSPACE_V1_VERSION);

	struct wlr_ext_workspace_manager_v1 *manager = calloc(1, sizeof(*manager));
	if (!manager) {
		return NULL;
	}

	manager->global =
		wl_global_create(display, &ext_workspace_manager_v1_interface, version,
						 manager, manager_bind);
	if (!manager->global) {
		free(manager);
		return NULL;
	}

	manager->event_loop = wl_display_get_event_loop(display);

	manager->display_destroy.notify = manager_handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	wl_list_init(&manager->groups);
	wl_list_init(&manager->workspaces);
	wl_list_init(&manager->resources);
	wl_signal_init(&manager->events.commit);
	wl_signal_init(&manager->events.destroy);

	return manager;
}

/* Create a new workspace group, advertise it to every bound manager and schedule a done. */
struct wlr_ext_workspace_group_handle_v1 *
wlr_ext_workspace_group_handle_v1_create(
	struct wlr_ext_workspace_manager_v1 *manager, uint32_t caps) {
	struct wlr_ext_workspace_group_handle_v1 *group = calloc(1, sizeof(*group));
	if (!group) {
		return NULL;
	}

	group->manager = manager;
	group->caps = caps;

	wl_list_init(&group->outputs);
	wl_list_init(&group->resources);
	wl_signal_init(&group->events.destroy);

	wl_list_insert(manager->groups.prev, &group->link);

	struct wlr_ext_workspace_manager_v1_resource *manager_res;
	wl_list_for_each(manager_res, &manager->resources, link) {
		struct wlr_ext_workspace_group_v1_resource *group_res =
			create_group_resource(group, manager_res);
		if (!group_res) {
			continue;
		}
		ext_workspace_manager_v1_send_workspace_group(manager_res->resource,
													  group_res->resource);
		group_send_details(group_res);
	}

	manager_schedule_done(manager);

	return group;
}

/* Send workspace_enter/workspace_leave on every matching (workspace, group) resource pair. */
static void
workspace_send_group(struct wlr_ext_workspace_handle_v1 *workspace,
					 struct wlr_ext_workspace_group_handle_v1 *group,
					 bool enter) {

	struct wlr_ext_workspace_v1_resource *workspace_res;
	wl_list_for_each(workspace_res, &workspace->resources, link) {
		struct wlr_ext_workspace_group_v1_resource *group_res;
		wl_list_for_each(group_res, &group->resources, link) {
			if (group_res->manager != workspace_res->manager) {
				continue;
			}
			if (enter) {
				ext_workspace_group_handle_v1_send_workspace_enter(
					group_res->resource, workspace_res->resource);
			} else {
				ext_workspace_group_handle_v1_send_workspace_leave(
					group_res->resource, workspace_res->resource);
			}
		}
	}

	manager_schedule_done(workspace->manager);
}

/* Unlink and free a group/output binding helper, removing its listeners. */
static void
destroy_group_output(struct wlr_ext_workspace_v1_group_output *group_output) {
	wl_list_remove(&group_output->output_bind.link);
	wl_list_remove(&group_output->output_destroy.link);
	wl_list_remove(&group_output->link);
	free(group_output);
}

/* Send output_enter/output_leave for an output on every group resource of matching clients. */
static void group_send_output(struct wlr_ext_workspace_group_handle_v1 *group,
							  struct wlr_output *output, bool enter) {

	struct wlr_ext_workspace_group_v1_resource *group_res;
	wl_list_for_each(group_res, &group->resources, link) {
		struct wl_client *client = wl_resource_get_client(group_res->resource);

		struct wl_resource *output_resource;
		wl_resource_for_each(output_resource, &output->resources) {
			if (wl_resource_get_client(output_resource) != client) {
				continue;
			}
			if (enter) {
				ext_workspace_group_handle_v1_send_output_enter(
					group_res->resource, output_resource);
			} else {
				ext_workspace_group_handle_v1_send_output_leave(
					group_res->resource, output_resource);
			}
		}
	}

	manager_schedule_done(group->manager);
}

/* Destroy a workspace group: detach its workspaces, send removed to clients and free. */
void wlr_ext_workspace_group_handle_v1_destroy(
	struct wlr_ext_workspace_group_handle_v1 *group) {
	if (!group) {
		return;
	}

	wl_signal_emit_mutable(&group->events.destroy, NULL);

	assert(wl_list_empty(&group->events.destroy.listener_list));

	struct wlr_ext_workspace_handle_v1 *workspace;
	wl_list_for_each(workspace, &group->manager->workspaces, link) {
		if (workspace->group == group) {
			workspace_send_group(workspace, group, false);
			workspace->group = NULL;
		}
	}

	struct wlr_ext_workspace_group_v1_resource *group_res, *tmp;
	wl_list_for_each_safe(group_res, tmp, &group->resources, link) {
		ext_workspace_group_handle_v1_send_removed(group_res->resource);
		destroy_group_resource(group_res);
	}

	struct wlr_ext_workspace_manager_v1_resource *manager_res;
	wl_list_for_each(manager_res, &group->manager->resources, link) {
		clear_requests_by(manager_res, group, NULL);
	}

	struct wlr_ext_workspace_v1_group_output *group_output, *tmp3;
	wl_list_for_each_safe(group_output, tmp3, &group->outputs, link) {
		group_send_output(group, group_output->output, false);
		destroy_group_output(group_output);
	}

	manager_schedule_done(group->manager);

	wl_list_remove(&group->link);
	free(group);
}

/* Listener: a client newly bound to an output — send output_enter for any group on that output. */
static void handle_output_bind(struct wl_listener *listener, void *data) {
	struct wlr_ext_workspace_v1_group_output *group_output =
		wl_container_of(listener, group_output, output_bind);
	struct wlr_output_event_bind *event = data;
	struct wl_client *client = wl_resource_get_client(event->resource);

	struct wlr_ext_workspace_group_v1_resource *group_res;
	wl_list_for_each(group_res, &group_output->group->resources, link) {
		if (wl_resource_get_client(group_res->resource) == client) {
			ext_workspace_group_handle_v1_send_output_enter(group_res->resource,
															event->resource);
		}
	}

	manager_schedule_done(group_output->group->manager);
}

/* Listener: output destroyed — emit output_leave and tear down its group binding. */
static void handle_output_destroy(struct wl_listener *listener, void *data) {
	struct wlr_ext_workspace_v1_group_output *group_output =
		wl_container_of(listener, group_output, output_destroy);
	group_send_output(group_output->group, group_output->output, false);
	destroy_group_output(group_output);
}

/* Look up the binding between a group and a given output, or NULL. */
static struct wlr_ext_workspace_v1_group_output *
get_group_output(struct wlr_ext_workspace_group_handle_v1 *group,
				 struct wlr_output *output) {
	struct wlr_ext_workspace_v1_group_output *group_output;
	wl_list_for_each(group_output, &group->outputs, link) {
		if (group_output->output == output) {
			return group_output;
		}
	}
	return NULL;
}

/* Bind a group to an output, hook bind/destroy listeners and broadcast output_enter. */
void wlr_ext_workspace_group_handle_v1_output_enter(
	struct wlr_ext_workspace_group_handle_v1 *group,
	struct wlr_output *output) {
	if (get_group_output(group, output)) {
		return;
	}
	struct wlr_ext_workspace_v1_group_output *group_output =
		calloc(1, sizeof(*group_output));
	if (!group_output) {
		return;
	}
	group_output->output = output;
	group_output->group = group;
	wl_list_insert(&group->outputs, &group_output->link);

	group_output->output_bind.notify = handle_output_bind;
	wl_signal_add(&output->events.bind, &group_output->output_bind);
	group_output->output_destroy.notify = handle_output_destroy;
	wl_signal_add(&output->events.destroy, &group_output->output_destroy);

	group_send_output(group, output, true);
}

/* Remove a group's binding to an output and broadcast output_leave. */
void wlr_ext_workspace_group_handle_v1_output_leave(
	struct wlr_ext_workspace_group_handle_v1 *group,
	struct wlr_output *output) {
	struct wlr_ext_workspace_v1_group_output *group_output =
		get_group_output(group, output);
	if (!group_output) {
		return;
	}

	group_send_output(group, output, false);
	destroy_group_output(group_output);
}

/* Create a new workspace handle, advertise it to every bound manager and schedule a done. */
struct wlr_ext_workspace_handle_v1 *
wlr_ext_workspace_handle_v1_create(struct wlr_ext_workspace_manager_v1 *manager,
								   const char *id, uint32_t caps) {
	struct wlr_ext_workspace_handle_v1 *workspace =
		calloc(1, sizeof(*workspace));
	if (!workspace) {
		return NULL;
	}

	workspace->manager = manager;
	workspace->caps = caps;

	if (id) {
		workspace->id = strdup(id);
		if (!workspace->id) {
			free(workspace);
			return NULL;
		}
	}

	wl_list_init(&workspace->resources);
	wl_array_init(&workspace->coordinates);
	wl_signal_init(&workspace->events.destroy);

	wl_list_insert(manager->workspaces.prev, &workspace->link);

	struct wlr_ext_workspace_manager_v1_resource *manager_res;
	wl_list_for_each(manager_res, &manager->resources, link) {
		struct wlr_ext_workspace_v1_resource *workspace_res =
			create_workspace_resource(workspace, manager_res);
		if (!workspace_res) {
			continue;
		}
		ext_workspace_manager_v1_send_workspace(manager_res->resource,
												workspace_res->resource);
		workspace_send_details(workspace_res);
	}

	manager_schedule_done(manager);

	return workspace;
}

/* Destroy a workspace handle: send removed to clients, drop pending requests and free state. */
void wlr_ext_workspace_handle_v1_destroy(
	struct wlr_ext_workspace_handle_v1 *workspace) {
	if (!workspace) {
		return;
	}

	wl_signal_emit_mutable(&workspace->events.destroy, NULL);

	assert(wl_list_empty(&workspace->events.destroy.listener_list));

	if (workspace->group) {
		workspace_send_group(workspace, workspace->group, false);
	}

	struct wlr_ext_workspace_v1_resource *workspace_res, *tmp;
	wl_list_for_each_safe(workspace_res, tmp, &workspace->resources, link) {
		ext_workspace_handle_v1_send_removed(workspace_res->resource);
		destroy_workspace_resource(workspace_res);
	}

	struct wlr_ext_workspace_manager_v1_resource *manager_res;
	wl_list_for_each(manager_res, &workspace->manager->resources, link) {
		clear_requests_by(manager_res, NULL, workspace);
	}

	manager_schedule_done(workspace->manager);

	wl_list_remove(&workspace->link);
	wl_array_release(&workspace->coordinates);
	free(workspace->id);
	free(workspace->name);
	free(workspace);
}

/* Reassign a workspace to a different group, sending leave/enter events as needed. */
void wlr_ext_workspace_handle_v1_set_group(
	struct wlr_ext_workspace_handle_v1 *workspace,
	struct wlr_ext_workspace_group_handle_v1 *group) {
	if (workspace->group == group) {
		return;
	}

	if (workspace->group) {
		workspace_send_group(workspace, workspace->group, false);
	}
	workspace->group = group;
	if (group) {
		workspace_send_group(workspace, group, true);
	}
}

/* Update a workspace's display name and broadcast the new name to subscribers. */
void wlr_ext_workspace_handle_v1_set_name(
	struct wlr_ext_workspace_handle_v1 *workspace, const char *name) {
	assert(name);

	if (workspace->name && strcmp(workspace->name, name) == 0) {
		return;
	}

	free(workspace->name);
	workspace->name = strdup(name);
	if (workspace->name == NULL) {
		return;
	}

	struct wlr_ext_workspace_v1_resource *workspace_res;
	wl_list_for_each(workspace_res, &workspace->resources, link) {
		ext_workspace_handle_v1_send_name(workspace_res->resource,
										  workspace->name);
	}

	manager_schedule_done(workspace->manager);
}

/* Update a workspace's coordinate array and broadcast the new coordinates if they changed. */
void wlr_ext_workspace_handle_v1_set_coordinates(
	struct wlr_ext_workspace_handle_v1 *workspace, const uint32_t *coords,
	size_t coords_len) {
	size_t size = coords_len * sizeof(coords[0]);
	if (size == workspace->coordinates.size &&
		(size == 0 || memcmp(workspace->coordinates.data, coords, size) == 0)) {
		return;
	}

	wl_array_release(&workspace->coordinates);
	wl_array_init(&workspace->coordinates);
	struct wl_array arr = {
		.data = (void *)coords,
		.size = size,
	};
	wl_array_copy(&workspace->coordinates, &arr);

	struct wlr_ext_workspace_v1_resource *workspace_res;
	wl_list_for_each(workspace_res, &workspace->resources, link) {
		ext_workspace_handle_v1_send_coordinates(workspace_res->resource,
												 &workspace->coordinates);
	}

	manager_schedule_done(workspace->manager);
}

/* Set or clear a state bit on a workspace and broadcast the new state on change. */
static void workspace_set_state(struct wlr_ext_workspace_handle_v1 *workspace,
								enum ext_workspace_handle_v1_state state,
								bool enabled) {
	uint32_t old_state = workspace->state;
	if (enabled) {
		workspace->state |= state;
	} else {
		workspace->state &= ~state;
	}
	if (old_state == workspace->state) {
		return;
	}

	struct wlr_ext_workspace_v1_resource *workspace_res;
	wl_list_for_each(workspace_res, &workspace->resources, link) {
		ext_workspace_handle_v1_send_state(workspace_res->resource,
										   workspace->state);
	}

	manager_schedule_done(workspace->manager);
}

/* Toggle the ACTIVE state bit on a workspace. */
void wlr_ext_workspace_handle_v1_set_active(
	struct wlr_ext_workspace_handle_v1 *workspace, bool enabled) {
	workspace_set_state(workspace, EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE,
						enabled);
}

/* Toggle the URGENT state bit on a workspace. */
void wlr_ext_workspace_handle_v1_set_urgent(
	struct wlr_ext_workspace_handle_v1 *workspace, bool enabled) {
	workspace_set_state(workspace, EXT_WORKSPACE_HANDLE_V1_STATE_URGENT,
						enabled);
}

/* Toggle the HIDDEN state bit on a workspace. */
void wlr_ext_workspace_handle_v1_set_hidden(
	struct wlr_ext_workspace_handle_v1 *workspace, bool enabled) {
	workspace_set_state(workspace, EXT_WORKSPACE_HANDLE_V1_STATE_HIDDEN,
						enabled);
}
