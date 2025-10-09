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

