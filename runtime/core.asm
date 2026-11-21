; asmsyntax=fasm
; Standalone executable (cmpl_rt embedded — no external .so)

format ELF64 executable 3
entry _start

include 'runtime/macros.asm'

_start:
    call func_main
	mov rdi, rax
	_SysExit rdi
