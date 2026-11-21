// Milestone 2: fixed arrays [N]Type
// Expected exit: 30  (10+20)

main :: () -> int {
    arr: [4]int;
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 0;
    arr[3] = 0;
    return arr[0] + arr[1];
}
