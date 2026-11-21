/*
 * Thin game session for Cmpl: window + textured sprite present.
 * Position / input decisions live in Jai; this only open/poll/key/draw/close.
 *
 * Key codes for cmpl_vk_game_key:
 *   0=left  1=right  2=up  3=down  (arrows + WASD on typical evdev/X11)
 */
#define VK_USE_PLATFORM_XCB_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <xcb/xcb.h>
#include <wayland-client.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>
#include <vulkan/vulkan_wayland.h>
#include "wayland/xdg-shell-client-protocol.h"
#include "cmpl_vk_present.h"

#define CHECK(x) do { VkResult _r = (x); if (_r != VK_SUCCESS) { \
    fprintf(stderr, "VK error %d at %s:%d\n", (int)_r, __FILE__, __LINE__); \
    return (int)_r ? (int)_r : 1; } } while (0)

#define TEX_W 64
#define TEX_H 64
#define KEY_LEFT  0
#define KEY_RIGHT 1
#define KEY_UP    2
#define KEY_DOWN  3

typedef enum { PLAT_NONE, PLAT_XCB, PLAT_WL } PlatKind;

typedef struct {
    PlatKind kind;
    int running;
    int keys[4];
    uint32_t width, height;
    int frame;
    int max_frames;

    /* XCB */
    xcb_connection_t *xcb;
    xcb_window_t xwin;
    xcb_atom_t wm_delete;

    /* Wayland */
    struct wl_display *wdisplay;
    struct wl_registry *wregistry;
    struct wl_compositor *wcompositor;
    struct xdg_wm_base *wm_base;
    struct wl_surface *wsurface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *toplevel;
    int configured;

    /* Vulkan */
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice pdev;
    VkDevice device;
    VkQueue queue;
    uint32_t qfam;
    VkSwapchainKHR swapchain;
    uint32_t img_count;
    VkImage *images;
    VkImageView *views;
    VkFramebuffer *fbs;
    VkFormat swap_fmt;
    VkRenderPass rp;
    VkPipelineLayout layout;
    VkPipeline pipeline;
    VkCommandPool pool;
    VkCommandBuffer *cmds;
    VkSemaphore image_avail, render_done;
    VkFence fence;
    VkBuffer vbo;
    VkDeviceMemory vbo_mem;
    VkImage tex_img;
    VkDeviceMemory tex_mem;
    VkImageView tex_view;
    VkSampler sampler;
    VkDescriptorSetLayout dsl;
    VkDescriptorPool dp;
    VkDescriptorSet ds;
    VkShaderModule vert_mod, frag_mod;
} Game;

static Game G;

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

static uint32_t find_mem_type(VkPhysicalDevice pd, uint32_t bits, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(pd, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & flags) == flags)
            return i;
    return UINT32_MAX;
}

static void fill_sprite_rgba(uint8_t *px) {
    for (int y = 0; y < TEX_H; y++) {
        for (int x = 0; x < TEX_W; x++) {
            uint8_t *p = px + (size_t)(y * TEX_W + x) * 4;
            int cx = x - TEX_W / 2;
            int cy = y - TEX_H / 2;
            int r2 = cx * cx + cy * cy;
            if (r2 > (28 * 28)) {
                p[0] = p[1] = p[2] = p[3] = 0;
                continue;
            }
            p[0] = 40; p[1] = 120; p[2] = 70; p[3] = 255;
            if (cy < -2 && cx * cx + (cy + 8) * (cy + 8) < 10 * 10) {
                p[0] = 220; p[1] = 190; p[2] = 150;
            }
            if ((cx + 5) * (cx + 5) + (cy + 10) * (cy + 10) < 9) {
                p[0] = p[1] = p[2] = 20;
            }
            if ((cx - 5) * (cx - 5) + (cy + 10) * (cy + 10) < 9) {
                p[0] = p[1] = p[2] = 20;
            }
            if (cy > 4 && cy < 10 && cx * cx < 18 * 18) {
                p[0] = 180; p[1] = 140; p[2] = 40;
            }
        }
    }
}

