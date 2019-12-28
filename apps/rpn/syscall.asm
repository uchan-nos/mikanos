bits 64
section .text

global SyscallLogString
SyscallLogString:
    mov rax, 0x80000000
    mov r10, rcx
    syscall
    ret

global SyscallPutString
SyscallPutString:
    mov rax, 0x80000001
    mov r10, rcx
    syscall
    ret
