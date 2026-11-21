// Jai pointers: *Type, *addr, .* deref  (The_Way_to_Jai §10)
// Expected exit: 108

main :: () -> int {
    a: int;
    a = 42;
    b: *int;
    b = *a;
    b.* = 108;
    return b.*;
}
