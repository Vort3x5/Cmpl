// Milestone: real Vulkan FFI from Jai (no cmpl_vk_* helper).
// Natural struct layout (u32 + pad for pointers) matches vulkan_core.h on x86_64.
//
//   nix-shell   # or: nix develop
//   ./nob all
//   ./out/cmpl examples/BasicJai/vulkan_ffi.jai
//   ./out/out; echo $?   # 0 = VK_SUCCESS create+destroy

vulkan :: #system_library "vulkan";

vkCreateInstance :: (info: *VkInstanceCreateInfo, alloc: int, instance: *int) -> int #foreign vulkan "vkCreateInstance";
vkDestroyInstance :: (instance: int, alloc: int) #foreign vulkan "vkDestroyInstance";

VkInstanceCreateInfo :: struct {
    sType: u32;
    pNext: int;
    flags: u32;
    pApplicationInfo: int;
    enabledLayerCount: u32;
    ppEnabledLayerNames: int;
    enabledExtensionCount: u32;
    ppEnabledExtensionNames: int;
}

main :: () -> int {
    ci: VkInstanceCreateInfo;
    ci.sType = 1;  // VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
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