static void set_key_by_code(int code, int down) {
    /* Linux evdev / X11 common keycodes */
    if (code == 113 || code == 38) G.keys[KEY_LEFT] = down;   /* Left / A */
    if (code == 114 || code == 40) G.keys[KEY_RIGHT] = down;  /* Right / D */
    if (code == 111 || code == 25) G.keys[KEY_UP] = down;     /* Up / W */
    if (code == 116 || code == 39) G.keys[KEY_DOWN] = down;   /* Down / S */
}

/* ---- Wayland listeners (minimal) ---- */
static void xdg_wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(wm_base, serial);
}
static const struct xdg_wm_base_listener wm_base_listener = { .ping = xdg_wm_base_ping };

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    (void)data;
    xdg_surface_ack_configure(xdg_surface, serial);
    G.configured = 1;
}
static const struct xdg_surface_listener xdg_surface_listener = { .configure = xdg_surface_configure };

static void toplevel_configure(void *data, struct xdg_toplevel *toplevel,
                               int32_t width, int32_t height, struct wl_array *states) {
    (void)data; (void)toplevel; (void)states;
    if (width > 0) G.width = (uint32_t)width;
    if (height > 0) G.height = (uint32_t)height;
}
static void toplevel_close(void *data, struct xdg_toplevel *toplevel) {
    (void)data; (void)toplevel;
    G.running = 0;
}
static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = toplevel_configure, .close = toplevel_close,
};

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface, uint32_t version) {
    (void)data; (void)version;
    if (strcmp(interface, "wl_compositor") == 0)
        G.wcompositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    else if (strcmp(interface, "xdg_wm_base") == 0) {
        G.wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(G.wm_base, &wm_base_listener, NULL);
    }
}
static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data; (void)registry; (void)name;
}
static const struct wl_registry_listener registry_listener = {
    .global = registry_global, .global_remove = registry_global_remove,
};

static int create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                         VkMemoryPropertyFlags mem_flags,
                         VkBuffer *out_buf, VkDeviceMemory *out_mem) {
    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size, .usage = usage,
    };
    CHECK(vkCreateBuffer(G.device, &bci, NULL, out_buf));
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(G.device, *out_buf, &mr);
    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mr.size,
        .memoryTypeIndex = find_mem_type(G.pdev, mr.memoryTypeBits, mem_flags),
    };
    CHECK(vkAllocateMemory(G.device, &mai, NULL, out_mem));
    CHECK(vkBindBufferMemory(G.device, *out_buf, *out_mem, 0));
    return 0;
}

static void update_vbo(int px, int py) {
    float cx = (G.width > 0) ? ((float)px / (float)(G.width / 2)) : 0.f;
    float cy = (G.height > 0) ? (-(float)py / (float)(G.height / 2)) : 0.f;
    float hs = 0.28f;
    float verts[] = {
        cx - hs, cy - hs, 0.f, 1.f,
        cx + hs, cy - hs, 1.f, 1.f,
        cx + hs, cy + hs, 1.f, 0.f,
        cx - hs, cy - hs, 0.f, 1.f,
        cx + hs, cy + hs, 1.f, 0.f,
        cx - hs, cy + hs, 0.f, 0.f,
    };
    void *map = NULL;
    vkMapMemory(G.device, G.vbo_mem, 0, sizeof(verts), 0, &map);
    memcpy(map, verts, sizeof(verts));
    vkUnmapMemory(G.device, G.vbo_mem);
}

