bits 64
section .text

global Syscall
Syscall:
    mov r9, rcx
    syscall
    ret
