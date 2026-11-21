// for bound from a call — evaluate once into slot (no push).
// fval(2) == 3; inclusive 0..3 → 4 iterations.
// Expected exit: 4

sse :: #library "examples/c_lib/libfval";
fval :: (x: int) -> int #foreign sse;

main :: () -> int {
    s := 0;
    for i: 0..fval(2) {
        s = s + 1;
    }
    return s;
}
