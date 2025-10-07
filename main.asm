; asmsyntax=fasm

include 'runtime/core.asm'

_FuncBeginWithLocals main, 24
    _DeclareVar i, 8
    _DeclareVar sum, 8
    
    _Assign i, _Num 0
    _Assign sum, _Num 0
    
    _While _Sub _Num 12, _Var i, <\
        _Assign sum, _Add _Var sum, _Num 1,\
        _Assign i, _Add _Var i, _Num 1>
    
    _LoadVar rax, sum
_FuncEnd
