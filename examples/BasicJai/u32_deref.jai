// *u32 store must not overwrite the next field/word.
// Expected exit: 42

Pair :: struct {
    a: u32;
    b: u32;
}

main :: () -> int {
    p: Pair;
    p.a = 0;
    p.b = 42;
    q: *u32;
    q = *p;       // address of first field (same as &p.a layout-wise via *p)
    q.* = 1;
    return p.b;
}
