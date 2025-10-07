; asmsyntax=fasm

if_counter = 0
while_counter = 0

macro _Num value 
{
    mov rax, value
}

macro _Var name 
{
    _LoadVar rax, name
}

macro _Add left, right 
{
	common
    left
    push rax
    right
    mov rbx, rax
    pop rax
    add rax, rbx
}

macro _Sub left, right 
{
	common
    left
    push rax
    right
    mov rbx, rax
    pop rax
    sub rax, rbx
}

macro _Mul left, right 
{
	common
    left
    push rax
    right
    mov rbx, rax
    pop rax
    imul rax, rbx
}

macro _Assign var_name, expr 
{
	common
    expr
    _StoreVar var_name, rax
}

; Comparison operators
macro _Equal left, right
{
    common
    left
    push rax
    right
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    sete al
}

macro _Less left, right
{
    common
    left
    push rax
    right
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setl al
}

macro _Greater left, right
{
    common
    left
    push rax
    right
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setg al
}

; Block-style control flow macros
macro _BeginIf condition
{
    local ..if_else, ..if_end
    if_label_else equ ..if_else
    if_label_end equ ..if_end
    if_else_used = 0
    
    condition
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

macro _BeginWhile condition
{
    local ..while_start, ..while_end
    while_label_start equ ..while_start
    while_label_end equ ..while_end
    
    ..while_start:
    condition
    test rax, rax
    jz ..while_end
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
