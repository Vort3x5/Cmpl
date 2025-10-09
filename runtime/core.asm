; asmsyntax=fasm

format ELF64 executable 3
entry _start

include 'runtime/expr.asm'
include 'runtime/ctrl_flow.asm'

macro _SysExit code
{
	mov rax, 60
	mov rdi, code
	syscall
}

macro _FuncBegin name
{
	name:
	    push rbp
	    mov rbp, rsp
}

macro _FuncEnd
{
	mov rsp, rbp
	pop rbp
	ret
}

current_stack_offset = 0

macro _DeclareVar name, size 
{
    current_stack_offset = current_stack_offset + size
    name#_offset = current_stack_offset
}

macro _LoadVar dest, var_name 
{
    mov dest, [rbp - var_name#_offset]
}

macro _StoreVar var_name, src 
{
    mov [rbp - var_name#_offset], src
}

macro _FuncBeginWithLocals name, locals_size 
{
    name:
    push rbp
    mov rbp, rsp
    if locals_size > 0
        sub rsp, locals_size
    end if
    current_stack_offset = 0
}

_start:
    call main
	mov rdi, rax
	_SysExit rdi
