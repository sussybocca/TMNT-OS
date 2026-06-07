#include "sb16.h"
#include "pit.h"
#include "../system/string.h"

#define SB16_BASE        0x220
#define SB16_MIXER       0x224
#define SB16_MIXER_DATA  0x225
#define SB16_RESET       0x226
#define SB16_READ        0x22A
#define SB16_WRITE       0x22C
#define SB16_READ_STATUS 0x22E

// DMA buffer MUST be in low memory (below 16MB) for ISA DMA controller
static uint8_t dma_buffer[65536] __attribute__((aligned(65536), section(".dma_buffer")));

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void sb16_write_dsp(uint8_t value) {
    while(inb(SB16_WRITE) & 0x80);
    outb(SB16_WRITE, value);
}

static uint8_t sb16_read_dsp(void) {
    while(!(inb(SB16_READ_STATUS) & 0x80));
    return inb(SB16_READ);
}

static int sb16_reset_dsp(void) {
    outb(SB16_RESET, 1);
    for(volatile int i = 0; i < 65536; i++) asm volatile("nop");
    outb(SB16_RESET, 0);
    
    if(sb16_read_dsp() != 0xAA) return -1;
    return 0;
}

void sb16_init(void) {
    sb16_reset_dsp();
    outb(SB16_MIXER, 0x00);
    outb(SB16_MIXER_DATA, 0x00);
    outb(SB16_MIXER, 0x22);
    outb(SB16_MIXER_DATA, 0xFF);
    outb(SB16_MIXER, 0x04);
    outb(SB16_MIXER_DATA, 0xFF);
}

static int sb16_playing = 0;

void sb16_play_pcm(uint8_t* data, uint32_t size, uint16_t sample_rate) {
    if(sb16_reset_dsp() != 0) return;
    
    sb16_playing = 1;
    
    sb16_write_dsp(0x41);
    sb16_write_dsp((uint8_t)(sample_rate >> 8));
    sb16_write_dsp((uint8_t)(sample_rate & 0xFF));
    
    uint32_t remaining = size;
    uint32_t offset = 0;
    
    sb16_write_dsp(0xD1);
    
    while(remaining > 0) {
        if(!sb16_playing) break;

        uint32_t chunk = remaining;
        if(chunk > 65535) chunk = 65535;
        
        for(uint32_t i = 0; i < chunk; i++) {
            dma_buffer[i] = data[offset + i];
        }
        
        uint32_t phys = (uint32_t)dma_buffer;
        uint8_t page = (phys >> 16) & 0xFF;
        uint16_t addr_offset = phys & 0xFFFF;
        uint16_t count = (chunk - 1) & 0xFFFF;
        
        outb(0x0A, 0x05);
        outb(0x0C, 0x00);
        outb(0x0B, 0x49);
        outb(0x02, addr_offset & 0xFF);
        outb(0x02, (addr_offset >> 8) & 0xFF);
        outb(0x83, page);
        outb(0x03, count & 0xFF);
        outb(0x03, (count >> 8) & 0xFF);
        outb(0x0A, 0x01);
        
        sb16_write_dsp(0x14);
        sb16_write_dsp(count & 0xFF);
        sb16_write_dsp((uint8_t)(count >> 8));
        
        // Wait for DMA to complete - poll DSP read status
        while(!(inb(SB16_READ_STATUS) & 0x80));
        inb(SB16_READ);
        
        offset += chunk;
        remaining -= chunk;
    }
    
    sb16_write_dsp(0xD3);
    sb16_playing = 0;
}

void sb16_play_pcm_single(uint8_t* data, uint32_t size, uint16_t sample_rate) {
    if(sb16_reset_dsp() != 0) return;
    
    sb16_playing = 1;
    
    sb16_write_dsp(0x41);
    sb16_write_dsp((uint8_t)(sample_rate >> 8));
    sb16_write_dsp((uint8_t)(sample_rate & 0xFF));
    
    // JUST PLAY THE FIRST 65535 BYTES - SINGLE CHUNK, NO LOOP
    uint32_t chunk = size;
    if(chunk > 65535) chunk = 65535;
    
    for(uint32_t i = 0; i < chunk; i++) {
        dma_buffer[i] = data[i];
    }
    
    uint32_t phys = (uint32_t)dma_buffer;
    uint8_t page = (phys >> 16) & 0xFF;
    uint16_t addr_offset = phys & 0xFFFF;
    uint16_t count = (chunk - 1) & 0xFFFF;
    
    outb(0x0A, 0x05);
    outb(0x0C, 0x00);
    outb(0x0B, 0x49);
    outb(0x02, addr_offset & 0xFF);
    outb(0x02, (addr_offset >> 8) & 0xFF);
    outb(0x83, page);
    outb(0x03, count & 0xFF);
    outb(0x03, (count >> 8) & 0xFF);
    outb(0x0A, 0x01);
    
    sb16_write_dsp(0xD1);
    sb16_write_dsp(0x14);
    sb16_write_dsp(count & 0xFF);
    sb16_write_dsp((uint8_t)(count >> 8));
    
    // Wait for DSP to signal completion
    while(!(inb(SB16_READ_STATUS) & 0x80));
    inb(SB16_READ);
    
    sb16_write_dsp(0xD3);
    sb16_playing = 0;
}

void sb16_stop(void) {
    sb16_write_dsp(0xD3);
    outb(0x0A, 0x05);
    sb16_playing = 0;
}

int sb16_is_playing(void) {
    return sb16_playing;
}