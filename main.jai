// Test 1: Basic arithmetic (15)
test_arithmetic :: () -> int {
    a := 10;
    b := 5;
    c := a + b;
    return c;
}

// Test 2: Multiple operations (50)
test_operations :: () -> int {
    a := 10;
    b := 5;
    c := a - b;
    d := c * 10;
    return d;
}

// Test 3: If statement (100)
test_if :: () -> int {
    x := 20;
    if x > 10 {
        return 100;
    }
    return 0;
}

// Test 4: If-else (42)
test_if_else :: () -> int {
    x := 5;
    if x > 10 {
        return 10;
    } else {
        return 42;
    }
}

// Test 5: While loop sum 1..10 (55)
test_while :: () -> int {
    sum := 0;
    i := 1;
    while i < 11 {
        sum = sum + i;
        i = i + 1;
    }
    return sum;
}

// Test 6: Comparison operators (3)
test_comparisons :: () -> int {
    result := 0;
    
    if 10 > 5 {
        result = result + 1;
    }
    
    if 5 < 10 {
        result = result + 1;
    }
    
    if 7 == 7 {
        result = result + 1;
    }
    
    return result;
}

// Test 7: Nested loops (25)
test_nested_loops :: () -> int {
    sum := 0;
    i := 0;
    
    while i < 5 {
        j := 0;
        while j < 5 {
            sum = sum + 1;
            j = j + 1;
        }
        i = i + 1;
    }
    
    return sum;
}

// Test 8: Function with parameters (30)
add :: (a: int, b: int) -> int {
    return a + b;
}

test_function_params :: () -> int {
    x := add(10, 20);
    return x;
}

// Test 9: Multiple function calls (120)
multiply :: (a: int, b: int) -> int {
    result := 0;
    i := 0;
    while i < b {
        result = result + a;
        i = i + 1;
    }
    return result;
}

test_multiple_calls :: () -> int {
    a := multiply(5, 6);
    b := multiply(3, 10);
    c := multiply(2, 15);
    return a + b + c;
}

// Test 10: Complex expressions (150)
test_complex_expr :: () -> int {
    a := 5;
    b := 10;
    c := 3;
    
    result := a * b + c * 10 + b * c;
    return result;
}

// Test 11: Nested conditionals (200)
max :: (a: int, b: int) -> int {
    if a > b {
        return a;
    } else {
        return b;
    }
}

test_nested_conditionals :: () -> int {
    x := 50;
    y := 75;
    z := 100;
    
    m1 := max(x, y);
    m2 := max(m1, z);
    m3 := max(m2, 200);
    
    return m3;
}

// Test 12: Factorial-like (120)
factorial :: (n: int) -> int {
    result := 1;
    i := 1;
    
    while i < n + 1 {
        result = result * i;
        i = i + 1;
    }
    
    return result;
}

test_factorial :: () -> int {
    return factorial(5);
}

// Test 13: Fibonacci-like (89)
fib :: (n: int) -> int {
    if n < 2 {
        return n;
    }
    
    a := 0;
    b := 1;
    i := 2;
    
    while i < n + 1 {
        temp := a + b;
        a = b;
        b = temp;
        i = i + 1;
    }
    
    return b;
}

test_fib :: () -> int {
    return fib(11);
}

// Test 14: Loop with conditional (36)
test_loop_conditional :: () -> int {
    sum := 0;
    i := 0;
    
    while i < 10 {
        if i > 5 {
            sum = sum + i;
        }
        i = i + 1;
    }
    
    return sum;
}

// Test 15: Assignment chains (60)
test_assignments :: () -> int {
    a := 10;
    b := a;
    c := b;
    d := c;
    e := d;
    f := e;
    return a + b + c + d + e + f;
}

// Main test runner
main :: () -> int {
    t1 := test_arithmetic();           // 15
    t2 := test_operations();           // 50
    t3 := test_if();                   // 100
    t4 := test_if_else();              // 42
    t5 := test_while();                // 55
    t6 := test_comparisons();          // 3
    t7 := test_nested_loops();         // 25
    t8 := test_function_params();      // 30
    t9 := test_multiple_calls();       // 120
    t10 := test_complex_expr();        // 150
    t11 := test_nested_conditionals(); // 200
    t12 := test_factorial();           // 120
    t13 := test_fib();                 // 89
    t14 := test_loop_conditional();    // 36
    t15 := test_assignments();         // 60
    
    total := t1 + t2;
    total = total + t3;
    total = total + t4;
    total = total + t5;
    total = total + t6;
    total = total + t7;
    total = total + t8;
    total = total + t9;
    total = total + t10;
    total = total + t11;
    total = total + t12;
    total = total + t13;
    total = total + t14;
    total = total + t15;
    
    return total;  // Expected: 1095
}
