/*
 * Wayland window host for Cmpl Vulkan present loop.
 */
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <stdio.h>
#include <string.h>
#include <wayland-client.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>
#include "wayland/xdg-shell-client-protocol.h"
#include "cmpl_vk_present.h"

#define CHECK(x) do { VkResult _r = (x); if (_r != VK_SUCCESS) { \
    fprintf(stderr, "VK error %d at %s:%d\n", (int)_r, __FILE__, __LINE__); \
    return (int)_r ? (int)_r : 1; } } while (0)

typedef struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct xdg_wm_base *wm_base;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *toplevel;
    int configured;
    int running;
    int width;
    int height;
} WlState;

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = { .ping = xdg_wm_base_ping };

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    WlState *wl = (WlState *)data;
    xdg_surface_ack_configure(xdg_surface, serial);
    wl->configured = 1;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void toplevel_configure(void *data, struct xdg_toplevel *toplevel,
                               int32_t width, int32_t height, struct wl_array *states) {
    (void)toplevel;
    (void)states;
    WlState *wl = (WlState *)data;
    if (width > 0) wl->width = width;
    if (height > 0) wl->height = height;
}

static void toplevel_close(void *data, struct xdg_toplevel *toplevel) {
    (void)toplevel;
    ((WlState *)data)->running = 0;
}

static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = toplevel_configure,
    .close = toplevel_close,
};

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface, uint32_t version) {
    WlState *wl = (WlState *)data;
    (void)version;
    if (strcmp(interface, "wl_compositor") == 0) {
        wl->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, "xdg_wm_base") == 0) {
        wl->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(wl->wm_base, &wm_base_listener, wl);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static void wl_poll(void *ctx) {
    WlState *wl = (WlState *)ctx;
    while (wl_display_prepare_read(wl->display) != 0)
        wl_display_dispatch_pending(wl->display);
    wl_display_flush(wl->display);
    wl_display_read_events(wl->display);
    wl_display_dispatch_pending(wl->display);
}

int cmpl_vk_window_wayland(void) {
    WlState wl = {0};
    wl.width = 800;
    wl.height = 600;
    wl.running = 1;

    wl.display = wl_display_connect(NULL);
    if (!wl.display) {
        fprintf(stderr, "cmpl_vk_wl: no Wayland display (set WAYLAND_DISPLAY)\n");
        return 10;
    }

    wl.registry = wl_display_get_registry(wl.display);
    wl_registry_add_listener(wl.registry, &registry_listener, &wl);
    wl_display_roundtrip(wl.display);

    if (!wl.compositor || !wl.wm_base) {
        fprintf(stderr, "cmpl_vk_wl: missing compositor or xdg_wm_base\n");
        wl_display_disconnect(wl.display);
        return 11;
    }

    wl.surface = wl_compositor_create_surface(wl.compositor);
    wl.xdg_surface = xdg_wm_base_get_xdg_surface(wl.wm_base, wl.surface);
    xdg_surface_add_listener(wl.xdg_surface, &xdg_surface_listener, &wl);
    wl.toplevel = xdg_surface_get_toplevel(wl.xdg_surface);
    xdg_toplevel_add_listener(wl.toplevel, &toplevel_listener, &wl);
    xdg_toplevel_set_title(wl.toplevel, "Cmpl Vulkan");
    wl_surface_commit(wl.surface);

    while (!wl.configured)
        wl_display_dispatch(wl.display);

    const char *inst_exts[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
    };
    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "CmplVulkanWL",
        .apiVersion = VK_API_VERSION_1_0,
    };
    VkInstanceCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app,
        .enabledExtensionCount = 2,
        .ppEnabledExtensionNames = inst_exts,
    };
    VkInstance instance;
    CHECK(vkCreateInstance(&ici, NULL, &instance));

    VkWaylandSurfaceCreateInfoKHR sci = {
        .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .display = wl.display,
        .surface = wl.surface,
    };
    VkSurfaceKHR surface;
    CHECK(vkCreateWaylandSurfaceKHR(instance, &sci, NULL, &surface));

    CmplVkWindowHost host = {
        .instance = instance,
        .surface = surface,
        .width = (uint32_t)wl.width,
        .height = (uint32_t)wl.height,
        .running = &wl.running,
        .poll_events = wl_poll,
        .poll_ctx = &wl,
        .log_tag = "cmpl_vk_wl",
    };
    int rc = cmpl_vk_run_swapchain(&host);

    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);
    xdg_toplevel_destroy(wl.toplevel);
    xdg_surface_destroy(wl.xdg_surface);
    wl_surface_destroy(wl.surface);
    xdg_wm_base_destroy(wl.wm_base);
    wl_compositor_destroy(wl.compositor);
    wl_registry_destroy(wl.registry);
    wl_display_disconnect(wl.display);
    return rc;
}
