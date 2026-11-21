#pragma once
#include <vulkan/vulkan.h>
#include <stdint.h>

typedef struct CmplVkWindowHost {
    VkInstance instance;
    VkSurfaceKHR surface;
    uint32_t width;
    uint32_t height;
    int *running;
    void (*poll_events)(void *ctx);
    void *poll_ctx;
    const char *log_tag;
} CmplVkWindowHost;

int cmpl_vk_frame_limit(void);
/* Present red triangle until *running==0 or frame limit. Does not destroy instance/surface. */
int cmpl_vk_run_swapchain(CmplVkWindowHost *host);

int cmpl_vk_window_wayland(void);
int cmpl_vk_window_xcb(void);
