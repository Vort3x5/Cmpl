; asmsyntax=fasm
; Relocatable object linked with `cc` (CRT provides _start; we provide main).

format ELF64

section '.text' executable writeable
public main

include 'runtime/macros.asm'

; int main(void) — return value in eax for CRT
main:
    push rbp
    mov rbp, rsp
    call func_main
    mov rsp, rbp
    pop rbp
    ret
