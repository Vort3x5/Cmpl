# DIY Jai: Game-Driven Systems Programming Language

A Jai-inspired systems programming language built while developing a 2D fantasy Vulkan-Wayland game. Every language feature is driven by real game development needs.

Syntax and semantics are aligned with [The Way to Jai](https://github.com/Ivo-Balbaert/The_Way_to_Jai) (submodule `res/The_Way_to_Jai`) where we claim support — this is still a **bootstrap subset**, not a full Jai implementation.

## Core Philosophy

### Essential Rules
1. **Game-Driven Development**: Every compiler feature must successfully build the current game milestone
2. **No Feature Without Use Case**: Nothing gets implemented without a concrete game requirement
3. **Self-Hosting Build System**: Build scripts written in the language itself, not external tools
4. **Iterative Delivery**: Always have a working compiler + playable game
5. **Performance First**: 60 FPS gameplay with <1 second compile times

### Technology Stack
- **C**: Compiler core (lexer, parser, codegen)
- **Cmpl**: Build system and metaprogramming (planned)
- **FASM**: Codegen target (+ `ld` when `#library` needs a real `.so`)
- **NixOS**: Reproducible development environment

### What We Don't Do
- Features without a game use case
- C++ / OOP complexity
- Premature optimization
- **CUDA** — GPU work goes through **Vulkan**; hot CPU paths through **FASM SIMD** / `#soa`

## Build & Run

```bash
./nob all
./nob test                     # builds cmpl + runs tests.json
./out/cmpl <file.jai>          # → out/out.asm → out/out
./out/out; echo $?             # exit status = low 8 bits of main's return
```

Requires `cc`, `fasm`, and (for `#library` `.so` demos) `ld` on `PATH`. For `vulkan_ffi` / Vulkan tests use `nix-shell` first.

### How to test (look at exit code + side effects)

| File | Expected `$?` | Also watch |
|------|---------------|------------|
| `examples/BasicJai/structs.jai` | **42** | `struc` + field load/store in `out/out.asm` |
| `examples/BasicJai/arrays.jai` | **30** | `[N]int` indexing |
| `examples/BasicJai/for_range.jai` | **55** | Jai-inclusive `0..10` → sum 55 |
| `examples/BasicJai/defer.jai` | **3** | `defer` LIFO (block + loop body per iteration) |
| `examples/BasicJai/pointers.jai` | **108** | `*Type`, `*a`, `b.*` |
| `examples/BasicJai/nested_call.jai` | **6** | `add(1, add(2, 3))` |
| `examples/BasicJai/u32_fields.jai` | **5** | sized `u32` field store/load |
| `examples/BasicJai/u32_local.jai` | **1** | `u32` local in non-`main` (8-byte slot) |
| `examples/BasicJai/u32_array.jai` | **7** | `[4]u32` element store/load |
| `examples/BasicJai/u32_deref.jai` | **42** | `*u32` store does not clobber neighbor |
| `examples/BasicJai/defer_call_return.jai` | **7** | `defer fval(1); return 7` |
| `examples/BasicJai/for_call_bound.jai` | **4** | `for i: 0..fval(2)` |
| `examples/BasicJai/shift.jai` | **8** | `1 << 3` |
| `examples/BasicJai/sse_call_expr.jai` | **4** | `1 + fval(2)` — TAC stack temps / SSE ABI |
| `examples/BasicJai/arena.jai` | **42** | `#foreign cmpl_rt` → `arena_alloc` |
| `examples/BasicJai/foreign.jai` | **3** | prints **`Hi`**, `#library` / `#foreign` |
| `examples/BasicJai/foreign_so.jai` | **30** | `#library "…/libadd"` (no `.so` in path) |
| `examples/BasicJai/vulkan_ffi.jai` | **0** | `#system_library "vulkan"` → `vkCreateInstance` (no C helper) |
| `examples/BasicJai/vulkan_triangle.jai` | **0** | legacy headless triangle via `libcmpl_vk` (shrink, don’t grow) |
| `examples/BasicJai/loop_fcalls.jai` | **70** | regression |
| `main.jai` | **251** | bootstrap suite (1019 & 0xFF) |

Shared-lib demo:

```bash
cc -shared -fPIC -o examples/c_lib/libadd.so examples/c_lib/add.c
./out/cmpl examples/BasicJai/foreign_so.jai
./out/out; echo $?   # 30
```

Vulkan (needs `libvulkan` + Wayland for the window — on Nix use the shell):

```bash
nix-shell          # or: nix develop
./nob all          # builds out/cmpl + examples/vulkan/libcmpl_vk.so
./out/cmpl examples/BasicJai/vulkan_triangle.jai && ./out/out; echo $?   # headless red pixel
./out/cmpl examples/BasicJai/vulkan_window.jai && CMPL_VK_FRAMES=90 ./out/out; echo $?  # on-screen triangle
./out/cmpl examples/BasicJai/vulkan_sprite.jai && CMPL_VK_FRAMES=90 ./out/out; echo $?   # textured sprite
./out/cmpl examples/BasicJai/vulkan_player.jai && CMPL_VK_FRAMES=120 ./out/out; echo $?  # moving player (Jai loop)
```

Details: [examples/vulkan/README.md](examples/vulkan/README.md) · Nix: [`shell.nix`](shell.nix) / [`flake.nix`](flake.nix)

## Jai alignment (vs The Way to Jai)

### Matches (for supported features)
- `Name :: …`, `:=`, `name: Type;`, `-> Type`
- `struct`, field access, fixed `[N]Type` arrays
- `if` / `while` / `for i: a..b` (**inclusive** ends, per §15/§18)
- `defer` at block / scope exit (LIFO), including per loop iteration
- Pointers: `*Type`, `*addr`, postfix `.*` deref (§10)
- Shifts: `<<`, `>>`
- Natural struct field alignment; sized loads/stores for `u8`/`u16`/`u32`
- FFI: `#library` / `#system_library` + `#foreign lib` (§29); library paths omit `.so`
- Optional C symbol rename: `#foreign lib "CSymbol"`

### Still diverging / missing (OK for bootstrap)
- No `#import "Basic"`, `#load`, `#run`, Context, modules
- No `[]Type` views, `.count`, array literals, `for arr`
- No `name: Type = expr`, `---`, floats, strings-as-values
- `cmpl_rt` is an embedded FASM runtime (stand-in until more of libc/Vulkan is linked)

## Planned Language Features

### 1: Bootstrap
- [x] Lexer/parser, procs, vars, control flow, FASM macros, calls
- [x] Inclusive `for` ranges; `#foreign` / `#library`
- [x] Comparisons + shifts (`<<` / `>>`); [ ] `/`, `%` in codegen

### 2: Memory & Graphics
- [x] Structs + member access (natural align, sized fields)
- [x] Fixed arrays `[N]Type`
- [ ] Slices `[]Type`
- [x] Defer (block + loop)
- [x] Arena bump (`cmpl_rt`)
- [x] `#foreign` + real `.so` link
- [x] Direct Vulkan FFI (`vulkan_ffi.jai` → `vkCreateInstance`)
- [x] Vulkan triangle via C helper `libcmpl_vk` (legacy — shrink, don’t grow)
- [x] Wayland/XCB window via C helper (legacy)
- [x] Textured sprite via C helper (legacy)
- [x] Moving player via C helper session API (legacy; Jai owns loop)

### 3–6
See earlier roadmap: enums, pointers-as-first-class APIs, `#run`, SIMD/`#soa`, self-hosting.

## Game Milestones

| Phase | Game Goal | Technical Focus |
|-------|-----------|----------------|
| 1 | Vulkan instance | direct `#foreign` → `libvulkan` (`vulkan_ffi.jai`) |
| 1b | On-screen triangle | Wayland / XCB + swapchain |
| 2 | Textured sprites | Image + sampler + textured quad |
| 3 | Moving player | Jai loop + position; thin C session API |
| 4 | NPCs and dialog | Data-driven content |
| 5 | Full gameplay | FASM SIMD / `#soa` |
| 6 | Polished release | Advanced language features |

**Status**: Milestone 2 language surface + `.so` FFI path working  
**License**: GPL
