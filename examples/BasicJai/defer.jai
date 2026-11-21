// Milestone 2: defer (LIFO before return / scope end)
// Expected exit: 3
// Flow: x=1; defer x=x+2; defer x=x+10 would run +10 then +2 if we returned x after,
// but we return a snapshot. Here: set x, defer increments, return x after defers via:
//   x := 0; defer x = x + 1; defer x = x + 2; return x
// Defers run before return with saved return value — so return 0, defers bump x unused.
// Better test: mutate through side effect on a var we read after inner block.

main :: () -> int {
    x := 0;
    {
        defer x = x + 1;
        defer x = x + 2;
    }
    // After block: +2 then +1 => x == 3
    return x;
}
