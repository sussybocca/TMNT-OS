[bits 32]

; --- THE MULTIBOOT HEADER (GRUB requirement) - WITH GRAPHICS REQUEST ---
section .multiboot
align 4
    dd 0x1BADB002              ; Magic number for Multiboot 1
    dd 0x07                    ; Flags: align + memory info + graphics mode
    dd -(0x1BADB002 + 0x07)    ; Checksum
    
    ; Graphics mode request (flags bit 2 enables these fields)
    dd 0                       ; header_addr (unused)
    dd 0                       ; load_addr (unused)
    dd 0                       ; load_end_addr (unused)
    dd 0                       ; bss_end_addr (unused)
    dd 0                       ; entry_addr (unused)
    dd 0                       ; mode_type: 0 = use linear graphics mode
    dd 1024                    ; preferred width
    dd 768                     ; preferred height
    dd 32                      ; preferred depth (32-bit BGR)

; --- THE ENTRY POINT ---
section .text
global _start

extern kernel_main

_start:
    ; Set up a safe stack pointer for 32-bit mode operations
    mov esp, stack_space
    
    ; Push multiboot info and magic to pass to kernel_main
    push ebx                   ; multiboot info structure pointer
    push eax                   ; multiboot magic number
    
    ; Clear the screen and print our greeting string
    mov esi, msg_tmnt_boot
    call print_string_32
    
    ; Jump to the C kernel with multiboot info
    call kernel_main
    
    ; If kernel_main returns, halt safely
.hang:
    hlt
    jmp .hang

; --- 32-BIT VGA TEXT MODE PRINTING FUNCTION ---
print_string_32:
    mov edi, 0xB8000
    mov ah, 0x0F
.print_loop:
    lodsb
    or al, al
    jz .done
    mov [edi], ax
    add edi, 2
    jmp .print_loop
.done:
    ret

; --- DATA SECTION ---
section .data
msg_tmnt_boot db 'TMNT OS v1.0 - Cowabunga!', 0

; --- UNINITIALIZED DATA SECTION (STACK SETUP) ---
section .bss
align 4
resb 32768                      ; Reserve 32KB for stack
stack_space: