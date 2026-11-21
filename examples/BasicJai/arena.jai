// Bump arena via #foreign (cmpl_rt runtime)
// Expected exit: 42

cmpl_rt :: #library "cmpl_rt";

arena_reset :: () #foreign cmpl_rt;
arena_alloc :: (size: int) -> int #foreign cmpl_rt;
mem_store :: (addr: int, val: int) -> int #foreign cmpl_rt;
mem_load :: (addr: int) -> int #foreign cmpl_rt;

main :: () -> int {
    arena_reset();
    p := arena_alloc(8);
    mem_store(p, 42);
    v := mem_load(p);
    return v;
}
