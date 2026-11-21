; asmsyntax=fasm
; Hand-written MacroLang tests (condition in rax, then _BeginIf / _WhileTest)

include '../../runtime/core.asm'

; Test 1: Simple while loop - sum from 1 to 10
_FuncBeginWithLocals func_test_sum, 16
    _DeclareVar i, 8
    _DeclareVar sum, 8
    
    _Assign i, _Num 1
    _Assign sum, _Num 0
    
    _BeginWhile
        _Less <_Var i>, _Num 11
        _WhileTest
        _Assign sum, <_Add <_Var sum>, <_Var i>>
        _Assign i, <_Add <_Var i>, _Num 1>
    _EndWhile
    
    _Return <_Var sum>
_FuncEnd

; Test 2: If-else - return absolute value
_FuncBeginWithLocals func_test_abs, 8
    _DeclareVar x, 8
    
    _Assign x, _Num -42
    
    _Less <_Var x>, _Num 0
    _BeginIf
        _Assign x, <_Sub _Num 0, <_Var x>>
    _EndIf
    
    _Return <_Var x>
_FuncEnd

; Test 3: Max of two numbers using if-else
_FuncBeginWithLocals func_test_max, 16
    _DeclareVar a, 8
    _DeclareVar b, 8
    
    _Assign a, _Num 17
    _Assign b, _Num 23
    
    _Greater <_Var a>, <_Var b>
    _BeginIf
        _Return <_Var a>
    _Else
        _Return <_Var b>
    _EndIf
_FuncEnd

; Dummy main so this file still links as ELF
_FuncBeginWithLocals func_main, 0
    _Return _Num 0
_FuncEnd
