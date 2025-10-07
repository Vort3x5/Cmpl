; asmsyntax=fasm

include '../../runtime/core.asm'

; Test 1: Simple while loop - sum from 1 to 10
_FuncBeginWithLocals test_sum, 16
    _DeclareVar i, 8
    _DeclareVar sum, 8
    
    _Assign i, _Num 1
    _Assign sum, _Num 0
    
    _BeginWhile <_Less <_Var i>, _Num 11>
        _Assign sum, <_Add <_Var sum>, <_Var i>>
        _Assign i, <_Add <_Var i>, _Num 1>
    _EndWhile
    
    _Return <_Var sum>
_FuncEnd

; Test 2: If-else - return absolute value
_FuncBeginWithLocals test_abs, 8
    _DeclareVar x, 8
    
    _Assign x, _Num -42
    
    _BeginIf <_Less <_Var x>, _Num 0>
        _Assign x, <_Sub _Num 0, <_Var x>>
    _EndIf
    
    _Return <_Var x>
_FuncEnd

; Test 3: Max of two numbers using if-else
_FuncBeginWithLocals test_max, 16
    _DeclareVar a, 8
    _DeclareVar b, 8
    
    _Assign a, _Num 17
    _Assign b, _Num 23
    
    _BeginIf <_Greater <_Var a>, <_Var b>>
        _Return <_Var a>
    _Else
        _Return <_Var b>
    _EndIf
_FuncEnd

; Test 4: Factorial (nested control flow)
_FuncBeginWithLocals test_factorial, 24
    _DeclareVar n, 8
    _DeclareVar result, 8
    _DeclareVar i, 8
    
    _Assign n, _Num 5
    _Assign result, _Num 1
    _Assign i, _Num 1
    
    _BeginWhile <_Less <_Var i>, <_Add <_Var n>, _Num 1>>
        _Assign result, <_Mul <_Var result>, <_Var i>>
        _Assign i, <_Add <_Var i>, _Num 1>
    _EndWhile
    
    _Return <_Var result>
_FuncEnd

; Test 5: Early return in loop
_FuncBeginWithLocals test_early_return, 24
    _DeclareVar i, 8
    _DeclareVar sum, 8
    _DeclareVar limit, 8
    
    _Assign i, _Num 0
    _Assign sum, _Num 0
    _Assign limit, _Num 50
    
    _BeginWhile <_Less <_Var i>, _Num 100>
        _Assign sum, <_Add <_Var sum>, <_Var i>>
        
        ; Early return if sum exceeds limit
        _BeginIf <_Greater <_Var sum>, <_Var limit>>
            _Return <_Var sum>
        _EndIf
        
        _Assign i, <_Add <_Var i>, _Num 1>
    _EndWhile
    
    _Return <_Var sum>
_FuncEnd

; Test 6: Nested loops (multiplication table row)
_FuncBeginWithLocals test_nested_loops, 24
    _DeclareVar i, 8
    _DeclareVar j, 8
    _DeclareVar sum, 8
    
    _Assign i, _Num 1
    _Assign sum, _Num 0
    
    _BeginWhile <_Less <_Var i>, _Num 4>
        _Assign j, _Num 1
        
        _BeginWhile <_Less <_Var j>, _Num 4>
            _Assign sum, <_Add <_Var sum>, <_Mul <_Var i>, <_Var j>>>
            _Assign j, <_Add <_Var j>, _Num 1>
        _EndWhile
        
        _Assign i, <_Add <_Var i>, _Num 1>
    _EndWhile
    
    _Return <_Var sum>
_FuncEnd

; Main function - runs all tests
_FuncBeginWithLocals main, 8
    _DeclareVar result, 8
    
    ; Test 1: sum 1..10 = 55
    call test_sum
    _StoreVar result, rax
    
    ; Test 2: abs(-42) = 42
    call test_abs
    _StoreVar result, rax
    
    ; Test 3: max(17, 23) = 23
    call test_max
    _StoreVar result, rax
    
    ; Test 4: factorial(5) = 120
    call test_factorial
    _StoreVar result, rax
    
    ; Test 5: early return test
    call test_early_return
    _StoreVar result, rax
    
    ; Test 6: nested loops = 36
    call test_nested_loops
    
_FuncEnd
