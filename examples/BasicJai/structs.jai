// Milestone 2: structs + field access
// Expected exit: 42  (10 + 32)

Point :: struct {
    x: int;
    y: int;
}

main :: () -> int {
    p: Point;
    p.x = 10;
    p.y = 32;
    return p.x + p.y;
}
