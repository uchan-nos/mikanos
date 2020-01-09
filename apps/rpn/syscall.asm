bits 64
section .text

global SyscallLogString
SyscallLogString:
    mov eax, 0x80000000
    mov r10, rcx
    syscall
    ret

; #@@range_begin(put_string_wrapper)
global SyscallPutString
SyscallPutString:
    mov eax, 0x80000001
    mov r10, rcx
    syscall
    ret
; #@@range_end(put_string_wrapper)
