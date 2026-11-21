// Nested call args must not clobber registers (TAC_PARAM after all temps).
// Expected exit: 6

add :: (a: int, b: int) -> int {
    return a + b;
}

main :: () -> int {
    return add(1, add(2, 3));
}
