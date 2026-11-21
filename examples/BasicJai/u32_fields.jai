// Sized u32 field store/load (not 8-byte stomps).
// Expected exit: 5

S :: struct {
    a: u32;
    b: u32;
}

main :: () -> int {
    s: S;
    s.b = 5;
    s.a = 1;
    return s.b;
}
