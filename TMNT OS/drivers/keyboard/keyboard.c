#include "keyboard.h"
#include "../vga/vga.h"
#include "../../system/string.h"

static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0
};

static const char scancode_to_ascii_shift[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0
};

static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static int buffer_head = 0;
static int buffer_tail = 0;
static int shift_pressed = 0;
static int caps_lock = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void keyboard_init() {
    buffer_head = 0;
    buffer_tail = 0;
    shift_pressed = 0;
}

char keyboard_getchar() {
    while(buffer_head == buffer_tail) {
        asm volatile("hlt");
    }
    char c = keyboard_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

int keyboard_getint() {
    char buf[32];
    int i = 0;
    char c;
    while(1) {
        c = keyboard_getchar();
        if(c == '\n') break;
        if(c == '\b' && i > 0) {
            i--;
            vga_putchar('\b');
        } else if((c >= '0' && c <= '9') || (c == '-' && i == 0)) {
            buf[i++] = c;
            vga_putchar(c);
        }
    }
    buf[i] = '\0';
    vga_putchar('\n');
    return str_to_int(buf);
}

char* keyboard_readline() {
    static char buf[256];
    int i = 0;
    char c;
    while(1) {
        c = keyboard_getchar();
        if(c == '\n') {
            buf[i] = '\0';
            vga_putchar('\n');
            break;
        } else if(c == '\b' && i > 0) {
            i--;
            vga_putchar('\b');
            vga_putchar(' ');
            vga_putchar('\b');
        } else if(c >= ' ') {
            buf[i++] = c;
            vga_putchar(c);
        }
    }
    return buf;
}

void keyboard_handler() {
    uint8_t scancode = inb(KEYBOARD_PORT);
    if(scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; return; }
    if(scancode == 0xAA || scancode == 0xB6) { shift_pressed = 0; return; }
    if(scancode == 0x3A) { caps_lock = !caps_lock; return; }
    
    if(scancode < sizeof(scancode_to_ascii)) {
        char ascii = (shift_pressed || caps_lock) ? 
                     scancode_to_ascii_shift[scancode] : 
                     scancode_to_ascii[scancode];
        if(ascii) {
            keyboard_buffer[buffer_head] = ascii;
            buffer_head = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
        }
    }
}