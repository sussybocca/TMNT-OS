#include "idt.h"
#include "../drivers/keyboard/keyboard.h"
#include "../drivers/vga/vga.h"

static void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

void irq_handler(int int_num) {
    // Send EOI to PIC
    if(int_num >= 40) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
    
    // Handle keyboard (IRQ1 = interrupt 33)
    if(int_num == 33) {
        keyboard_handler();
    }
}

void fault_handler(void* esp) {
    vga_set_fg_color(VGA_COLOR_LIGHT_RED);
    vga_print("\n!!! SYSTEM FAULT !!!\n");
    vga_print("System halted.\n");
    while(1) asm volatile("hlt");
}