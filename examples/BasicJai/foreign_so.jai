// Real .so via #library (The_Way_to_Jai §29)
// Build lib first:  cc -shared -fPIC -o examples/c_lib/libadd.so examples/c_lib/add.c
// Expected exit: 30

add :: #library "examples/c_lib/libadd.so";

add_ints :: (a: int, b: int) -> int #foreign add;

main :: () -> int {
    return add_ints(10, 20);
}