static int vk_init_after_surface(void) {
    uint32_t pdev_count = 0;
    CHECK(vkEnumeratePhysicalDevices(G.instance, &pdev_count, NULL));
    if (!pdev_count) return 2;
    VkPhysicalDevice *pdevs = calloc(pdev_count, sizeof(*pdevs));
    CHECK(vkEnumeratePhysicalDevices(G.instance, &pdev_count, pdevs));
    G.pdev = VK_NULL_HANDLE;
    G.qfam = UINT32_MAX;
    for (uint32_t pi = 0; pi < pdev_count && G.pdev == VK_NULL_HANDLE; pi++) {
        uint32_t qcount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pdevs[pi], &qcount, NULL);
        VkQueueFamilyProperties *qfp = calloc(qcount, sizeof(*qfp));
        vkGetPhysicalDeviceQueueFamilyProperties(pdevs[pi], &qcount, qfp);
        for (uint32_t i = 0; i < qcount; i++) {
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(pdevs[pi], i, G.surface, &present);
            if ((qfp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
                G.pdev = pdevs[pi];
                G.qfam = i;
                break;
            }
        }
        free(qfp);
    }
    free(pdevs);
    if (G.pdev == VK_NULL_HANDLE) return 3;

    float qpri = 1.0f;
    VkDeviceQueueCreateInfo qci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = G.qfam, .queueCount = 1, .pQueuePriorities = &qpri,
    };
    const char *dev_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci,
        .enabledExtensionCount = 1, .ppEnabledExtensionNames = dev_exts,
    };
    CHECK(vkCreateDevice(G.pdev, &dci, NULL, &G.device));
    vkGetDeviceQueue(G.device, G.qfam, 0, &G.queue);

    VkSurfaceCapabilitiesKHR caps;
    CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(G.pdev, G.surface, &caps));
    uint32_t fmt_count = 0;
    CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(G.pdev, G.surface, &fmt_count, NULL));
    VkSurfaceFormatKHR *fmts = calloc(fmt_count, sizeof(*fmts));
    CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(G.pdev, G.surface, &fmt_count, fmts));
    VkSurfaceFormatKHR surf_fmt = fmts[0];
    for (uint32_t i = 0; i < fmt_count; i++)
        if (fmts[i].format == VK_FORMAT_B8G8R8A8_UNORM || fmts[i].format == VK_FORMAT_R8G8B8A8_UNORM) {
            surf_fmt = fmts[i]; break;
        }
    free(fmts);
    G.swap_fmt = surf_fmt.format;

    if (caps.currentExtent.width != UINT32_MAX) {
        G.width = caps.currentExtent.width;
        G.height = caps.currentExtent.height;
    }
    if (G.width == 0) G.width = 800;
    if (G.height == 0) G.height = 600;

    G.img_count = caps.minImageCount + 1;
    if (caps.maxImageCount && G.img_count > caps.maxImageCount)
        G.img_count = caps.maxImageCount;

    VkSwapchainCreateInfoKHR swci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = G.surface,
        .minImageCount = G.img_count,
        .imageFormat = surf_fmt.format,
        .imageColorSpace = surf_fmt.colorSpace,
        .imageExtent = { G.width, G.height },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
    };
    CHECK(vkCreateSwapchainKHR(G.device, &swci, NULL, &G.swapchain));
    CHECK(vkGetSwapchainImagesKHR(G.device, G.swapchain, &G.img_count, NULL));
    G.images = calloc(G.img_count, sizeof(*G.images));
    CHECK(vkGetSwapchainImagesKHR(G.device, G.swapchain, &G.img_count, G.images));
    G.views = calloc(G.img_count, sizeof(*G.views));
    for (uint32_t i = 0; i < G.img_count; i++) {
        VkImageViewCreateInfo ivci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = G.images[i], .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = G.swap_fmt,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };
        CHECK(vkCreateImageView(G.device, &ivci, NULL, &G.views[i]));
    }

    VkAttachmentDescription att = {
        .format = G.swap_fmt, .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference color_ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription sub = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1, .pColorAttachments = &color_ref,
    };
    VkSubpassDependency dep = {
        .srcSubpass = VK_SUBPASS_EXTERNAL, .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };
    VkRenderPassCreateInfo rpci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1, .pAttachments = &att,
        .subpassCount = 1, .pSubpasses = &sub,
        .dependencyCount = 1, .pDependencies = &dep,
    };
    CHECK(vkCreateRenderPass(G.device, &rpci, NULL, &G.rp));

    G.fbs = calloc(G.img_count, sizeof(*G.fbs));
    for (uint32_t i = 0; i < G.img_count; i++) {
        VkFramebufferCreateInfo fbci = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = G.rp, .attachmentCount = 1, .pAttachments = &G.views[i],
            .width = G.width, .height = G.height, .layers = 1,
        };
        CHECK(vkCreateFramebuffer(G.device, &fbci, NULL, &G.fbs[i]));
    }

    /* texture + sampler + descriptors (same as sprite path) */
    VkImageCreateInfo imgci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D, .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = { TEX_W, TEX_H, 1 }, .mipLevels = 1, .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    CHECK(vkCreateImage(G.device, &imgci, NULL, &G.tex_img));
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(G.device, G.tex_img, &mr);
    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mr.size,
        .memoryTypeIndex = find_mem_type(G.pdev, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    CHECK(vkAllocateMemory(G.device, &mai, NULL, &G.tex_mem));
    CHECK(vkBindImageMemory(G.device, G.tex_img, G.tex_mem, 0));

    VkBuffer staging; VkDeviceMemory staging_mem;
    VkDeviceSize tex_bytes = TEX_W * TEX_H * 4;
    if (create_buffer(tex_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &staging, &staging_mem))
        return 1;
    uint8_t *mapped = NULL;
    CHECK(vkMapMemory(G.device, staging_mem, 0, tex_bytes, 0, (void **)&mapped));
    fill_sprite_rgba(mapped);
    vkUnmapMemory(G.device, staging_mem);

    VkCommandPoolCreateInfo cpci0 = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = G.qfam,
    };
    VkCommandPool upload_pool;
    CHECK(vkCreateCommandPool(G.device, &cpci0, NULL, &upload_pool));
    VkCommandBufferAllocateInfo cbai0 = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = upload_pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer upcmd;
    CHECK(vkAllocateCommandBuffers(G.device, &cbai0, &upcmd));
    VkCommandBufferBeginInfo begin0 = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    CHECK(vkBeginCommandBuffer(upcmd, &begin0));
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = G.tex_img,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
    };
    vkCmdPipelineBarrier(upcmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, NULL, 0, NULL, 1, &barrier);
    VkBufferImageCopy copy = {
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageExtent = { TEX_W, TEX_H, 1 },
    };
    vkCmdCopyBufferToImage(upcmd, staging, G.tex_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(upcmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, NULL, 0, NULL, 1, &barrier);
    CHECK(vkEndCommandBuffer(upcmd));
    VkSubmitInfo upsubmit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &upcmd,
    };
    CHECK(vkQueueSubmit(G.queue, 1, &upsubmit, VK_NULL_HANDLE));
    CHECK(vkQueueWaitIdle(G.queue));
    vkDestroyCommandPool(G.device, upload_pool, NULL);
    vkDestroyBuffer(G.device, staging, NULL);
    vkFreeMemory(G.device, staging_mem, NULL);

    VkImageViewCreateInfo tiv = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = G.tex_img, .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
    };
    CHECK(vkCreateImageView(G.device, &tiv, NULL, &G.tex_view));
    VkSamplerCreateInfo sci = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST, .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    };
    CHECK(vkCreateSampler(G.device, &sci, NULL, &G.sampler));

    VkDescriptorSetLayoutBinding dslb = {
        .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    VkDescriptorSetLayoutCreateInfo dslci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1, .pBindings = &dslb,
    };
    CHECK(vkCreateDescriptorSetLayout(G.device, &dslci, NULL, &G.dsl));
    VkDescriptorPoolSize dps = { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1 };
    VkDescriptorPoolCreateInfo dpci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &dps,
    };
    CHECK(vkCreateDescriptorPool(G.device, &dpci, NULL, &G.dp));
    VkDescriptorSetAllocateInfo dsai = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = G.dp, .descriptorSetCount = 1, .pSetLayouts = &G.dsl,
    };
    CHECK(vkAllocateDescriptorSets(G.device, &dsai, &G.ds));
    VkDescriptorImageInfo dii = {
        .sampler = G.sampler, .imageView = G.tex_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet wds = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = G.ds, .dstBinding = 0, .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &dii,
    };
    vkUpdateDescriptorSets(G.device, 1, &wds, 0, NULL);

    if (create_buffer(6 * 16, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &G.vbo, &G.vbo_mem))
        return 1;
    update_vbo(0, 0);

    size_t vert_n = 0, frag_n = 0;
    uint32_t *vert_code = load_spv("examples/vulkan/shaders/sprite.vert.spv", &vert_n);
    uint32_t *frag_code = load_spv("examples/vulkan/shaders/sprite.frag.spv", &frag_n);
    if (!vert_code || !frag_code) return 4;
    VkShaderModuleCreateInfo smci = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    smci.codeSize = vert_n * 4; smci.pCode = vert_code;
    CHECK(vkCreateShaderModule(G.device, &smci, NULL, &G.vert_mod));
    smci.codeSize = frag_n * 4; smci.pCode = frag_code;
    CHECK(vkCreateShaderModule(G.device, &smci, NULL, &G.frag_mod));
    free(vert_code); free(frag_code);

    VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &G.dsl,
    };
    CHECK(vkCreatePipelineLayout(G.device, &plci, NULL, &G.layout));

    VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = G.vert_mod, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = G.frag_mod, .pName = "main" },
    };
    VkVertexInputBindingDescription bind = { .binding = 0, .stride = 16, .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attrs[2] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0 },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 8 },
    };
    VkPipelineVertexInputStateCreateInfo vi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &bind,
        .vertexAttributeDescriptionCount = 2, .pVertexAttributeDescriptions = attrs,
    };
    VkPipelineInputAssemblyStateCreateInfo ia = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    VkViewport vp = { 0, 0, (float)G.width, (float)G.height, 0, 1 };
    VkRect2D sc = { {0, 0}, {G.width, G.height} };
    VkPipelineViewportStateCreateInfo vps = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .pViewports = &vp, .scissorCount = 1, .pScissors = &sc,
    };
    VkPipelineRasterizationStateCreateInfo rs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, .lineWidth = 1.0f,
    };
    VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineColorBlendAttachmentState blend_att = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = 0xF,
    };
    VkPipelineColorBlendStateCreateInfo cb = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1, .pAttachments = &blend_att,
    };
    VkGraphicsPipelineCreateInfo gpci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2, .pStages = stages,
        .pVertexInputState = &vi, .pInputAssemblyState = &ia,
        .pViewportState = &vps, .pRasterizationState = &rs,
        .pMultisampleState = &ms, .pColorBlendState = &cb,
        .layout = G.layout, .renderPass = G.rp,
    };
    CHECK(vkCreateGraphicsPipelines(G.device, VK_NULL_HANDLE, 1, &gpci, NULL, &G.pipeline));

    VkCommandPoolCreateInfo cpci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = G.qfam,
    };
    CHECK(vkCreateCommandPool(G.device, &cpci, NULL, &G.pool));
    G.cmds = calloc(G.img_count, sizeof(*G.cmds));
    VkCommandBufferAllocateInfo cbai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = G.pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = G.img_count,
    };
    CHECK(vkAllocateCommandBuffers(G.device, &cbai, G.cmds));

    VkSemaphoreCreateInfo semci = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    CHECK(vkCreateSemaphore(G.device, &semci, NULL, &G.image_avail));
    CHECK(vkCreateSemaphore(G.device, &semci, NULL, &G.render_done));
    VkFenceCreateInfo fci = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    CHECK(vkCreateFence(G.device, &fci, NULL, &G.fence));
    return 0;
}

