; asmfunc.asm
;
; System V AMD64 Calling Convention
; Registers: RDI, RSI, RDX, RCX, R8, R9

bits 64
section .text

global IoOut32  ; void IoOut32(uint16_t addr, uint32_t data);
IoOut32:
    mov dx, di    ; dx = addr
    mov eax, esi  ; eax = data
    out dx, eax
    ret

global IoIn32  ; uint32_t IoIn32(uint16_t addr);
IoIn32:
    mov dx, di    ; dx = addr
    in eax, dx
    ret

global GetCS  ; uint16_t GetCS(void);
GetCS:
    xor eax, eax  ; also clears upper 32 bits of rax
    mov ax, cs
    ret

global LoadIDT  ; void LoadIDT(uint16_t limit, uint64_t offset);
LoadIDT:
    push rbp
    mov rbp, rsp
    sub rsp, 10
    mov [rsp], di  ; limit
    mov [rsp + 2], rsi  ; offset
    lidt [rsp]
    mov rsp, rbp
    pop rbp
    ret

global LoadGDT  ; void LoadGDT(uint16_t limit, uint64_t offset);
LoadGDT:
    push rbp
    mov rbp, rsp
    sub rsp, 10
    mov [rsp], di  ; limit
    mov [rsp + 2], rsi  ; offset
    lgdt [rsp]
    mov rsp, rbp
    pop rbp
    ret

global SetCSSS  ; void SetCSSS(uint16_t cs, uint16_t ss);
SetCSSS:
    push rbp
    mov rbp, rsp
    mov ss, si
    mov rax, .next
    push rdi    ; CS
    push rax    ; RIP
    o64 retf
.next:
    mov rsp, rbp
    pop rbp
    ret

global SetDSAll  ; void SetDSAll(uint16_t value);
SetDSAll:
    mov ds, di
    mov es, di
    mov fs, di
    mov gs, di
    ret

global SetCR3  ; void SetCR3(uint64_t value);
SetCR3:
    mov cr3, rdi
    ret

extern kernel_main_stack
extern KernelMainNewStack

global KernelMain
KernelMain:
    mov rsp, kernel_main_stack + 1024 * 1024
    call KernelMainNewStack
.fin:
    hlt
    jmp .fin

#@range_begin(switch_context)
global SwitchContext
SwitchContext:  ; void SwitchContext(CPURegisters* to, CPURegisters* current);
    mov [rsi +  2*8], rax
    mov [rsi +  3*8], rbx
    mov [rsi +  4*8], rcx
    mov [rsi +  5*8], rdx
    mov [rsi +  6*8], rdi
    mov [rsi +  7*8], rsi
    mov [rsi +  8*8], rbp
    mov [rsi +  9*8], rsp
    mov [rsi + 10*8], r8
    mov [rsi + 11*8], r9
    mov [rsi + 12*8], r10
    mov [rsi + 13*8], r11
    mov [rsi + 14*8], r12
    mov [rsi + 15*8], r13
    mov [rsi + 16*8], r14
    mov [rsi + 17*8], r15

    mov rax, [rdi +  2*8]
    mov rbx, [rdi +  3*8]
    mov rcx, [rdi +  4*8]
    mov rdx, [rdi +  5*8]
    mov rsi, [rdi +  7*8]
    mov rbp, [rdi +  8*8]
    mov rsp, [rdi +  9*8]
    mov r8,  [rdi + 10*8]
    mov r9,  [rdi + 11*8]
    mov r10, [rdi + 12*8]
    mov r11, [rdi + 13*8]
    mov r12, [rdi + 14*8]
    mov r13, [rdi + 15*8]
    mov r14, [rdi + 16*8]
    mov r15, [rdi + 17*8]

    mov rdi, [rdi +  6*8]
    ret
#@range_end(switch_context)
