; asmsyntax=fasm

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
    left
    push rax
    right
    mov rbx, rax
    pop rax
    add rax, rbx
}

macro _Sub left, right 
{
    left
    push rax
    right
    mov rbx, rax
    pop rax
    sub rax, rbx
}

macro _Mul left, right 
{
    left
    push rax
    right
    mov rbx, rax
    pop rax
    imul rax, rbx
}

macro _Assign var_name, [expr] 
{
	forward
    expr
	forward
    _StoreVar var_name, rax
}