static int open_xcb(void) {
    int screen_num = 0;
    G.xcb = xcb_connect(NULL, &screen_num);
    if (xcb_connection_has_error(G.xcb)) {
        fprintf(stderr, "cmpl_vk_game: no X display\n");
        return 10;
    }
    const xcb_setup_t *setup = xcb_get_setup(G.xcb);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screen_num; i++) xcb_screen_next(&it);
    xcb_screen_t *screen = it.data;
    G.width = 800; G.height = 600;
    G.xwin = xcb_generate_id(G.xcb);
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[] = {
        screen->black_pixel,
        XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE,
    };
    xcb_create_window(G.xcb, XCB_COPY_FROM_PARENT, G.xwin, screen->root,
                      0, 0, 800, 600, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual, mask, values);
    xcb_intern_atom_cookie_t proto_c = xcb_intern_atom(G.xcb, 1, 12, "WM_PROTOCOLS");
    xcb_intern_atom_cookie_t del_c = xcb_intern_atom(G.xcb, 0, 16, "WM_DELETE_WINDOW");
    xcb_intern_atom_reply_t *proto_r = xcb_intern_atom_reply(G.xcb, proto_c, NULL);
    xcb_intern_atom_reply_t *del_r = xcb_intern_atom_reply(G.xcb, del_c, NULL);
    if (proto_r && del_r) {
        G.wm_delete = del_r->atom;
        xcb_change_property(G.xcb, XCB_PROP_MODE_REPLACE, G.xwin,
                            proto_r->atom, XCB_ATOM_ATOM, 32, 1, &del_r->atom);
    }
    free(proto_r); free(del_r);
    const char *title = "Cmpl Player";
    xcb_change_property(G.xcb, XCB_PROP_MODE_REPLACE, G.xwin, XCB_ATOM_WM_NAME,
                        XCB_ATOM_STRING, 8, (uint32_t)strlen(title), title);
    xcb_map_window(G.xcb, G.xwin);
    xcb_flush(G.xcb);

    const char *inst_exts[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME };
    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "CmplPlayer", .apiVersion = VK_API_VERSION_1_0,
    };
    VkInstanceCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app, .enabledExtensionCount = 2, .ppEnabledExtensionNames = inst_exts,
    };
    CHECK(vkCreateInstance(&ici, NULL, &G.instance));
    VkXcbSurfaceCreateInfoKHR sci = {
        .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .connection = G.xcb, .window = G.xwin,
    };
    CHECK(vkCreateXcbSurfaceKHR(G.instance, &sci, NULL, &G.surface));
    G.kind = PLAT_XCB;
    return vk_init_after_surface();
}

