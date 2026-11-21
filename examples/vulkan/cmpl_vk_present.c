#include "cmpl_vk_present.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { VkResult _r = (x); if (_r != VK_SUCCESS) { \
    fprintf(stderr, "VK error %d at %s:%d\n", (int)_r, __FILE__, __LINE__); \
    return (int)_r ? (int)_r : 1; } } while (0)

#define TEX_W 64
#define TEX_H 64

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

static uint32_t find_mem_type(VkPhysicalDevice pd, uint32_t bits, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(pd, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & flags) == flags)
            return i;
    return UINT32_MAX;
}

/* Simple procedural "hero" sprite: body + eyes on checkerboard alpha. */
static void fill_sprite_rgba(uint8_t *px) {
    for (int y = 0; y < TEX_H; y++) {
        for (int x = 0; x < TEX_W; x++) {
            uint8_t *p = px + (size_t)(y * TEX_W + x) * 4;
            int cx = x - TEX_W / 2;
            int cy = y - TEX_H / 2;
            int r2 = cx * cx + cy * cy;
            if (r2 > (28 * 28)) {
                p[0] = p[1] = p[2] = 0;
                p[3] = 0;
                continue;
            }
            /* cloak */
            p[0] = 40;
            p[1] = 120;
            p[2] = 70;
            p[3] = 255;
            /* face oval */
            if (cy < -2 && cx * cx + (cy + 8) * (cy + 8) < 10 * 10) {
                p[0] = 220; p[1] = 190; p[2] = 150;
            }
            /* eyes */
            if ((cx + 5) * (cx + 5) + (cy + 10) * (cy + 10) < 3 * 3) {
                p[0] = p[1] = p[2] = 20;
            }
            if ((cx - 5) * (cx - 5) + (cy + 10) * (cy + 10) < 3 * 3) {
                p[0] = p[1] = p[2] = 20;
            }
            /* belt */
            if (cy > 4 && cy < 10 && cx * cx < 18 * 18) {
                p[0] = 180; p[1] = 140; p[2] = 40;
            }
        }
    }
}

static int create_buffer(VkDevice device, VkPhysicalDevice pdev, VkDeviceSize size,
                         VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_flags,
                         VkBuffer *out_buf, VkDeviceMemory *out_mem) {
    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
    };
    CHECK(vkCreateBuffer(device, &bci, NULL, out_buf));
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(device, *out_buf, &mr);
    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mr.size,
        .memoryTypeIndex = find_mem_type(pdev, mr.memoryTypeBits, mem_flags),
    };
    CHECK(vkAllocateMemory(device, &mai, NULL, out_mem));
    CHECK(vkBindBufferMemory(device, *out_buf, *out_mem, 0));
    return 0;
}

