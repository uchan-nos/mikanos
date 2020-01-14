bits 64
section .text

%macro define_syscall 2
global Syscall%1
Syscall%1:
    mov rax, %2
    mov r10, rcx
    syscall
    ret
%endmacro

define_syscall LogString,  0x80000000
define_syscall PutString,  0x80000001
define_syscall Exit,       0x80000002
define_syscall OpenWindow, 0x80000003
