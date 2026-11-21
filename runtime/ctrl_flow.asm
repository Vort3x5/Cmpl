; asmsyntax=fasm

; Condition must already be in rax before _BeginIf / _WhileTest.

macro _BeginIf
{
    local ..if_else, ..if_end
    if_label_else equ ..if_else
    if_label_end equ ..if_end
    if_else_used = 0
    
    test rax, rax
    jz ..if_else
}

macro _Else
{
    if_else_used = 1
    jmp if_label_end
    if_label_else:
}

macro _EndIf
{
    if if_else_used = 0
        if_label_else:
    end if
    if_label_end:
    restore if_else_used
    restore if_label_else
    restore if_label_end
}

macro _BeginWhile
{
    local ..while_start, ..while_end
    while_label_start equ ..while_start
    while_label_end equ ..while_end
    
    ..while_start:
}

macro _WhileTest
{
    test rax, rax
    jz while_label_end
}

macro _EndWhile
{
    jmp while_label_start
    while_label_end:
    restore while_label_start
    restore while_label_end
}

macro _Return expr 
{
    common
    expr
    mov rsp, rbp
    pop rbp
    ret
}

macro _ReturnRax
{
    mov rsp, rbp
    pop rbp
    ret
}
