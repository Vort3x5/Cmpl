; asmsyntax=fasm

include 'runtime/core.asm'

_FuncBeginWithLocals main, 24
    _DeclareVar x, 8
    _DeclareVar y, 8
    _DeclareVar result, 8
    
    _Assign x, _Num 10
    _Assign y, _Num 20
    _Assign result, _Add _Var x, _Var y
    
    _LoadVar rax, result
_FuncEnd
