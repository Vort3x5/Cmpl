#include "cmpl_vk_present.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { VkResult _r = (x); if (_r != VK_SUCCESS) { \
    fprintf(stderr, "VK error %d at %s:%d\n", (int)_r, __FILE__, __LINE__); \
    return (int)_r ? (int)_r : 1; } } while (0)

int cmpl_vk_frame_limit(void) {
    const char *e = getenv("CMPL_VK_FRAMES");
    if (!e || !e[0]) return 180;
    int n = atoi(e);
    return n < 0 ? 0 : n;
}

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

int cmpl_vk_run_swapchain(CmplVkWindowHost *host) {
    VkInstance instance = host->instance;
    VkSurfaceKHR surface = host->surface;
    const char *tag = host->log_tag ? host->log_tag : "cmpl_vk";

    uint32_t pdev_count = 0;
    CHECK(vkEnumeratePhysicalDevices(instance, &pdev_count, NULL));
    if (pdev_count == 0) { fprintf(stderr, "%s: no GPU\n", tag); return 2; }
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
        fprintf(stderr, "%s: no graphics+present queue\n", tag);
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

    uint32_t w = host->width ? host->width : 800;
    uint32_t h = host->height ? host->height : 600;
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

    int max_frames = cmpl_vk_frame_limit();
    int frame = 0;
    fprintf(stderr, "%s: window %ux%u, frames=%d (0=until close)\n", tag, w, h, max_frames);

    while (host->running && *host->running) {
        if (host->poll_events)
            host->poll_events(host->poll_ctx);

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
        if (pr != VK_ERROR_OUT_OF_DATE_KHR && pr != VK_SUBOPTIMAL_KHR)
            CHECK(pr);

        frame++;
        if (max_frames > 0 && frame >= max_frames)
            *host->running = 0;
    }

    vkDeviceWaitIdle(device);
    fprintf(stderr, "%s: presented %d frames\n", tag, frame);

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
    return 0;
}

/* Prefer Wayland when WAYLAND_DISPLAY is set; else X11/XCB on DISPLAY (bspwm etc.). */
int cmpl_vk_window(void) {
    const char *wd = getenv("WAYLAND_DISPLAY");
    const char *xd = getenv("DISPLAY");
    if (wd && wd[0])
        return cmpl_vk_window_wayland();
    if (xd && xd[0])
        return cmpl_vk_window_xcb();
    int r = cmpl_vk_window_wayland();
    if (r != 10)
        return r;
    return cmpl_vk_window_xcb();
}
