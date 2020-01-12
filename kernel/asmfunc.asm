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

global GetCR3  ; uint64_t GetCR3();
GetCR3:
    mov rax, cr3
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

global SwitchContext
SwitchContext:  ; void SwitchContext(uint64_t* to_rsp, uint64_t* current_rsp);
    push rax
    push rbx
    push rcx
    push rdx
    push rdi
    push rsi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov [rsi], rsp
    mov rsp, [rdi]

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ret

global CallApp
CallApp:  ; int CallApp(int argc, char** argv, uint16_t ss, uint64_t rip, uint64_t rsp, uint64_t* os_stack_ptr);
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    mov [r9], rsp ; OS 用のスタックポインタを保存

    push rdx  ; SS
    push r8   ; RSP
    add rdx, 8
    push rdx  ; CS
    push rcx  ; RIP
    o64 retf
    ; アプリケーションが終了してもここには来ない

global WriteMSR
WriteMSR:  ; void WriteMSR(uint32_t msr, uint64_t value);
    mov rdx, rsi
    shr rdx, 32
    mov eax, esi
    mov ecx, edi
    wrmsr
    ret

extern syscall_table
global SyscallEntry
SyscallEntry:  ; void SyscallEntry(void);
    push rbp
    push rcx  ; original RIP
    push r11  ; original RFLAGS

    push rax  ; システムコール番号を保存

    mov rcx, r10
    and eax, 0x7fffffff
    mov rbp, rsp
    and rsp, 0xfffffffffffffff0

    call [syscall_table + 8 * eax]
    ; rbx, r12-r15 は callee-saved なので呼び出し側で保存しない
    ; rax は戻り値用なので呼び出し側で保存しない

    mov rsp, rbp

    pop rsi  ; システムコール番号を復帰
    cmp esi, 0x80000002
    je  .exit

    pop r11
    pop rcx
    pop rbp
    o64 sysret

.exit
    mov rsp, rax
    mov eax, edx

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ret  ; CallApp の次の行に飛ぶ
