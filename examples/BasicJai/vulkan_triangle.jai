// Milestone: Vulkan red triangle (headless) via #library
//
//   nix-shell   # or: nix develop
//   ./nob all   # builds out/cmpl + examples/vulkan/libcmpl_vk.so
//   ./out/cmpl examples/BasicJai/vulkan_triangle.jai
//   ./out/out; echo $?
//
// Expected: stderr shows "center pixel rgba=255,0,0,255", exit 0

vk :: #library "examples/vulkan/libcmpl_vk.so";

cmpl_vk_triangle :: () -> int #foreign vk;

main :: () -> int {
    return cmpl_vk_triangle();
}