static int open_wayland(void) {
    G.wdisplay = wl_display_connect(NULL);
    if (!G.wdisplay) return 10;
    G.wregistry = wl_display_get_registry(G.wdisplay);
    wl_registry_add_listener(G.wregistry, &registry_listener, NULL);
    wl_display_roundtrip(G.wdisplay);
    if (!G.wcompositor || !G.wm_base) {
        wl_display_disconnect(G.wdisplay);
        return 11;
    }
    G.width = 800; G.height = 600;
    G.wsurface = wl_compositor_create_surface(G.wcompositor);
    G.xdg_surface = xdg_wm_base_get_xdg_surface(G.wm_base, G.wsurface);
    xdg_surface_add_listener(G.xdg_surface, &xdg_surface_listener, NULL);
    G.toplevel = xdg_surface_get_toplevel(G.xdg_surface);
    xdg_toplevel_add_listener(G.toplevel, &toplevel_listener, NULL);
    xdg_toplevel_set_title(G.toplevel, "Cmpl Player");
    wl_surface_commit(G.wsurface);
    while (!G.configured) wl_display_dispatch(G.wdisplay);

    const char *inst_exts[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME };
    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "CmplPlayer", .apiVersion = VK_API_VERSION_1_0,
    };
    VkInstanceCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app, .enabledExtensionCount = 2, .ppEnabledExtensionNames = inst_exts,
    };
    CHECK(vkCreateInstance(&ici, NULL, &G.instance));
    VkWaylandSurfaceCreateInfoKHR sci = {
        .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .display = G.wdisplay, .surface = G.wsurface,
    };
    CHECK(vkCreateWaylandSurfaceKHR(G.instance, &sci, NULL, &G.surface));
    G.kind = PLAT_WL;
    return vk_init_after_surface();
}

