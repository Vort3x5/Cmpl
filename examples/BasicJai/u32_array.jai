// [N]u32: stride and element store/load are 4 bytes.
// Expected exit: 7

main :: () -> int {
    arr: [4]u32;
    arr[1] = 7;
    arr[0] = 1;
    return arr[1];
}
