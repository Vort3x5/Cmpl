/*
 * X11/XCB window host for Cmpl Vulkan present loop (bspwm, i3, etc.).
 */
#define VK_USE_PLATFORM_XCB_KHR
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>
#include "cmpl_vk_present.h"

#define CHECK(x) do { VkResult _r = (x); if (_r != VK_SUCCESS) { \
    fprintf(stderr, "VK error %d at %s:%d\n", (int)_r, __FILE__, __LINE__); \
    return (int)_r ? (int)_r : 1; } } while (0)

typedef struct {
    xcb_connection_t *conn;
    xcb_window_t window;
    xcb_atom_t wm_delete;
    int running;
    uint32_t width;
    uint32_t height;
} XcbState;

static void xcb_poll(void *ctx) {
    XcbState *x = (XcbState *)ctx;
    xcb_generic_event_t *ev;
    while ((ev = xcb_poll_for_event(x->conn)) != NULL) {
        uint8_t type = ev->response_type & ~0x80;
        if (type == XCB_CLIENT_MESSAGE) {
            xcb_client_message_event_t *cm = (xcb_client_message_event_t *)ev;
            if (cm->data.data32[0] == x->wm_delete)
                x->running = 0;
        } else if (type == XCB_CONFIGURE_NOTIFY) {
            xcb_configure_notify_event_t *cn = (xcb_configure_notify_event_t *)ev;
            if (cn->width > 0) x->width = cn->width;
            if (cn->height > 0) x->height = cn->height;
        }
        free(ev);
    }
}

int cmpl_vk_window_xcb(void) {
    XcbState x = {0};
    x.width = 800;
    x.height = 600;
    x.running = 1;

    int screen_num = 0;
    x.conn = xcb_connect(NULL, &screen_num);
    if (xcb_connection_has_error(x.conn)) {
        fprintf(stderr, "cmpl_vk_xcb: cannot connect to X display (set DISPLAY)\n");
        if (x.conn) xcb_disconnect(x.conn);
        return 10;
    }

    const xcb_setup_t *setup = xcb_get_setup(x.conn);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screen_num; i++)
        xcb_screen_next(&it);
    xcb_screen_t *screen = it.data;

    x.window = xcb_generate_id(x.conn);
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[] = {
        screen->black_pixel,
        XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_KEY_PRESS,
    };
    xcb_create_window(x.conn, XCB_COPY_FROM_PARENT, x.window, screen->root,
                      0, 0, (uint16_t)x.width, (uint16_t)x.height, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
                      mask, values);

    xcb_intern_atom_cookie_t proto_c =
        xcb_intern_atom(x.conn, 1, 12, "WM_PROTOCOLS");
    xcb_intern_atom_cookie_t del_c =
        xcb_intern_atom(x.conn, 0, 16, "WM_DELETE_WINDOW");
    xcb_intern_atom_reply_t *proto_r = xcb_intern_atom_reply(x.conn, proto_c, NULL);
    xcb_intern_atom_reply_t *del_r = xcb_intern_atom_reply(x.conn, del_c, NULL);
    if (proto_r && del_r) {
        x.wm_delete = del_r->atom;
        xcb_change_property(x.conn, XCB_PROP_MODE_REPLACE, x.window,
                            proto_r->atom, XCB_ATOM_ATOM, 32, 1, &del_r->atom);
    }
    free(proto_r);
    free(del_r);

    const char *title = "Cmpl Vulkan";
    xcb_change_property(x.conn, XCB_PROP_MODE_REPLACE, x.window,
                        XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                        (uint32_t)strlen(title), title);

    xcb_map_window(x.conn, x.window);
    xcb_flush(x.conn);

    const char *inst_exts[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
    };
    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "CmplVulkanXCB",
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

    VkXcbSurfaceCreateInfoKHR sci = {
        .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .connection = x.conn,
        .window = x.window,
    };
    VkSurfaceKHR surface;
    CHECK(vkCreateXcbSurfaceKHR(instance, &sci, NULL, &surface));

    CmplVkWindowHost host = {
        .instance = instance,
        .surface = surface,
        .width = x.width,
        .height = x.height,
        .running = &x.running,
        .poll_events = xcb_poll,
        .poll_ctx = &x,
        .log_tag = "cmpl_vk_xcb",
    };
    int rc = cmpl_vk_run_swapchain(&host);

    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);
    xcb_destroy_window(x.conn, x.window);
    xcb_disconnect(x.conn);
    return rc;
}
