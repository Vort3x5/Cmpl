# Cmpl Vulkan (headless red triangle)

Game milestone 1: prove Vulkan works end-to-end from a `.jai` entry point.

## Layout

| Path | Role |
|------|------|
| `include/vulkan/` | Vendored Vulkan-Headers (v1.2.198) |
| `shaders/tri.*.spv` | Minimal red-triangle SPIR-V |
| `cmpl_vk.c` | Headless renderer (`cmpl_vk_triangle`) |
| `cmpl_vk_present.c` | Shared swapchain + red triangle |
| `cmpl_vk_wl.c` | Wayland host |
| `cmpl_vk_xcb.c` | X11/XCB host (bspwm, i3, …) |
| `wayland/` | Vendored xdg-shell protocol |
| `libcmpl_vk.so` | Built by `./nob all` / `./nob vk` |
| `../BasicJai/vulkan_triangle.jai` | Headless Jai driver |
| `../BasicJai/vulkan_window.jai` | On-screen triangle |
| `../BasicJai/vulkan_sprite.jai` | Textured sprite (milestone 2) |
| `shaders/sprite.*.spv` | Textured-quad shaders |

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

`./nob vk` rebuilds only `examples/vulkan/libcmpl_vk.so` (absolute `/usr/lib/libvulkan.so` + `libwayland-client.so` when present).

**Headless expect:** stderr `cmpl_vk: center pixel rgba=255,0,0,255` and **exit 0**.

### On-screen (Wayland)

```bash
./out/cmpl examples/BasicJai/vulkan_window.jai
CMPL_VK_FRAMES=120 ./out/out; echo $?
```

**Expect:** window titled `Cmpl Vulkan` with a red triangle; stderr `presented N frames`; **exit 0**.  
`cmpl_vk_window` picks **Wayland** if `WAYLAND_DISPLAY` is set, otherwise **XCB** if `DISPLAY` is set (bspwm/X11).  
Neither → exit **10**. `CMPL_VK_FRAMES=0` runs until the window is closed.

### Textured sprite (milestone 2)

```bash
./out/cmpl examples/BasicJai/vulkan_sprite.jai
CMPL_VK_FRAMES=90 ./out/out; echo $?
```

**Expect:** window `Cmpl Sprite` with a green procedural character; stderr `(textured sprite)`; **exit 0**.

On pure X11, `nix-shell` alone does not create Wayland — XCB is the right path. Nested Weston is optional:

```bash
nix-shell -p weston --run 'weston --backend=x11-backend.so --socket=cmpl-wl & sleep 1; WAYLAND_DISPLAY=cmpl-wl CMPL_VK_FRAMES=60 ./out/out'
```

## Notes

- Shaders are loaded from `examples/vulkan/shaders/` relative to the **current working directory** (run from repo root).
### Moving player (milestone 3)

Logic in Jai (`vulkan_player.jai`); C only session glue:

```bash
./out/cmpl examples/BasicJai/vulkan_player.jai
CMPL_VK_FRAMES=120 ./out/out; echo $?
```

API: `cmpl_vk_game_open/poll/key/draw/close`. WASD/arrows; auto-drift without keys.  
Next: NPCs/dialog when needed — grow more game state in Jai (`f32`/slices only if required).
