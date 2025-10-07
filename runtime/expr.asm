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
    expr
    _StoreVar var_name, rax
}

macro _If condition, [then_block] 
{
    common
    if_counter = if_counter + 1
    
    condition
    test rax, rax
    jz .if_false#if_counter
    
    forward
        then_block
    
    common
    jmp .if_end#if_counter
    
    .if_false#if_counter:
    .if_end#if_counter:
}

macro _BeginWhile condition 
{
    while_counter = while_counter + 1
    .while_start#while_counter:
    condition
    test rax, rax
    jz .while_end#while_counter
}

macro _EndWhile 
{
    jmp .while_start#while_counter
    .while_end#while_counter:
}

macro _Return [expr] 
{
    common
    forward
    expr
    common
    mov rsp, rbp
    pop rbp
    ret
}
