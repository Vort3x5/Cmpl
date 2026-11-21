// Milestone: real Vulkan FFI from Jai (no cmpl_vk_* helper).
// Layout pads match System V / Vulkan headers (u32 + pad → 8-byte store).
//
//   nix-shell   # or: nix develop
//   ./nob all
//   ./out/cmpl examples/BasicJai/vulkan_ffi.jai
//   ./out/out; echo $?   # 0 = VK_SUCCESS create+destroy
//
// Forces: #system_library, #foreign rename, *Struct / *int, u32 fields.

vulkan :: #system_library "vulkan";

// VkResult vkCreateInstance(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*);
vkCreateInstance :: (info: int, alloc: int, instance: int) -> int #foreign vulkan "vkCreateInstance";
vkDestroyInstance :: (instance: int, alloc: int) #foreign vulkan "vkDestroyInstance";

// sizeof == 64; offsets match vulkan_core.h on x86_64
VkInstanceCreateInfo :: struct {
    sType: u32;                 // 0
    _pad0: u32;                 // 4
    pNext: int;                 // 8
    flags: u32;                 // 16
    _pad1: u32;                 // 20
    pApplicationInfo: int;      // 24
    enabledLayerCount: u32;     // 32
    _pad2: u32;                 // 36
    ppEnabledLayerNames: int;   // 40
    enabledExtensionCount: u32; // 48
    _pad3: u32;                 // 52
    ppEnabledExtensionNames: int; // 56
}

main :: () -> int {
    ci: VkInstanceCreateInfo;
    // VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO == 1; 8-byte stores clear adjacent pads
    ci.sType = 1;
    ci.pNext = 0;
    ci.flags = 0;
    ci.pApplicationInfo = 0;
    ci.enabledLayerCount = 0;
    ci.ppEnabledLayerNames = 0;
    ci.enabledExtensionCount = 0;
    ci.ppEnabledExtensionNames = 0;

    instance: int;
    instance = 0;

    r := vkCreateInstance(*ci, 0, *instance);
    if r != 0 {
        return r;
    }
    vkDestroyInstance(instance, 0);
    return 0;
}
