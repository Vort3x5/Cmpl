// defer with SSE-sensitive call before return (slot, not push).
// Build: cc -shared -fPIC -o examples/c_lib/libfval.so examples/c_lib/fval.c
// Expected exit: 7

sse :: #library "examples/c_lib/libfval";
fval :: (x: int) -> int #foreign sse;

main :: () -> int {
    defer fval(1);
    return 7;
}
