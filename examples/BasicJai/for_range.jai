// Sum 0..10 via for-range — Jai ranges are inclusive (expected 55)
main :: () -> int {
    sum := 0;
    for i: 0..10 {
        sum = sum + i;
    }
    return sum;
}
