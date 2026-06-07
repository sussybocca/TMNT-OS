#include "input.h"
#include "gui.h"
#include "../drivers/vga/vga.h"
#include "../system/string.h"

// from kernel
extern uint32_t* fb;
extern uint32_t fb_width, fb_height, fb_pitch;

// from terminal.c - need to declare these as extern
extern int terminal_mode;
extern void term_fb_putchar(char c);
extern void term_fb_print(const char* str);

int mouse_x = 512, mouse_y = 384;
int mouse_btn_left = 0, mouse_btn_old_left = 0;

static const char scancode_to_ascii[] = {
    0,0,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,
    '\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,'\n'
};

  uint8_t inb(uint16_t port) { 
    uint8_t r; 
    asm volatile("inb %1,%0":"=a"(r):"Nd"(port)); 
    return r; 
}

  void outb(uint16_t port, uint8_t v) { 
    asm volatile("outb %0,%1"::"a"(v),"Nd"(port)); 
}

char keyboard_getchar_poll(void) {
    static int e0_prefix = 0;
    
    while(1) {
        uint8_t s = inb(0x64);
        
        // Check if mouse data is available (bit 5)
        if(s & 0x20) {
            static int mcycle = 0; 
            static uint8_t mbuf[3];
            
            mbuf[mcycle++] = inb(0x60);
            
            if(mcycle == 3) {
                mcycle = 0;
                
                // Parse mouse packet
                int dx = mbuf[1];
                int dy = mbuf[2];
                
                // Sign extend if bit 4 (x sign) or bit 5 (y sign) is set
                if(mbuf[0] & 0x10) dx -= 256;
                if(mbuf[0] & 0x20) dy -= 256;
                
                // Check left button (bit 0)
                int btn_now = (mbuf[0] & 0x01) ? 1 : 0;
                
                // Handle click event (rising edge detection)
                if(mouse_btn_old_left == 0 && btn_now == 1) {
                    gui_handle_click();
                }
                mouse_btn_old_left = btn_now;
                
                // Restore background under cursor if in GUI mode
                if(gui_mode) {
                    gui_restore_cursor_bg();
                }
                
                // Update mouse position
                mouse_x += dx;
                mouse_y -= dy;  // Y axis is inverted
                
                // Clamp mouse position to screen bounds
                if(mouse_x < 0) mouse_x = 0;
                if(mouse_y < 0) mouse_y = 0;
                if(mouse_x >= (int)fb_width) mouse_x = fb_width - 1;
                if(mouse_y >= (int)fb_height) mouse_y = fb_height - 1;
                
                // Redraw cursor in new position if in GUI mode
                if(gui_mode) {
                    gui_save_cursor_bg();
                    gui_draw_cursor();
                }
            }
            continue;
        }
        
        // Check if keyboard data is available (bit 0)
        if(s & 0x01) {
            uint8_t sc = inb(0x60);
            
            // Handle extended key prefix (0xE0)
            if(sc == 0xE0) { 
                e0_prefix = 1; 
                continue; 
            }
            
            // Skip key release events (bit 7 set)
            if(sc & 0x80) { 
                e0_prefix = 0; 
                continue; 
            }
            
            // Handle extended key: E0 1C = numpad enter
            if(e0_prefix && sc == 0x1C) { 
                e0_prefix = 0; 
                return '\n'; 
            }
            
            e0_prefix = 0;
            
            // Map scancode to ASCII
            if(sc >= sizeof(scancode_to_ascii)) continue;
            
            char c = scancode_to_ascii[sc];
            if(c == 0) continue;
            
            return c;
        }
    }
}

