test_arithmetic :: () -> int {
    a := 10;
    b := 5;
    c := a + b;
    return c;
}

test_while :: () -> int {
    sum := 0;
    i := 1;
    
    while i < 11 {
        sum = sum + i;
        i = i + 1;
    }
    
    return sum;
}

main :: () -> int {
    t1 := test_arithmetic();
    t2 := test_while();
    return t1 + t2;
}
