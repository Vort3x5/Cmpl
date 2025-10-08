main :: () {
    x := 10;
    y := 20;
    sum := x + y;
    result := sum * 2;
}

factorial :: () {
    n := 5;
    result := 1;
    i := 1;
    
    while i < n + 1 {
        result := result * i;
        i := i + 1;
    }
    
    return result;
}

test_if :: () {
    a := 17;
    b := 23;
    
    if a > b {
        return a;
    } 
	else {
        return b;
    }
}
