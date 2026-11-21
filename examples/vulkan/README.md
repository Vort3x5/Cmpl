# Cmpl Vulkan (headless red triangle)

Game milestone 1: prove Vulkan works end-to-end from a `.jai` entry point.

## Layout

| Path | Role |
|------|------|
| `include/vulkan/` | Vendored Vulkan-Headers (v1.2.198) |
| `shaders/tri.*.spv` | Minimal red-triangle SPIR-V |
| `cmpl_vk.c` | Renderer (`cmpl_vk_triangle`) |
| `libcmpl_vk.so` | Built by `./nob all` / `./nob vk` |
| `../BasicJai/vulkan_triangle.jai` | Jai driver via `#library` / `#foreign` |

Window/WSI (Wayland) comes later — this path is headless so it works without a compositor.

## Build & test

On NixOS / nix-on-other-distros, enter the project shell first (provides `vulkan-loader` for the linker):

```bash
nix-shell          # or: nix develop
./nob all
./out/cmpl examples/BasicJai/vulkan_triangle.jai
./out/out; echo $?
```

**fish:**

```fish
cd ~/Mvsc/Cmpl
nix-shell --run 'fish'   # or: nix develop -c fish
./nob all
./out/cmpl examples/BasicJai/vulkan_triangle.jai
./out/out
echo $status
```

`./nob vk` rebuilds only `examples/vulkan/libcmpl_vk.so` (links via absolute `/usr/lib/libvulkan.so` when present, else `-lvulkan`).

**Expect:** stderr `cmpl_vk: center pixel rgba=255,0,0,255` and **exit 0**.

## Notes

- Shaders are loaded from `examples/vulkan/shaders/` relative to the **current working directory** (run from repo root).
- Next step toward the game: Wayland surface + swapchain, then move more setup from C into Jai as the language grows (`*T` structs matching Vk*).