int cmpl_vk_game_open(void) {
    memset(&G, 0, sizeof(G));
    G.running = 1;
    G.max_frames = cmpl_vk_frame_limit();
    const char *wd = getenv("WAYLAND_DISPLAY");
    const char *xd = getenv("DISPLAY");
    int rc;
    if (wd && wd[0]) rc = open_wayland();
    else if (xd && xd[0]) rc = open_xcb();
    else {
        rc = open_wayland();
        if (rc == 10) rc = open_xcb();
    }
    if (rc != 0) {
        fprintf(stderr, "cmpl_vk_game: open failed (%d)\n", rc);
        return rc;
    }
    fprintf(stderr, "cmpl_vk_game: open %ux%u frames=%d (WASD/arrows move; Jai owns position)\n",
            G.width, G.height, G.max_frames);
    return 0;
}

int cmpl_vk_game_poll(void) {
    if (!G.running) return 0;
    if (G.kind == PLAT_XCB) {
        xcb_generic_event_t *ev;
        while ((ev = xcb_poll_for_event(G.xcb)) != NULL) {
            uint8_t type = ev->response_type & ~0x80;
            if (type == XCB_CLIENT_MESSAGE) {
                xcb_client_message_event_t *cm = (xcb_client_message_event_t *)ev;
                if (cm->data.data32[0] == G.wm_delete) G.running = 0;
            } else if (type == XCB_KEY_PRESS) {
                xcb_key_press_event_t *kp = (xcb_key_press_event_t *)ev;
                set_key_by_code(kp->detail, 1);
            } else if (type == XCB_KEY_RELEASE) {
                xcb_key_release_event_t *kr = (xcb_key_release_event_t *)ev;
                set_key_by_code(kr->detail, 0);
            } else if (type == XCB_CONFIGURE_NOTIFY) {
                xcb_configure_notify_event_t *cn = (xcb_configure_notify_event_t *)ev;
                if (cn->width > 0) G.width = cn->width;
                if (cn->height > 0) G.height = cn->height;
            }
            free(ev);
        }
    } else if (G.kind == PLAT_WL) {
        while (wl_display_prepare_read(G.wdisplay) != 0)
            wl_display_dispatch_pending(G.wdisplay);
        wl_display_flush(G.wdisplay);
        wl_display_read_events(G.wdisplay);
        wl_display_dispatch_pending(G.wdisplay);
    }
    return G.running ? 1 : 0;
}

