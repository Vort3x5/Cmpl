/*
 * Wayland + Vulkan swapchain red triangle for Cmpl.
 * Opens a window, presents until close or CMPL_VK_FRAMES (default 180).
 * Returns 0 on success; 10 if no Wayland display.
 *
 * Build: ./nob all   (or ./nob vk)
 */
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <wayland-client.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>
#include "wayland/xdg-shell-client-protocol.h"

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

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

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

static uint32_t *load_spv(const char *path, size_t *out_words) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint32_t *buf = (uint32_t *)malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *out_words = (size_t)sz / 4;
    return buf;
}

static int frame_limit(void) {
    const char *e = getenv("CMPL_VK_FRAMES");
    if (!e || !e[0]) return 180;
    int n = atoi(e);
    return n < 0 ? 0 : n; /* 0 = until window close */
}

/* Exported — called from Jai via #foreign */
int cmpl_vk_window(void) {
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

    uint32_t pdev_count = 0;
    CHECK(vkEnumeratePhysicalDevices(instance, &pdev_count, NULL));
    if (pdev_count == 0) { fprintf(stderr, "no GPU\n"); return 2; }
    VkPhysicalDevice *pdevs = (VkPhysicalDevice *)calloc(pdev_count, sizeof(*pdevs));
    CHECK(vkEnumeratePhysicalDevices(instance, &pdev_count, pdevs));

    VkPhysicalDevice pdev = VK_NULL_HANDLE;
    uint32_t qfam = UINT32_MAX;
    for (uint32_t pi = 0; pi < pdev_count && pdev == VK_NULL_HANDLE; pi++) {
        uint32_t qcount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pdevs[pi], &qcount, NULL);
        VkQueueFamilyProperties *qfp = (VkQueueFamilyProperties *)calloc(qcount, sizeof(*qfp));
        vkGetPhysicalDeviceQueueFamilyProperties(pdevs[pi], &qcount, qfp);
        for (uint32_t i = 0; i < qcount; i++) {
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(pdevs[pi], i, surface, &present);
            if ((qfp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
                pdev = pdevs[pi];
                qfam = i;
                break;
            }
        }
        free(qfp);
    }
    free(pdevs);
    if (pdev == VK_NULL_HANDLE) {
        fprintf(stderr, "cmpl_vk_wl: no graphics+present queue\n");
        return 3;
    }

    float qpri = 1.0f;
    VkDeviceQueueCreateInfo qci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = qfam,
        .queueCount = 1,
        .pQueuePriorities = &qpri,
    };
    const char *dev_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &qci,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = dev_exts,
    };
    VkDevice device;
    CHECK(vkCreateDevice(pdev, &dci, NULL, &device));
    VkQueue queue;
    vkGetDeviceQueue(device, qfam, 0, &queue);

    VkSurfaceCapabilitiesKHR caps;
    CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pdev, surface, &caps));
    uint32_t fmt_count = 0;
    CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(pdev, surface, &fmt_count, NULL));
    VkSurfaceFormatKHR *fmts = (VkSurfaceFormatKHR *)calloc(fmt_count, sizeof(*fmts));
    CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(pdev, surface, &fmt_count, fmts));
    VkSurfaceFormatKHR surf_fmt = fmts[0];
    for (uint32_t i = 0; i < fmt_count; i++) {
        if (fmts[i].format == VK_FORMAT_B8G8R8A8_UNORM ||
            fmts[i].format == VK_FORMAT_R8G8B8A8_UNORM) {
            surf_fmt = fmts[i];
            break;
        }
    }
    free(fmts);

    uint32_t w = (uint32_t)wl.width;
    uint32_t h = (uint32_t)wl.height;
    if (caps.currentExtent.width != UINT32_MAX) {
        w = caps.currentExtent.width;
        h = caps.currentExtent.height;
    } else {
        if (w < caps.minImageExtent.width) w = caps.minImageExtent.width;
        if (h < caps.minImageExtent.height) h = caps.minImageExtent.height;
        if (w > caps.maxImageExtent.width) w = caps.maxImageExtent.width;
        if (h > caps.maxImageExtent.height) h = caps.maxImageExtent.height;
    }
    if (w == 0) w = 800;
    if (h == 0) h = 600;

    uint32_t img_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && img_count > caps.maxImageCount)
        img_count = caps.maxImageCount;

    VkSwapchainCreateInfoKHR swci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = img_count,
        .imageFormat = surf_fmt.format,
        .imageColorSpace = surf_fmt.colorSpace,
        .imageExtent = { w, h },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
    };
    VkSwapchainKHR swapchain;
    CHECK(vkCreateSwapchainKHR(device, &swci, NULL, &swapchain));

    CHECK(vkGetSwapchainImagesKHR(device, swapchain, &img_count, NULL));
    VkImage *images = (VkImage *)calloc(img_count, sizeof(*images));
    CHECK(vkGetSwapchainImagesKHR(device, swapchain, &img_count, images));

    VkImageView *views = (VkImageView *)calloc(img_count, sizeof(*views));
    for (uint32_t i = 0; i < img_count; i++) {
        VkImageViewCreateInfo ivci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surf_fmt.format,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };
        CHECK(vkCreateImageView(device, &ivci, NULL, &views[i]));
    }

    VkAttachmentDescription att = {
        .format = surf_fmt.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference color_ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription sub = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
    };
    VkSubpassDependency dep = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };
    VkRenderPassCreateInfo rpci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &att,
        .subpassCount = 1,
        .pSubpasses = &sub,
        .dependencyCount = 1,
        .pDependencies = &dep,
    };
    VkRenderPass rp;
    CHECK(vkCreateRenderPass(device, &rpci, NULL, &rp));

    VkFramebuffer *fbs = (VkFramebuffer *)calloc(img_count, sizeof(*fbs));
    for (uint32_t i = 0; i < img_count; i++) {
        VkFramebufferCreateInfo fbci = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = rp,
            .attachmentCount = 1,
            .pAttachments = &views[i],
            .width = w,
            .height = h,
            .layers = 1,
        };
        CHECK(vkCreateFramebuffer(device, &fbci, NULL, &fbs[i]));
    }

    size_t vert_n = 0, frag_n = 0;
    uint32_t *vert_code = load_spv("examples/vulkan/shaders/tri.vert.spv", &vert_n);
    uint32_t *frag_code = load_spv("examples/vulkan/shaders/tri.frag.spv", &frag_n);
    if (!vert_code || !frag_code) return 4;

    VkShaderModuleCreateInfo smci = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    smci.codeSize = vert_n * 4; smci.pCode = vert_code;
    VkShaderModule vert_mod; CHECK(vkCreateShaderModule(device, &smci, NULL, &vert_mod));
    smci.codeSize = frag_n * 4; smci.pCode = frag_code;
    VkShaderModule frag_mod; CHECK(vkCreateShaderModule(device, &smci, NULL, &frag_mod));
    free(vert_code); free(frag_code);

    VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert_mod, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag_mod, .pName = "main" },
    };
    VkPipelineVertexInputStateCreateInfo vi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo ia = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    VkViewport vp = { 0, 0, (float)w, (float)h, 0, 1 };
    VkRect2D sc = { {0, 0}, {w, h} };
    VkPipelineViewportStateCreateInfo vps = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .pViewports = &vp, .scissorCount = 1, .pScissors = &sc,
    };
    VkPipelineRasterizationStateCreateInfo rs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
    };
    VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineColorBlendAttachmentState blend_att = { .colorWriteMask = 0xF };
    VkPipelineColorBlendStateCreateInfo cb = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_att,
    };
    VkPipelineLayoutCreateInfo plci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    VkPipelineLayout layout;
    CHECK(vkCreatePipelineLayout(device, &plci, NULL, &layout));

    VkGraphicsPipelineCreateInfo gpci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vi,
        .pInputAssemblyState = &ia,
        .pViewportState = &vps,
        .pRasterizationState = &rs,
        .pMultisampleState = &ms,
        .pColorBlendState = &cb,
        .layout = layout,
        .renderPass = rp,
    };
    VkPipeline pipeline;
    CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, NULL, &pipeline));

    VkCommandPoolCreateInfo cpci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = qfam,
    };
    VkCommandPool pool;
    CHECK(vkCreateCommandPool(device, &cpci, NULL, &pool));

    VkCommandBuffer *cmds = (VkCommandBuffer *)calloc(img_count, sizeof(*cmds));
    VkCommandBufferAllocateInfo cbai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = img_count,
    };
    CHECK(vkAllocateCommandBuffers(device, &cbai, cmds));

    for (uint32_t i = 0; i < img_count; i++) {
        VkCommandBufferBeginInfo begin = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        CHECK(vkBeginCommandBuffer(cmds[i], &begin));
        VkClearValue clear = { .color = {{0.1f, 0.1f, 0.1f, 1.0f}} };
        VkRenderPassBeginInfo rpbi = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = rp,
            .framebuffer = fbs[i],
            .renderArea = { {0, 0}, {w, h} },
            .clearValueCount = 1,
            .pClearValues = &clear,
        };
        vkCmdBeginRenderPass(cmds[i], &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmds[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdDraw(cmds[i], 3, 1, 0, 0);
        vkCmdEndRenderPass(cmds[i]);
        CHECK(vkEndCommandBuffer(cmds[i]));
    }

    VkSemaphore image_avail, render_done;
    VkSemaphoreCreateInfo semci = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    CHECK(vkCreateSemaphore(device, &semci, NULL, &image_avail));
    CHECK(vkCreateSemaphore(device, &semci, NULL, &render_done));
    VkFence fence;
    VkFenceCreateInfo fci = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    CHECK(vkCreateFence(device, &fci, NULL, &fence));

    int max_frames = frame_limit();
    int frame = 0;
    fprintf(stderr, "cmpl_vk_wl: window %ux%u, frames=%d (0=until close)\n", w, h, max_frames);

    while (wl.running) {
        while (wl_display_prepare_read(wl.display) != 0)
            wl_display_dispatch_pending(wl.display);
        wl_display_flush(wl.display);
        wl_display_read_events(wl.display);
        wl_display_dispatch_pending(wl.display);

        CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
        CHECK(vkResetFences(device, 1, &fence));

        uint32_t idx = 0;
        VkResult acq = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                             image_avail, VK_NULL_HANDLE, &idx);
        if (acq == VK_ERROR_OUT_OF_DATE_KHR) break;
        if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) CHECK(acq);

        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &image_avail,
            .pWaitDstStageMask = &wait_stage,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmds[idx],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &render_done,
        };
        CHECK(vkQueueSubmit(queue, 1, &submit, fence));

        VkPresentInfoKHR present = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &render_done,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &idx,
        };
        VkResult pr = vkQueuePresentKHR(queue, &present);
        if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) {
            /* keep going until frame limit / close for this milestone */
        } else {
            CHECK(pr);
        }

        frame++;
        if (max_frames > 0 && frame >= max_frames)
            wl.running = 0;
    }

    vkDeviceWaitIdle(device);
    fprintf(stderr, "cmpl_vk_wl: presented %d frames\n", frame);

    vkDestroyFence(device, fence, NULL);
    vkDestroySemaphore(device, render_done, NULL);
    vkDestroySemaphore(device, image_avail, NULL);
    vkDestroyCommandPool(device, pool, NULL);
    vkDestroyPipeline(device, pipeline, NULL);
    vkDestroyPipelineLayout(device, layout, NULL);
    vkDestroyShaderModule(device, frag_mod, NULL);
    vkDestroyShaderModule(device, vert_mod, NULL);
    for (uint32_t i = 0; i < img_count; i++) {
        vkDestroyFramebuffer(device, fbs[i], NULL);
        vkDestroyImageView(device, views[i], NULL);
    }
    free(cmds); free(fbs); free(views); free(images);
    vkDestroyRenderPass(device, rp, NULL);
    vkDestroySwapchainKHR(device, swapchain, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);

    xdg_toplevel_destroy(wl.toplevel);
    xdg_surface_destroy(wl.xdg_surface);
    wl_surface_destroy(wl.surface);
    xdg_wm_base_destroy(wl.wm_base);
    wl_compositor_destroy(wl.compositor);
    wl_registry_destroy(wl.registry);
    wl_display_disconnect(wl.display);
    return 0;
}
