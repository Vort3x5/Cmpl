; asmsyntax=fasm
; Minimal OS FFI via syscalls (cmpl_rt). Real .so linking comes next.
;
;   sys_write(fd, buf, n) -> bytes_written
;   msg_hi_ptr() / msg_hi_len() — demo helpers

sys_write:
    push rbp
    mov rbp, rsp
    mov rax, 1          ; SYS_write
    syscall
    mov rsp, rbp
    pop rbp
    ret

msg_hi: db 'Hi', 10
msg_hi_len_val = $ - msg_hi

msg_hi_ptr:
    push rbp
    mov rbp, rsp
    mov rax, msg_hi
    mov rsp, rbp
    pop rbp
    ret

msg_hi_len:
    push rbp
    mov rbp, rsp
    mov rax, msg_hi_len_val
    mov rsp, rbp
    pop rbp
    ret