char* keyboard_readline_poll(void) {
    static char buf[256];
    int i = 0;
    
    // Print initial prompt based on current mode
    if(terminal_mode) {
        term_fb_print("> ");
    }
    
    while(1) {
        char c = keyboard_getchar_poll();
        
        if(c == '\n') { 
            buf[i] = '\0';
            
            // Echo newline to appropriate output
            if(terminal_mode) {
                term_fb_putchar('\n');
            } else {
                vga_putchar('\n');
            }
            
            return buf; 
        }
        
        if(c == '\b') { 
            if(i > 0) { 
                i--;
                
                // Handle backspace in appropriate output mode
                if(terminal_mode) {
                    term_fb_putchar('\b');
                } else {
                    vga_putchar('\b');
                    vga_putchar(' ');
                    vga_putchar('\b');
                }
            }
            continue; 
        }
        
        if(c >= ' ' && i < 254) { 
            buf[i++] = c;
            
            // Echo character to appropriate output
            if(terminal_mode) {
                term_fb_putchar(c);
            } else {
                vga_putchar(c);
            }
        }
    }
}

char* keyboard_read_multiline(void) {
    static char buf[4096];
    int pos = 0;
    
    // Print multiline instructions to appropriate output
    if(terminal_mode) {
        term_fb_print("(Type '.' on empty line to finish)\n");
    } else {
        vga_print("(Type '.' on empty line to finish)\n");
    }
    
    while(pos < 4090) {
        // Print prompt based on mode
        if(terminal_mode) {
            term_fb_print("> ");
        } else {
            vga_print("> ");
        }
        
        char* line = keyboard_readline_poll();
        
        // Check for termination condition
        if(line[0] == '.' && line[1] == '\0') break;
        
        // Append line to buffer
        int j = 0;
        while(line[j]) {
            buf[pos++] = line[j++];
        }
        buf[pos++] = '\n';
    }
    
    buf[pos] = '\0';
    return buf;
}

int keyboard_getint_poll(void) {
    char buf[32];
    int i = 0;
    
    while(1) {
        char c = keyboard_getchar_poll();
        
        if(c == '\n') break;
        
        if(c == '\b' && i > 0) { 
            i--;
            
            // Handle backspace in appropriate output mode
            if(terminal_mode) {
                term_fb_putchar('\b');
            } else {
                vga_putchar('\b');
            }
        }
        else if((c >= '0' && c <= '9') || (c == '-' && i == 0)) { 
            buf[i++] = c;
            
            // Echo character to appropriate output
            if(terminal_mode) {
                term_fb_putchar(c);
            } else {
                vga_putchar(c);
            }
        }
    }
    
    buf[i] = '\0';
    
    // Echo newline to appropriate output
    if(terminal_mode) {
        term_fb_putchar('\n');
    } else {
        vga_putchar('\n');
    }
    
    // Convert string to integer
    int result = 0;
    int sign = 1;
    int j = 0;
    
    if(buf[0] == '-') { 
        sign = -1; 
        j = 1; 
    }
    
    while(buf[j]) { 
        result = result * 10 + (buf[j] - '0'); 
        j++; 
    }
    
    return result * sign;
}

void mouse_init(void) {
    // Enable auxiliary device (mouse)
    while(inb(0x64) & 0x02);  // Wait for input buffer empty
    outb(0x64, 0xA8);
    
    // Get current configuration byte
    while(inb(0x64) & 0x02);
    outb(0x64, 0x20);
    while(!(inb(0x64) & 0x01));
    uint8_t cfg = inb(0x60);
    
    // Enable mouse interrupts and disable mouse clock
    cfg |= 0x02;   // Set bit 1 (enable mouse IRQ12)
    cfg &= ~0x20;  // Clear bit 5 (disable mouse clock)
    
    // Write new configuration byte
    while(inb(0x64) & 0x02);
    outb(0x64, 0x60);
    while(inb(0x64) & 0x02);
    outb(0x60, cfg);
    
    // Send "enable data reporting" command to mouse
    while(inb(0x64) & 0x02);
    outb(0x64, 0xD4);  // Tell controller to send next byte to mouse
    while(inb(0x64) & 0x02);
    outb(0x60, 0xF4);  // Enable data reporting command
    
    // Wait for and discard acknowledgment byte
    while(!(inb(0x64) & 0x01));
    inb(0x60);
}