int cmpl_vk_game_key(int k) {
    if (k < 0 || k > 3) return 0;
    return G.keys[k] ? 1 : 0;
}

int cmpl_vk_game_draw(int x, int y) {
    if (!G.running) return 0;
    update_vbo(x, y);

    CHECK(vkWaitForFences(G.device, 1, &G.fence, VK_TRUE, UINT64_MAX));
    CHECK(vkResetFences(G.device, 1, &G.fence));

    uint32_t idx = 0;
    VkResult acq = vkAcquireNextImageKHR(G.device, G.swapchain, UINT64_MAX,
                                         G.image_avail, VK_NULL_HANDLE, &idx);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) { G.running = 0; return 0; }
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) CHECK(acq);

    VkCommandBuffer cmd = G.cmds[idx];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    CHECK(vkBeginCommandBuffer(cmd, &begin));
    VkClearValue clear = { .color = {{0.08f, 0.09f, 0.12f, 1.0f}} };
    VkRenderPassBeginInfo rpbi = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = G.rp, .framebuffer = G.fbs[idx],
        .renderArea = { {0, 0}, {G.width, G.height} },
        .clearValueCount = 1, .pClearValues = &clear,
    };
    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, G.pipeline);
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &G.vbo, &off);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, G.layout, 0, 1, &G.ds, 0, NULL);
    vkCmdDraw(cmd, 6, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
    CHECK(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &G.image_avail,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1, .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1, .pSignalSemaphores = &G.render_done,
    };
    CHECK(vkQueueSubmit(G.queue, 1, &submit, G.fence));
    VkPresentInfoKHR present = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &G.render_done,
        .swapchainCount = 1, .pSwapchains = &G.swapchain, .pImageIndices = &idx,
    };
    VkResult pr = vkQueuePresentKHR(G.queue, &present);
    if (pr != VK_ERROR_OUT_OF_DATE_KHR && pr != VK_SUBOPTIMAL_KHR && pr != VK_SUCCESS)
        CHECK(pr);

    G.frame++;
    if (G.max_frames > 0 && G.frame >= G.max_frames)
        G.running = 0;
    return G.running ? 1 : 0;
}

