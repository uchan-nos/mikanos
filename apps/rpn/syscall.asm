bits 64
section .text

global SyscallLogString
SyscallLogString:
    mov eax, 0x80000000
    mov r10, rcx
    syscall
    ret
