# DIY Jai: Game-Driven Systems Programming Language

A Jai-inspired systems programming language built while developing a 2D fantasy Vulkan-Wayland game. Every language feature is driven by real game development needs.

## 🎯 Core Philosophy

### **Essential Rules**
1. **Game-Driven Development**: Every compiler feature must successfully build the current game milestone
2. **No Feature Without Use Case**: Nothing gets implemented without a concrete game requirement
3. **Self-Hosting Build System**: Build scripts written in the language itself, not external tools
4. **Iterative Delivery**: Always have a working compiler + playable game
5. **Performance First**: 60 FPS gameplay with <1 second compile times

### **Technology Stack**
- **C**: Compiler core (lexer, parser, codegen)
- **Cmpl**: Build system and metaprogramming  
- **Lua**: Asset pipeline and game data
- **FASM**: Performance-critical assembly operations
- **NixOS**: Reproducible development environment

## 🚀 Planned Language Features

### **1: Bootstrap**
- [x] Basic lexer and parser
- [x] Procedures and variables (`main :: () { x := 42; }`)
- [x] C code generation
- [x] Simple expressions and function calls

### **2: Memory & Graphics**
- [ ] Structs and member access
- [ ] Arrays and slices (`[]Type`, `[N]Type`)
- [ ] Defer statements
- [ ] Arena allocator integration
- [ ] Basic FFI for Vulkan calls

### **3: Game Logic**
- [ ] Control flow (`if`, `for`, `while`)
- [ ] Enums and basic pattern matching
- [ ] Dynamic arrays (`[dynamic]Type`)
- [ ] Pointers and basic arithmetic
- [ ] Standard library essentials

### **4: Compile-Time Power**
- [ ] `#run` directive - compile-time execution
- [ ] Lua integration for asset processing
- [ ] Compile-time constants from external data
- [ ] Basic metaprogramming

### **5: Performance**
- [ ] FASM integration for SIMD operations
- [ ] Structure of Arrays (`#soa` directive)
- [ ] Context system for allocators
- [ ] Profile-guided optimization

### **6: Advanced Features**
- [ ] Code generation and `#modify`
- [ ] Polymorphic procedures
- [ ] Self-hosted build system
- [ ] Package/module system

## 🎮 Game Milestones

| Phase | Game Goal | Technical Focus |
|-------|-----------|----------------|
| 1 | Vulkan triangle | Basic compilation |
| 2 | Textured sprites | Memory management |
| 3 | Moving player | Game logic |
| 4 | NPCs and dialog | Data-driven content |
| 5 | Full gameplay | Performance optimization |
| 6 | Polished release | Advanced language features |

## 🎯 Success Metrics

### **Compiler Metrics**
- Compile speed: <1 second for game-sized projects
- Memory usage: <100MB during compilation
- Feature completeness: 60%+ of planned Jai features

### **Game Metrics**
- Performance: Stable 60 FPS
- Completeness: Playable start-to-finish
- Fun factor: Actually enjoyable to play

## 🚨 Development Rules

### **What We DON'T Do**
- ❌ Implement features without game use cases
- ❌ Add C++ complexity or OOP features
- ❌ Optimize prematurely
- ❌ Perfect code over working code

### **What We DO**
- ✅ Test every feature by using it in the game
- ✅ Keep the game simple but complete
- ✅ Document with real examples

**Status**: In progress  
**License**: GPL
