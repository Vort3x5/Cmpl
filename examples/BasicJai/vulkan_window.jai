// Wayland + Vulkan swapchain red triangle
//
//   nix-shell
//   ./nob all
//   ./out/cmpl examples/BasicJai/vulkan_window.jai
//   ./out/out; echo $?
//
// Uses WAYLAND_DISPLAY if set, else DISPLAY (X11/XCB — bspwm etc.).
// Optional: CMPL_VK_FRAMES=60 (default 180; 0=until close).
// Expected: window with red triangle, stderr "presented N frames", exit 0.
// No display: exit 10.

vk :: #library "examples/vulkan/libcmpl_vk.so";

cmpl_vk_window :: () -> int #foreign vk;

main :: () -> int {
    return cmpl_vk_window();
}
