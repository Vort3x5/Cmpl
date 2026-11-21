; asmsyntax=fasm
; Bump arena (cmpl_rt). System V calling convention.
;   arena_reset()
;   arena_alloc(size) -> ptr
;   mem_store(addr, val)
;   mem_load(addr) -> val

arena_reset:
    push rbp
    mov rbp, rsp
    mov qword [arena_cursor], arena_mem
    mov rsp, rbp
    pop rbp
    ret

arena_alloc:
    push rbp
    mov rbp, rsp
    ; rdi = size, 8-byte align
    add rdi, 7
    and rdi, not 7
    mov rax, [arena_cursor]
    mov rbx, rax
    add rbx, rdi
    mov rcx, arena_mem + arena_mem_size
    cmp rbx, rcx
    ja .oom
    mov [arena_cursor], rbx
    mov rsp, rbp
    pop rbp
    ret
.oom:
    xor rax, rax
    mov rsp, rbp
    pop rbp
    ret

mem_store:
    push rbp
    mov rbp, rsp
    mov [rdi], rsi
    mov rax, rsi
    mov rsp, rbp
    pop rbp
    ret

mem_load:
    push rbp
    mov rbp, rsp
    mov rax, [rdi]
    mov rsp, rbp
    pop rbp
    ret

align 16
arena_mem_size = 65536
arena_mem: rb arena_mem_size
arena_cursor: dq arena_mem