int cmpl_vk_game_close(void) {
    if (G.device) {
        vkDeviceWaitIdle(G.device);
        fprintf(stderr, "cmpl_vk_game: closed after %d frames\n", G.frame);
        vkDestroyFence(G.device, G.fence, NULL);
        vkDestroySemaphore(G.device, G.render_done, NULL);
        vkDestroySemaphore(G.device, G.image_avail, NULL);
        vkDestroyCommandPool(G.device, G.pool, NULL);
        vkDestroyPipeline(G.device, G.pipeline, NULL);
        vkDestroyPipelineLayout(G.device, G.layout, NULL);
        vkDestroyShaderModule(G.device, G.frag_mod, NULL);
        vkDestroyShaderModule(G.device, G.vert_mod, NULL);
        vkDestroyDescriptorPool(G.device, G.dp, NULL);
        vkDestroyDescriptorSetLayout(G.device, G.dsl, NULL);
        vkDestroySampler(G.device, G.sampler, NULL);
        vkDestroyImageView(G.device, G.tex_view, NULL);
        vkDestroyImage(G.device, G.tex_img, NULL);
        vkFreeMemory(G.device, G.tex_mem, NULL);
        vkDestroyBuffer(G.device, G.vbo, NULL);
        vkFreeMemory(G.device, G.vbo_mem, NULL);
        for (uint32_t i = 0; i < G.img_count; i++) {
            vkDestroyFramebuffer(G.device, G.fbs[i], NULL);
            vkDestroyImageView(G.device, G.views[i], NULL);
        }
        free(G.cmds); free(G.fbs); free(G.views); free(G.images);
        vkDestroyRenderPass(G.device, G.rp, NULL);
        vkDestroySwapchainKHR(G.device, G.swapchain, NULL);
        vkDestroyDevice(G.device, NULL);
    }
    if (G.instance) {
        vkDestroySurfaceKHR(G.instance, G.surface, NULL);
        vkDestroyInstance(G.instance, NULL);
    }
    if (G.kind == PLAT_XCB && G.xcb) {
        xcb_destroy_window(G.xcb, G.xwin);
        xcb_disconnect(G.xcb);
    }
    if (G.kind == PLAT_WL && G.wdisplay) {
        xdg_toplevel_destroy(G.toplevel);
        xdg_surface_destroy(G.xdg_surface);
        wl_surface_destroy(G.wsurface);
        xdg_wm_base_destroy(G.wm_base);
        wl_compositor_destroy(G.wcompositor);
        wl_registry_destroy(G.wregistry);
        wl_display_disconnect(G.wdisplay);
    }
    memset(&G, 0, sizeof(G));
    return 0;
}
