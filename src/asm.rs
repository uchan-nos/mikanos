use std::arch::global_asm;

#[cfg(not(target_arch = "x86_64"))]
compile_error!("This crate only supports x86_64 architecture");

global_asm!(
    r#"
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
"#r
);
