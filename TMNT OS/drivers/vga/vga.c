// TMNT OS VGA Driver - Fixed
#include "vga.h"
#include "../../system/string.h"

typedef struct {
    uint8_t character;
    uint8_t color;
} __attribute__((packed)) vga_cell_t;

static vga_cell_t* vga_buffer = (vga_cell_t*)VGA_MEMORY;
static uint8_t current_fg = VGA_COLOR_WHITE;
static uint8_t current_bg = VGA_COLOR_BLACK;
int cursor_x = 0;
int cursor_y = 0;

static uint8_t vga_colors[16][3] = {
    {0, 0, 0},       {0, 0, 170},     {0, 170, 0},     {0, 170, 170},
    {170, 0, 0},     {170, 0, 170},   {170, 85, 0},    {170, 170, 170},
    {85, 85, 85},    {85, 85, 255},   {85, 255, 85},   {85, 255, 255},
    {255, 85, 85},   {255, 85, 255},  {255, 255, 85},  {255, 255, 255}
};

// I/O port functions - MUST be before any usage
static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void vga_init() {
    vga_clear_screen();
    cursor_x = 0;
    cursor_y = 0;
}

void vga_clear_screen() {
    for(int y = 0; y < VGA_HEIGHT; y++) {
        for(int x = 0; x < VGA_WIDTH; x++) {
            int index = y * VGA_WIDTH + x;
            vga_buffer[index].character = ' ';
            vga_buffer[index].color = (current_bg << 4) | current_fg;
        }
    }
    cursor_x = 0;
    cursor_y = 0;
}

void vga_set_fg_color(uint8_t color) { current_fg = color; }
void vga_set_bg_color(uint8_t color) { current_bg = color; }

void vga_putchar(char c) {
    if(c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if(c == '\r') {
        cursor_x = 0;
    } else if(c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
    } else if(c >= ' ') {
        int index = cursor_y * VGA_WIDTH + cursor_x;
        vga_buffer[index].character = c;
        vga_buffer[index].color = (current_bg << 4) | current_fg;
        cursor_x++;
    }
    
    if(cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    if(cursor_y >= VGA_HEIGHT) {
        vga_scroll();
    }
    
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_print(const char* str) {
    while(*str) vga_putchar(*str++);
}

void vga_print_int(int num) {
    char buf[16];
    int_to_str(num, buf);
    vga_print(buf);
}

void vga_print_at(const char* str, int x, int y) {
    int ox = cursor_x, oy = cursor_y;
    cursor_x = x; cursor_y = y;
    vga_print(str);
    cursor_x = ox; cursor_y = oy;
}

void vga_print_centered(const char* str, int y) {
    int x = (VGA_WIDTH - strlen(str)) / 2;
    vga_print_at(str, x, y);
}

void vga_draw_pixel(int x, int y, uint8_t fg, uint8_t bg) {
    if(x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        int index = y * VGA_WIDTH + x;
        vga_buffer[index].character = ' ';
        vga_buffer[index].color = (bg << 4) | fg;
    }
}

void vga_draw_rect(int x, int y, int w, int h, uint8_t fg, uint8_t bg) {
    for(int dy = 0; dy < h; dy++)
        for(int dx = 0; dx < w; dx++)
            vga_draw_pixel(x + dx, y + dy, fg, bg);
}

static uint8_t rgb_to_vga(uint8_t r, uint8_t g, uint8_t b) {
    int min_dist = 1000000, best = 0;
    for(int i = 0; i < 16; i++) {
        int dr = r - vga_colors[i][0];
        int dg = g - vga_colors[i][1];
        int db = b - vga_colors[i][2];
        int dist = dr*dr + dg*dg + db*db;
        if(dist < min_dist) { min_dist = dist; best = i; }
    }
    return best;
}

void vga_render_image(uint8_t* raw_data, int img_width, int img_height, int x_off, int y_off) {
    float xs = (float)img_width / VGA_WIDTH;
    float ys = (float)img_height / VGA_HEIGHT;
    for(int y = 0; y < VGA_HEIGHT && y + y_off < VGA_HEIGHT; y++) {
        for(int x = 0; x < VGA_WIDTH && x + x_off < VGA_WIDTH; x++) {
            int ix = (int)(x * xs);
            int iy = (int)(y * ys);
            int po = (iy * img_width + ix) * 3;
            if(po + 2 < img_width * img_height * 3) {
                uint8_t c = rgb_to_vga(raw_data[po], raw_data[po+1], raw_data[po+2]);
                vga_draw_pixel(x + x_off, y + y_off, c, VGA_COLOR_BLACK);
            }
        }
    }
}

void vga_scroll() {
    for(int y = 0; y < VGA_HEIGHT - 1; y++)
        for(int x = 0; x < VGA_WIDTH; x++)
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
    for(int x = 0; x < VGA_WIDTH; x++) {
        int i = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
        vga_buffer[i].character = ' ';
        vga_buffer[i].color = (current_bg << 4) | current_fg;
    }
    cursor_y = VGA_HEIGHT - 1;
}