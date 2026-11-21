// Regression: call inside expression must keep RSP 16-byte aligned (SSE ABI).
// Build: cc -shared -fPIC -o examples/c_lib/libfval.so examples/c_lib/fval.c
// Expected exit: 4  (1 + fval(2) == 1+3)

sse :: #library "examples/c_lib/libfval";

fval :: (x: int) -> int #foreign sse;

main :: () -> int {
    x := 1 + fval(2);
    return x;
}
