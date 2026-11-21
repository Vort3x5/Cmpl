#pragma once
#include <vulkan/vulkan.h>
#include <stdint.h>

typedef enum CmplVkDrawMode {
    CMPL_VK_DRAW_TRIANGLE = 0,
    CMPL_VK_DRAW_SPRITE = 1,
} CmplVkDrawMode;

typedef struct CmplVkWindowHost {
    VkInstance instance;
    VkSurfaceKHR surface;
    uint32_t width;
    uint32_t height;
    int *running;
    void (*poll_events)(void *ctx);
    void *poll_ctx;
    const char *log_tag;
    CmplVkDrawMode draw;
} CmplVkWindowHost;

int cmpl_vk_frame_limit(void);
/* Present until *running==0 or frame limit. Does not destroy instance/surface. */
int cmpl_vk_run_swapchain(CmplVkWindowHost *host);

int cmpl_vk_window_wayland(CmplVkDrawMode draw);
int cmpl_vk_window_xcb(CmplVkDrawMode draw);
