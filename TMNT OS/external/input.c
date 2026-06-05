#include "input.h"
#include "gui.h"
#include "../drivers/vga/vga.h"
#include "../system/string.h"

// from kernel
extern uint32_t* fb;
extern uint32_t fb_width, fb_height, fb_pitch;

int mouse_x = 512, mouse_y = 384;
int mouse_btn_left = 0, mouse_btn_old_left = 0;

static const char scancode_to_ascii[] = {
    0,0,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,
    '\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,'\n'
};

static inline uint8_t inb(uint16_t port) { uint8_t r; asm volatile("inb %1,%0":"=a"(r):"Nd"(port)); return r; }
static inline void outb(uint16_t port, uint8_t v) { asm volatile("outb %0,%1"::"a"(v),"Nd"(port)); }

char keyboard_getchar_poll(void) {
    static int e0_prefix = 0;
    while(1) {
        uint8_t s = inb(0x64);
        if(s & 0x20) {
            static int mcycle = 0; static uint8_t mbuf[3];
            mbuf[mcycle++] = inb(0x60);
            if(mcycle == 3) {
                mcycle = 0;
                int dx = mbuf[1], dy = mbuf[2];
                if(mbuf[0] & 0x10) dx -= 256;
                if(mbuf[0] & 0x20) dy -= 256;
                int btn_now = (mbuf[0] & 0x01) ? 1 : 0;
                if(mouse_btn_old_left == 0 && btn_now == 1)
                    gui_handle_click();
                mouse_btn_old_left = btn_now;
                if(gui_mode) gui_restore_cursor_bg();
                mouse_x += dx; mouse_y -= dy;
                if(mouse_x < 0) mouse_x = 0; if(mouse_y < 0) mouse_y = 0;
                if(mouse_x >= (int)fb_width) mouse_x = fb_width - 1;
                if(mouse_y >= (int)fb_height) mouse_y = fb_height - 1;
                if(gui_mode) { gui_save_cursor_bg(); gui_draw_cursor(); }
            }
            continue;
        }
        if(s & 0x01) {
            uint8_t sc = inb(0x60);
            if(sc == 0xE0) { e0_prefix = 1; continue; }
            if(sc & 0x80) { e0_prefix = 0; continue; }
            if(e0_prefix && sc == 0x1C) { e0_prefix = 0; return '\n'; }
            e0_prefix = 0;
            if(sc >= sizeof(scancode_to_ascii)) continue;
            char c = scancode_to_ascii[sc];
            if(c == 0) continue;
            return c;
        }
    }
}

char* keyboard_readline_poll(void) {
    static char buf[256]; int i = 0;
    while(1) {
        char c = keyboard_getchar_poll();
        if(c == '\n') { buf[i] = '\0'; vga_putchar('\n'); return buf; }
        if(c == '\b') { if(i>0){ i--; vga_putchar('\b'); vga_putchar(' '); vga_putchar('\b'); } continue; }
        if(c >= ' ' && i < 254) { buf[i++] = c; vga_putchar(c); }
    }
}

char* keyboard_read_multiline(void) {
    static char buf[4096]; int pos = 0;
    vga_print("(Type '.' on empty line to finish)\n");
    while(pos < 4090) {
        vga_print("> ");
        char* line = keyboard_readline_poll();
        if(line[0] == '.' && line[1] == '\0') break;
        int j = 0; while(line[j]) buf[pos++] = line[j++];
        buf[pos++] = '\n';
    }
    buf[pos] = '\0'; return buf;
}

int keyboard_getint_poll(void) {
    char buf[32]; int i = 0;
    while(1) {
        char c = keyboard_getchar_poll();
        if(c == '\n') break;
        if(c == '\b' && i > 0) { i--; vga_putchar('\b'); }
        else if((c >= '0' && c <= '9') || (c == '-' && i == 0)) { buf[i++] = c; vga_putchar(c); }
    }
    buf[i] = '\0'; vga_putchar('\n');
    int result = 0, sign = 1, j = 0;
    if(buf[0] == '-') { sign = -1; j = 1; }
    while(buf[j]) { result = result * 10 + (buf[j] - '0'); j++; }
    return result * sign;
}

void mouse_init(void) {
    while(inb(0x64) & 0x02); outb(0x64, 0xA8);
    while(inb(0x64) & 0x02); outb(0x64, 0x20);
    while(!(inb(0x64) & 0x01));
    uint8_t cfg = inb(0x60); cfg |= 0x02; cfg &= ~0x20;
    while(inb(0x64) & 0x02); outb(0x64, 0x60);
    while(inb(0x64) & 0x02); outb(0x60, cfg);
    while(inb(0x64) & 0x02); outb(0x64, 0xD4);
    while(inb(0x64) & 0x02); outb(0x60, 0xF4);
    while(!(inb(0x64) & 0x01)); inb(0x60);
}