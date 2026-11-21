// Milestone 2: textured sprite (procedural RGBA + sampler)
//
//   nix-shell
//   ./nob all
//   ./out/cmpl examples/BasicJai/vulkan_sprite.jai
//   CMPL_VK_FRAMES=90 ./out/out; echo $?
//
// Window shows a green cloak "hero" sprite on dark clear. Exit 0.

vk :: #library "examples/vulkan/libcmpl_vk";

cmpl_vk_sprite :: () -> int #foreign vk;

main :: () -> int {
    return cmpl_vk_sprite();
}
