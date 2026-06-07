#include "pit.h"

#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL_0    0x40

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static volatile uint32_t pit_ticks = 0;

void pit_init(void) {
    // Configure PIT Channel 0: Square Wave Mode, Low/High Byte, 1000Hz frequency
    // 1193182 Hz / 1000 Hz = 1193 divisor
    uint16_t divisor = 1193;
    outb(PIT_COMMAND_PORT, 0x36);
    outb(PIT_CHANNEL_0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL_0, (uint8_t)((divisor >> 8) & 0xFF));
}

// Simple microsecond sleep routine using raw x86 I/O bus delay loops
// Since the x86 I/O bus port 0x80 takes roughly 1 microsecond to execute,
// we can use it as a reliable clock step on modern bare-metal and emulators.
void delay_us(uint32_t us) {
    for (uint32_t i = 0; i < us; i++) {
        // Port 0x80 is an unused vintage POST checklist port. 
        // Writing to it forces a 1-microsecond CPU synchronization pause.
        outb(0x80, 0x00);
    }
}
