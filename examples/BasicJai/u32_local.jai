// Scalar u32 in a non-main function must not smash saved rbp (8-byte slot).
// Expected exit: 1

set_u32 :: () -> int {
    a: u32;
    a = 1;
    return a;
}

main :: () -> int {
    return set_u32();
}
