ENTRY(_start)

/* Define explicit program headers using numeric literals to maintain cross-version compatibility */
PHDRS
{
    none 0;               /* PT_NONE (0) - Prevents linker from injecting unwanted metadata at the front */
    text 1 FLAGS(5);      /* PT_LOAD (1) - Read and Execute (1 + 4) */
    data 1 FLAGS(6);      /* PT_LOAD (1) - Read and Write (2 + 4) */
}

SECTIONS
{
    /* Begin the kernel at the standard 1MB mark */
    . = 1M;
    
    _kernel_start = .;
    
    /* Ensure the text section is perfectly aligned on a 4KB boundary */
    . = ALIGN(4K);
    .text : AT(ADDR(.text))
    {
        *(.multiboot)   /* FORCES the multiboot header to the absolute beginning of the file */
        *(.text)
        *(.text.*)      /* Captures any compiler-generated text sub-sections */
    } :text             /* Assigns this section to the text program header */
    
    /* Read-only data section */
    . = ALIGN(4K);
    .rodata : AT(ADDR(.rodata))
    {
        *(.rodata)
        *(.rodata.*)
    } :data             /* Grouped into data header */
    
    /* Read-write initialized data section */
    . = ALIGN(4K);
    .data : AT(ADDR(.data))
    {
        *(.data)
        *(.data.*)
    } :data             /* Grouped into data header */
    
    /* Read-write uninitialized data section (stack, heap variables) */
    . = ALIGN(4K);
    .bss : AT(ADDR(.bss))
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    } :data             /* Grouped into data header */
    
    /* Discard compiler notes and annotations that can confuse GRUB's parser */
    /DISCARD/ :
    {
        *(.note)
        *(.note.*)
        *(.comment)
        *(.eh_frame)
    }
    
    /* Align the final marker to capture the exact size cleanly */
    . = ALIGN(4K);
    _kernel_end = .;
}
