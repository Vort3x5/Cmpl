# DIY Jai: Game-Driven Systems Programming Language

A Jai-inspired systems programming language built while developing a 2D fantasy Vulkan-Wayland game. Every language feature is driven by real game development needs.

Syntax and semantics are aligned with [The Way to Jai](res/The_Way_to_Jai) where we claim support — this is still a **bootstrap subset**, not a full Jai implementation.

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
./out/cmpl <file.jai>          # → out/out.asm → out/out
./out/out; echo $?             # exit status = low 8 bits of main's return
```

Requires `cc`, `fasm`, and (for `#library` `.so` demos) `ld` on `PATH`.

### How to test (look at exit code + side effects)

| File | Expected `$?` | Also watch |
|------|---------------|------------|
| `examples/BasicJai/structs.jai` | **42** | `struc` + field load/store in `out/out.asm` |
| `examples/BasicJai/arrays.jai` | **30** | `[N]int` indexing |
| `examples/BasicJai/for_range.jai` | **55** | Jai-inclusive `0..10` → sum 55 |
| `examples/BasicJai/defer.jai` | **3** | `; defer` LIFO at block end |
| `examples/BasicJai/pointers.jai` | **108** | `*Type`, `*a`, `<< b` |
| `examples/BasicJai/arena.jai` | **42** | `#foreign cmpl_rt` → `arena_alloc` |
| `examples/BasicJai/foreign.jai` | **3** | prints **`Hi`**, `#library` / `#foreign` |
| `examples/BasicJai/foreign_so.jai` | **30** | real `.so` via `ld` (build lib first) |
| `examples/BasicJai/vulkan_triangle.jai` | **0** | headless red triangle; stderr `rgba=255,0,0,255` |
| `examples/BasicJai/loop_fcalls.jai` | **70** | regression |
| `main.jai` | **251** | bootstrap suite (1019 & 0xFF) |

Shared-lib demo:

```bash
cc -shared -fPIC -o examples/c_lib/libadd.so examples/c_lib/add.c
./out/cmpl examples/BasicJai/foreign_so.jai
./out/out; echo $?   # 30
```

Vulkan triangle (needs `libvulkan` — on Nix use the shell):

```bash
nix-shell          # or: nix develop
./nob all          # builds out/cmpl + examples/vulkan/libcmpl_vk.so
./out/cmpl examples/BasicJai/vulkan_triangle.jai
./out/out; echo $?   # 0  + red pixel on stderr
```

Details: [examples/vulkan/README.md](examples/vulkan/README.md) · Nix: [`shell.nix`](shell.nix) / [`flake.nix`](flake.nix)

## Jai alignment (vs The Way to Jai)

### Matches (for supported features)
- `Name :: …`, `:=`, `name: Type;`, `-> Type`
- `struct`, field access, fixed `[N]Type` arrays
- `if` / `while` / `for i: a..b` (**inclusive** ends, per §15/§18)
- `defer` at block scope (LIFO)
- Pointers: `*Type`, `*addr`, `<<` deref (§10)
- FFI: `#library` / `#system_library` + `#foreign lib` (§29)
- Optional C symbol rename: `#foreign lib "CSymbol"`

### Still diverging / missing (OK for bootstrap)
- No `#import "Basic"`, `#load`, `#run`, Context, modules
- No `[]Type` views, `.count`, array literals, `for arr`
- No `name: Type = expr`, `---`, floats, strings-as-values
- Defer inside loops is not per-iteration yet
- `cmpl_rt` is an embedded FASM runtime (stand-in until more of libc/Vulkan is linked)

## Planned Language Features

### 1: Bootstrap
- [x] Lexer/parser, procs, vars, control flow, FASM macros, calls
- [x] Inclusive `for` ranges; `#foreign` / `#library`
- [ ] Full operator set in codegen (`/`, `%`, `!=`, …)

### 2: Memory & Graphics
- [x] Structs + member access
- [x] Fixed arrays `[N]Type`
- [ ] Slices `[]Type`
- [x] Defer (block scope)
- [x] Arena bump (`cmpl_rt`)
- [x] `#foreign` + real `.so` link (`ld`)
- [x] Vulkan triangle (headless red pixel via `libcmpl_vk.so`)
- [ ] Wayland surface + on-screen swapchain

### 3–6
See earlier roadmap: enums, pointers-as-first-class APIs, `#run`, SIMD/`#soa`, self-hosting.

## Game Milestones

| Phase | Game Goal | Technical Focus |
|-------|-----------|----------------|
| 1 | Vulkan triangle | `#foreign` + `libvulkan` |
| 2 | Textured sprites | Memory / slices |
| 3 | Moving player | Game logic |
| 4 | NPCs and dialog | Data-driven content |
| 5 | Full gameplay | FASM SIMD / `#soa` |
| 6 | Polished release | Advanced language features |

**Status**: Milestone 2 language surface + `.so` FFI path working  
**License**: GPL