int cmpl_vk_run_swapchain(CmplVkWindowHost *host) {
    VkInstance instance = host->instance;
    VkSurfaceKHR surface = host->surface;
    const char *tag = host->log_tag ? host->log_tag : "cmpl_vk";
    int sprite = (host->draw == CMPL_VK_DRAW_SPRITE);

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

    /* ---- resources for triangle vs sprite ---- */
    VkBuffer vbo = VK_NULL_HANDLE;
    VkDeviceMemory vbo_mem = VK_NULL_HANDLE;
    VkImage tex_img = VK_NULL_HANDLE;
    VkDeviceMemory tex_mem = VK_NULL_HANDLE;
    VkImageView tex_view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    VkDescriptorPool dp = VK_NULL_HANDLE;
    VkDescriptorSet ds = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkShaderModule vert_mod = VK_NULL_HANDLE, frag_mod = VK_NULL_HANDLE;

    const char *vert_path = sprite
        ? "examples/vulkan/shaders/sprite.vert.spv"
        : "examples/vulkan/shaders/tri.vert.spv";
    const char *frag_path = sprite
        ? "examples/vulkan/shaders/sprite.frag.spv"
        : "examples/vulkan/shaders/tri.frag.spv";

    size_t vert_n = 0, frag_n = 0;
    uint32_t *vert_code = load_spv(vert_path, &vert_n);
    uint32_t *frag_code = load_spv(frag_path, &frag_n);
    if (!vert_code || !frag_code) return 4;

    VkShaderModuleCreateInfo smci = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    smci.codeSize = vert_n * 4; smci.pCode = vert_code;
    CHECK(vkCreateShaderModule(device, &smci, NULL, &vert_mod));
    smci.codeSize = frag_n * 4; smci.pCode = frag_code;
    CHECK(vkCreateShaderModule(device, &smci, NULL, &frag_mod));
    free(vert_code); free(frag_code);

    VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert_mod, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag_mod, .pName = "main" },
    };

    VkPipelineVertexInputStateCreateInfo vi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkVertexInputBindingDescription bind = {
        .binding = 0, .stride = 16, .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attrs[2] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0 },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 8 },
    };
    if (sprite) {
        vi.vertexBindingDescriptionCount = 1;
        vi.pVertexBindingDescriptions = &bind;
        vi.vertexAttributeDescriptionCount = 2;
        vi.pVertexAttributeDescriptions = attrs;
    }

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
    VkPipelineColorBlendAttachmentState blend_att = {
        .blendEnable = sprite ? VK_TRUE : VK_FALSE,
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
        .attachmentCount = 1,
        .pAttachments = &blend_att,
    };

    if (sprite) {
        /* Texture image */
        VkImageCreateInfo imgci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = { TEX_W, TEX_H, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        CHECK(vkCreateImage(device, &imgci, NULL, &tex_img));
        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(device, tex_img, &mr);
        VkMemoryAllocateInfo mai = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = mr.size,
            .memoryTypeIndex = find_mem_type(pdev, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        };
        CHECK(vkAllocateMemory(device, &mai, NULL, &tex_mem));
        CHECK(vkBindImageMemory(device, tex_img, tex_mem, 0));

        VkBuffer staging;
        VkDeviceMemory staging_mem;
        VkDeviceSize tex_bytes = TEX_W * TEX_H * 4;
        if (create_buffer(device, pdev, tex_bytes,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          &staging, &staging_mem))
            return 1;
        uint8_t *mapped = NULL;
        CHECK(vkMapMemory(device, staging_mem, 0, tex_bytes, 0, (void **)&mapped));
        fill_sprite_rgba(mapped);
        vkUnmapMemory(device, staging_mem);

        /* One-shot upload command buffer */
        VkCommandPoolCreateInfo cpci0 = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = qfam,
        };
        VkCommandPool upload_pool;
        CHECK(vkCreateCommandPool(device, &cpci0, NULL, &upload_pool));
        VkCommandBufferAllocateInfo cbai0 = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = upload_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VkCommandBuffer upcmd;
        CHECK(vkAllocateCommandBuffers(device, &cbai0, &upcmd));
        VkCommandBufferBeginInfo begin0 = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        CHECK(vkBeginCommandBuffer(upcmd, &begin0));
        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = tex_img,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };
        vkCmdPipelineBarrier(upcmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        VkBufferImageCopy copy = {
            .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .imageExtent = { TEX_W, TEX_H, 1 },
        };
        vkCmdCopyBufferToImage(upcmd, staging, tex_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(upcmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        CHECK(vkEndCommandBuffer(upcmd));
        VkSubmitInfo upsubmit = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &upcmd,
        };
        CHECK(vkQueueSubmit(queue, 1, &upsubmit, VK_NULL_HANDLE));
        CHECK(vkQueueWaitIdle(queue));
        vkDestroyCommandPool(device, upload_pool, NULL);
        vkDestroyBuffer(device, staging, NULL);
        vkFreeMemory(device, staging_mem, NULL);

        VkImageViewCreateInfo tiv = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = tex_img,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };
        CHECK(vkCreateImageView(device, &tiv, NULL, &tex_view));

        VkSamplerCreateInfo sci = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        };
        CHECK(vkCreateSampler(device, &sci, NULL, &sampler));

        VkDescriptorSetLayoutBinding dslb = {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        };
        VkDescriptorSetLayoutCreateInfo dslci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &dslb,
        };
        CHECK(vkCreateDescriptorSetLayout(device, &dslci, NULL, &dsl));

        VkDescriptorPoolSize dps = {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
        };
        VkDescriptorPoolCreateInfo dpci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &dps,
        };
        CHECK(vkCreateDescriptorPool(device, &dpci, NULL, &dp));
        VkDescriptorSetAllocateInfo dsai = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = dp,
            .descriptorSetCount = 1,
            .pSetLayouts = &dsl,
        };
        CHECK(vkAllocateDescriptorSets(device, &dsai, &ds));
        VkDescriptorImageInfo dii = {
            .sampler = sampler,
            .imageView = tex_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        VkWriteDescriptorSet wds = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &dii,
        };
        vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);

        /* Quad in NDC: pos.xy + uv.xy */
        float verts[] = {
            -0.35f, -0.35f, 0.f, 1.f,
             0.35f, -0.35f, 1.f, 1.f,
             0.35f,  0.35f, 1.f, 0.f,
            -0.35f, -0.35f, 0.f, 1.f,
             0.35f,  0.35f, 1.f, 0.f,
            -0.35f,  0.35f, 0.f, 0.f,
        };
        if (create_buffer(device, pdev, sizeof(verts),
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          &vbo, &vbo_mem))
            return 1;
        void *vmap = NULL;
        CHECK(vkMapMemory(device, vbo_mem, 0, sizeof(verts), 0, &vmap));
        memcpy(vmap, verts, sizeof(verts));
        vkUnmapMemory(device, vbo_mem);

        VkPipelineLayoutCreateInfo plci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &dsl,
        };
        CHECK(vkCreatePipelineLayout(device, &plci, NULL, &layout));
    } else {
        VkPipelineLayoutCreateInfo plci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        CHECK(vkCreatePipelineLayout(device, &plci, NULL, &layout));
    }

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
        VkClearValue clear = { .color = {{0.08f, 0.09f, 0.12f, 1.0f}} };
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
        if (sprite) {
            VkDeviceSize off = 0;
            vkCmdBindVertexBuffers(cmds[i], 0, 1, &vbo, &off);
            vkCmdBindDescriptorSets(cmds[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    layout, 0, 1, &ds, 0, NULL);
            vkCmdDraw(cmds[i], 6, 1, 0, 0);
        } else {
            vkCmdDraw(cmds[i], 3, 1, 0, 0);
        }
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
    fprintf(stderr, "%s: %s %ux%u, frames=%d\n",
            tag, sprite ? "sprite" : "triangle", w, h, max_frames);

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
    fprintf(stderr, "%s: presented %d frames%s\n",
            tag, frame, sprite ? " (textured sprite)" : "");

    vkDestroyFence(device, fence, NULL);
    vkDestroySemaphore(device, render_done, NULL);
    vkDestroySemaphore(device, image_avail, NULL);
    vkDestroyCommandPool(device, pool, NULL);
    vkDestroyPipeline(device, pipeline, NULL);
    vkDestroyPipelineLayout(device, layout, NULL);
    vkDestroyShaderModule(device, frag_mod, NULL);
    vkDestroyShaderModule(device, vert_mod, NULL);
    if (sprite) {
        vkDestroyDescriptorPool(device, dp, NULL);
        vkDestroyDescriptorSetLayout(device, dsl, NULL);
        vkDestroySampler(device, sampler, NULL);
        vkDestroyImageView(device, tex_view, NULL);
        vkDestroyImage(device, tex_img, NULL);
        vkFreeMemory(device, tex_mem, NULL);
        vkDestroyBuffer(device, vbo, NULL);
        vkFreeMemory(device, vbo_mem, NULL);
    }
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

static int dispatch_window(CmplVkDrawMode draw) {
    const char *wd = getenv("WAYLAND_DISPLAY");
    const char *xd = getenv("DISPLAY");
    if (wd && wd[0])
        return cmpl_vk_window_wayland(draw);
    if (xd && xd[0])
        return cmpl_vk_window_xcb(draw);
    int r = cmpl_vk_window_wayland(draw);
    if (r != 10)
        return r;
    return cmpl_vk_window_xcb(draw);
}

int cmpl_vk_window(void) {
    return dispatch_window(CMPL_VK_DRAW_TRIANGLE);
}

int cmpl_vk_sprite(void) {
    return dispatch_window(CMPL_VK_DRAW_SPRITE);
}
