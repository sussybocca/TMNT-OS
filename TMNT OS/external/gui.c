#include "gui.h"
#include "../drivers/vga/vga.h"
#include "../system/string.h"

// Framebuffer (set by kernel_main)
extern uint32_t* fb;
extern uint32_t fb_width, fb_height, fb_pitch;

int gui_mode = 0;
int mouse_x = 512, mouse_y = 384;
int mouse_btn_left = 0, mouse_btn_old_left = 0;
uint32_t cursor_bg[16][16];

typedef struct { int x, y, w, h; char name[32]; } desktop_icon_t;
static desktop_icon_t icons[32];
static int icon_count = 0;

// ===== AUTO-DISCOVERED APPS REGISTRY =====
#define MAX_AUTO_APPS 32
typedef struct {
    char name[64];
    char icon_name[32];
    void (*open_func)(void);
    int is_dynamic;
} app_entry_t;

static app_entry_t app_registry[MAX_AUTO_APPS];
static int app_count = 0;

// ===== EXEMPTION LIST =====
static const char* exemption_list[] = {
    "file_manager",
    "text_editor", 
    "runner_game",
    "paint_studio",
    NULL
};

static int is_exempt(const char* app_name) {
    for(int i = 0; exemption_list[i] != NULL; i++) {
        if(strcmp(app_name, exemption_list[i]) == 0) return 1;
    }
    return 0;
}

// ===== APP REGISTRATION =====
void gui_register_auto_app(const char* name, const char* icon_name, void (*open_func)(void)) {
    if(app_count >= MAX_AUTO_APPS) return;
    
    app_entry_t* app = &app_registry[app_count];
    int i = 0;
    while(name[i] && i < 63) { app->name[i] = name[i]; i++; }
    app->name[i] = 0;
    
    i = 0;
    while(icon_name[i] && i < 31) { app->icon_name[i] = icon_name[i]; i++; }
    app->icon_name[i] = 0;
    
    app->open_func = open_func;
    app->is_dynamic = 1;
    app_count++;
}

/* 8x16 font for ASCII 32-127 */
const unsigned char font_8x16[96][16] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x18,0x3C,0x3C,0x3C,0x18,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00},
    {0x00,0x66,0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x6C,0x6C,0xFE,0x6C,0x6C,0xFE,0x6C,0x6C,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x18,0x7C,0xC6,0xC2,0xC0,0x7C,0x06,0x06,0x86,0xC6,0x7C,0x18,0x18,0x00,0x00},
    {0x00,0x00,0x00,0x00,0xC2,0xC6,0x0C,0x18,0x30,0x60,0xC6,0x86,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x38,0x6C,0x6C,0x38,0x76,0xDC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    {0x00,0x30,0x30,0x30,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x0C,0x18,0x30,0x30,0x30,0x30,0x30,0x30,0x18,0x0C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x30,0x18,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x18,0x30,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x18,0x30,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x02,0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x3C,0x66,0xC3,0xC3,0xDB,0xDB,0xC3,0xC3,0x66,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x18,0x38,0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x7C,0xC6,0x06,0x0C,0x18,0x30,0x60,0xC0,0xC6,0xFE,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x7C,0xC6,0x06,0x06,0x3C,0x06,0x06,0x06,0xC6,0x7C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x0C,0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x0C,0x0C,0x1E,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xFE,0xC0,0xC0,0xC0,0xFC,0x06,0x06,0x06,0xC6,0x7C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x38,0x60,0xC0,0xC0,0xFC,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xFE,0xC6,0x06,0x06,0x0C,0x18,0x30,0x30,0x30,0x30,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7E,0x06,0x06,0x06,0x0C,0x78,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x30,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x06,0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x06,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x60,0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x60,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x7C,0xC6,0xC6,0x0C,0x18,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x7C,0xC6,0xC6,0xDE,0xDE,0xDE,0xDC,0xC0,0x7C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x10,0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x66,0x66,0x66,0x66,0xFC,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x3C,0x66,0xC2,0xC0,0xC0,0xC0,0xC0,0xC2,0x66,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xF8,0x6C,0x66,0x66,0x66,0x66,0x66,0x66,0x6C,0xF8,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xFE,0x66,0x62,0x68,0x78,0x68,0x60,0x62,0x66,0xFE,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xFE,0x66,0x62,0x68,0x78,0x68,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x3C,0x66,0xC2,0xC0,0xC0,0xDE,0xC6,0xC6,0x66,0x3A,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x3C,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0xCC,0xCC,0xCC,0x78,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xE6,0x66,0x66,0x6C,0x78,0x78,0x6C,0x66,0x66,0xE6,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xF0,0x60,0x60,0x60,0x60,0x60,0x60,0x62,0x66,0xFE,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xC3,0xE7,0xFF,0xFF,0xDB,0xC3,0xC3,0xC3,0xC3,0xC3,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xC6,0xE6,0xF6,0xFE,0xDE,0xCE,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x60,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xD6,0xDE,0x7C,0x0C,0x0E,0x00,0x00},
    {0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x6C,0x66,0x66,0x66,0xE6,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x7C,0xC6,0xC6,0x60,0x38,0x0C,0x06,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xFF,0xDB,0x99,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0x66,0x3C,0x18,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xC3,0xC3,0xC3,0xC3,0xC3,0xDB,0xDB,0xFF,0x66,0x66,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xC3,0xC3,0x66,0x3C,0x18,0x18,0x3C,0x66,0xC3,0xC3,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xC3,0xC3,0xC3,0x66,0x3C,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xFF,0xC3,0x86,0x0C,0x18,0x30,0x60,0xC1,0xC3,0xFF,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x3C,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x80,0xC0,0xE0,0x70,0x38,0x1C,0x0E,0x06,0x02,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00,0x00,0x00,0x00},
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x00},
    {0x30,0x30,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x78,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    {0x00,0x00,0xE0,0x60,0x60,0x78,0x6C,0x66,0x66,0x66,0x66,0x7C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xC0,0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x1C,0x0C,0x0C,0x3C,0x6C,0xCC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xFE,0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x38,0x6C,0x64,0x60,0xF0,0x60,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x76,0xCC,0xCC,0xCC,0xCC,0xCC,0x7C,0x0C,0xCC,0x78,0x00},
    {0x00,0x00,0xE0,0x60,0x60,0x6C,0x76,0x66,0x66,0x66,0x66,0xE6,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x18,0x18,0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x06,0x06,0x00,0x0E,0x06,0x06,0x06,0x06,0x06,0x06,0x66,0x66,0x3C,0x00},
    {0x00,0x00,0xE0,0x60,0x60,0x66,0x6C,0x78,0x78,0x6C,0x66,0xE6,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0xE6,0xFF,0xDB,0xDB,0xDB,0xDB,0xDB,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x66,0x66,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x76,0xCC,0xCC,0xCC,0xCC,0xCC,0x7C,0x0C,0x0C,0x1E,0x00},
    {0x00,0x00,0x00,0x00,0x00,0xDC,0x76,0x66,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0x60,0x38,0x0C,0xC6,0x7C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x10,0x30,0x30,0xFC,0x30,0x30,0x30,0x30,0x36,0x1C,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0xC3,0xC3,0xC3,0xC3,0x66,0x3C,0x18,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0xC3,0xC3,0xC3,0xDB,0xDB,0xFF,0x66,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0xC3,0x66,0x3C,0x18,0x3C,0x66,0xC3,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7E,0x06,0x0C,0xF8,0x00},
    {0x00,0x00,0x00,0x00,0x00,0xFE,0xCC,0x18,0x30,0x60,0xC6,0xFE,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x0E,0x18,0x18,0x18,0x70,0x18,0x18,0x18,0x18,0x0E,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x18,0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x70,0x18,0x18,0x18,0x0E,0x18,0x18,0x18,0x18,0x70,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};

// Forward declarations for app functions
extern void file_manager_open(void);
extern void text_editor_open(void);
extern void runner_game_open(void);
extern void paint_studio_open(void);

// ===== EMBEDDED I/O (same as input.c) =====
static inline uint8_t gui_inb(uint16_t port) {
    uint8_t r;
    asm volatile("inb %1,%0":"=a"(r):"Nd"(port));
    return r;
}

static void fb_putpixel(int x, int y, uint32_t color) {
    if(x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height)
        fb[y * (fb_pitch / 4) + x] = color;
}

static uint32_t fb_getpixel(int x, int y) {
    if(x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height)
        return fb[y * (fb_pitch / 4) + x];
    return 0;
}

static void fb_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for(int dy = 0; dy < h; dy++)
        for(int dx = 0; dx < w; dx++)
            fb_putpixel(x + dx, y + dy, color);
}

static void fb_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for(int i = 0; i < w; i++) { fb_putpixel(x+i, y, color); fb_putpixel(x+i, y+h-1, color); }
    for(int i = 0; i < h; i++) { fb_putpixel(x, y+i, color); fb_putpixel(x+w-1, y+i, color); }
}

static void fb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    if(c < 32 || c > 126) return;
    const unsigned char* glyph = font_8x16[c - 32];
    for(int row = 0; row < 16; row++) {
        unsigned char line = glyph[row];
        for(int col = 0; col < 8; col++)
            fb_putpixel(x + col, y + row, (line & (0x80 >> col)) ? fg : bg);
    }
}

static void fb_draw_text(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    int cx = x;
    while(*str) {
        if(*str == '\n') { cx = x; y += 16; }
        else { fb_draw_char(cx, y, *str, fg, bg); cx += 8; }
        str++;
    }
}

// ===== TMNT-THEMED WINDOW CONTROL BUTTON DRAWINGS =====
void gui_draw_close_button(int x, int y, int size) {
    int cx = x + size/2, cy = y + size/2, r = size/2 - 1;
    for(int py = y; py < y + size; py++)
        for(int px = x; px < x + size; px++) {
            int dx = px - cx, dy = py - cy;
            if(dx*dx + dy*dy <= r*r) fb_putpixel(px, py, 0xCC2200);
            else fb_putpixel(px, py, 0x0A0A0A);
        }
    for(int py = y; py < y + size; py++)
        for(int px = x; px < x + size; px++) {
            int dx = px - cx, dy = py - cy;
            if(dx*dx + dy*dy <= r*r && dx*dx + dy*dy >= (r-1)*(r-1)) fb_putpixel(px, py, 0xFF6600);
        }
    fb_fill_rect(x + size/2 - 3, y + size/2 - 5, 2, 10, 0x8B0000);
    fb_fill_rect(x + size/2 - 5, y + size/2 - 3, 10, 2, 0x8B0000);
    fb_putpixel(x + size/2 - 2, y + size/2 - 4, 0xFF4444);
    fb_putpixel(x + size/2 - 4, y + size/2 - 2, 0xFF4444);
}

void gui_draw_minimize_button(int x, int y, int size) {
    int cx = x + size/2, cy = y + size/2, r = size/2 - 1;
    for(int py = y; py < y + size; py++)
        for(int px = x; px < x + size; px++) {
            int dx = px - cx, dy = py - cy;
            if(dx*dx + dy*dy <= r*r) fb_putpixel(px, py, 0x006600);
            else fb_putpixel(px, py, 0x0A0A0A);
        }
    for(int py = y; py < y + size; py++)
        for(int px = x; px < x + size; px++) {
            int dx = px - cx, dy = py - cy;
            if(dx*dx + dy*dy <= r*r && dx*dx + dy*dy >= (r-1)*(r-1)) fb_putpixel(px, py, 0x00AA00);
        }
    for(int py = y + 3; py < y + size - 3; py++)
        for(int px = x + 3; px < x + size - 3; px++) {
            int dx = px - cx, dy = py - cy;
            if(dx*dx + dy*dy <= (r-3)*(r-3) && (px + py) % 4 == 0) fb_putpixel(px, py, 0x004400);
        }
    fb_fill_rect(x + size/2 - 4, y + size/2 + 2, 8, 2, 0x00FF00);
}

void gui_draw_maximize_button(int x, int y, int size) {
    int cx = x + size/2, cy = y + size/2, r = size/2 - 1;
    for(int py = y; py < y + size; py++)
        for(int px = x; px < x + size; px++) {
            int dx = px - cx, dy = py - cy;
            if(dx*dx + dy*dy <= r*r) fb_putpixel(px, py, 0x444444);
            else fb_putpixel(px, py, 0x0A0A0A);
        }
    for(int py = y; py < y + size; py++)
        for(int px = x; px < x + size; px++) {
            int dx = px - cx, dy = py - cy;
            if(dx*dx + dy*dy <= r*r && dx*dx + dy*dy >= (r-1)*(r-1)) fb_putpixel(px, py, 0x888888);
        }
    for(int ring = 2; ring < r-2; ring += 2)
        for(int py = y; py < y + size; py++)
            for(int px = x; px < x + size; px++) {
                int dx = px - cx, dy = py - cy, dist = dx*dx + dy*dy;
                if(dist >= (ring-1)*(ring-1) && dist <= ring*ring) fb_putpixel(px, py, 0x666666);
            }
    fb_fill_rect(cx - 1, cy - 1, 3, 3, 0x222222);
    fb_putpixel(x + size - 5, y + 5, 0xCCCCCC); fb_putpixel(x + size - 4, y + 5, 0xCCCCCC);
    fb_putpixel(x + size - 5, y + 4, 0xCCCCCC); fb_putpixel(x + 5, y + size - 5, 0xCCCCCC);
    fb_putpixel(x + 4, y + size - 5, 0xCCCCCC); fb_putpixel(x + 5, y + size - 4, 0xCCCCCC);
}

void gui_draw_restore_button(int x, int y, int size) {
    int cx = x + size/2, cy = y + size/2, r = size/2 - 1;
    for(int py = y; py < y + size; py++)
        for(int px = x; px < x + size; px++) {
            int dx = px - cx, dy = py - cy;
            if(dx*dx + dy*dy <= r*r) fb_putpixel(px, py, 0x444444);
            else fb_putpixel(px, py, 0x0A0A0A);
        }
    for(int py = y; py < y + size; py++)
        for(int px = x; px < x + size; px++) {
            int dx = px - cx, dy = py - cy;
            if(dx*dx + dy*dy <= r*r && dx*dx + dy*dy >= (r-1)*(r-1)) fb_putpixel(px, py, 0x888888);
        }
    fb_draw_rect(x + 3, y + 5, 6, 6, 0xCCCCCC);
    fb_draw_rect(x + 6, y + 3, 6, 6, 0xCCCCCC);
    fb_fill_rect(x + 6, y + 3, 6, 6, 0x444444);
    fb_draw_rect(x + 6, y + 3, 6, 6, 0xFFFFFF);
}

void gui_draw_title_bar(int x, int y, int w, const char* title) {
    for(int dy = 0; dy < 24; dy++) {
        uint8_t g = 30 + dy;
        uint32_t color = (g/4 << 16) | (g << 8) | (g/3);
        for(int dx = 0; dx < w; dx++) fb_putpixel(x + dx, y + dy, color);
    }
    fb_draw_rect(x, y, w, 24, 0x00AA00);
    fb_draw_text(x + 40, y + 4, title, 0x00FF00, 0x1A2A0A);
    int btn_size = 18, btn_y = y + 3, btn_spacing = 22;
    gui_draw_minimize_button(x + w - btn_spacing * 3 + 2, btn_y, btn_size);
    gui_draw_maximize_button(x + w - btn_spacing * 2 + 2, btn_y, btn_size);
    gui_draw_close_button(x + w - btn_spacing + 2, btn_y, btn_size);
}

void gui_draw_window(int x, int y, int w, int h, const char* title) {
    fb_fill_rect(x + 3, y + 3, w, h, 0x000000);
    fb_fill_rect(x, y, w, h, 0x1A1A2E);
    fb_draw_rect(x, y, w, h, 0x00AA00);
    fb_draw_rect(x + 1, y + 1, w - 2, h - 2, 0x004400);
    gui_draw_title_bar(x + 1, y + 1, w - 2, title);
}

void gui_draw_tmnt_button(int x, int y, int w, int h, const char* text, uint32_t bg) {
    fb_fill_rect(x + 2, y + 2, w, h, 0x000000);
    fb_fill_rect(x, y, w, h, bg);
    fb_draw_rect(x, y, w, h, 0x00FF00);
    for(int i = 0; i < 3; i++) {
        for(int dx = i; dx < w - i - 1; dx++) fb_putpixel(x + dx, y + i, 0x44FF44);
        for(int dy = i; dy < h - i - 1; dy++) fb_putpixel(x + i, y + dy, 0x44FF44);
    }
    for(int i = 0; i < 2; i++) {
        for(int dx = i; dx < w - i - 1; dx++) fb_putpixel(x + dx, y + h - 1 - i, 0x004400);
        for(int dy = i; dy < h - i - 1; dy++) fb_putpixel(x + w - 1 - i, y + dy, 0x004400);
    }
    int text_x = x + (w - strlen(text) * 8) / 2, text_y = y + (h - 16) / 2;
    fb_draw_text(text_x, text_y, text, 0xFFFFFF, bg);
}

// ===== PUBLIC FB DRAWING FUNCTIONS FOR APPS =====
void gui_fb_putpixel(int x, int y, uint32_t color) { fb_putpixel(x, y, color); }
uint32_t gui_fb_getpixel(int x, int y) { return fb_getpixel(x, y); }
void gui_fb_fill_rect(int x, int y, int w, int h, uint32_t color) { fb_fill_rect(x, y, w, h, color); }
void gui_fb_draw_rect(int x, int y, int w, int h, uint32_t color) { fb_draw_rect(x, y, w, h, color); }
void gui_fb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) { fb_draw_char(x, y, c, fg, bg); }
void gui_fb_draw_text(int x, int y, const char* str, uint32_t fg, uint32_t bg) { fb_draw_text(x, y, str, fg, bg); }

// ===== AUTOMATIC APP RUNNER WITH EMBEDDED INPUT CONTROLS =====
void gui_run_auto_app(const char* title, int wx, int wy, int ww, int wh,
                      void (*draw_content)(int wx, int wy, int ww, int wh),
                      void (*handle_click)(int mx, int my),
                      void (*handle_key)(char key)) {
    
    int minimized = 0, maximized = 0;
    int dragging = 0, drag_ox = 0, drag_oy = 0;
    int need_redraw = 1;
    
    uint32_t bg_save[1024][768];
    int bg_sx = 0, bg_sy = 0, bg_sw = 0, bg_sh = 0, bg_ok = 0;
    
    // Save background
    if(ww > 1024) ww = 1024; if(wh > 768) wh = 768;
    bg_sx = wx; bg_sy = wy; bg_sw = ww + 5; bg_sh = wh + 5;
    for(int dy = 0; dy < bg_sh; dy++)
        for(int dx = 0; dx < bg_sw; dx++)
            bg_save[dx][dy] = fb_getpixel(wx + dx, wy + dy);
    bg_ok = 1;
    
    gui_save_cursor_bg();
    gui_draw_cursor();
    
    // Local mouse state (same logic as paint_studio ps_poll_input)
    int local_mx = mouse_x, local_my = mouse_y;
    int local_btn = mouse_btn_left, local_btn_old = mouse_btn_left;
    int mcycle = 0;
    uint8_t mbuf[3];
    
    // Keyboard state (same logic as input.c keyboard_getchar_poll)
    static const char scancode_to_ascii[] = {
        0,0,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
        'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
        'a','s','d','f','g','h','j','k','l',';','\'','`',0,
        '\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,'\n'
    };
    int e0_prefix = 0;
    
    while(1) {
        // Poll PS/2 port directly - NON-BLOCKING (same as ps_poll_input)
        int max_loops = 100;
        while(max_loops-- > 0) {
            uint8_t s = gui_inb(0x64);
            if(!(s & 0x01)) break;
            
            uint8_t data = gui_inb(0x60);
            
            // Mouse data (bit 5)
            if(s & 0x20) {
                mbuf[mcycle++] = data;
                if(mcycle == 3) {
                    mcycle = 0;
                    
                    gui_restore_cursor_bg();
                    
                    int dx = mbuf[1];
                    int dy = mbuf[2];
                    if(mbuf[0] & 0x10) dx -= 256;
                    if(mbuf[0] & 0x20) dy -= 256;
                    
                    local_btn = (mbuf[0] & 0x01) ? 1 : 0;
                    local_mx += dx;
                    local_my -= dy;
                    if(local_mx < 0) local_mx = 0;
                    if(local_my < 0) local_my = 0;
                    if(local_mx >= (int)fb_width) local_mx = fb_width - 1;
                    if(local_my >= (int)fb_height) local_my = fb_height - 1;
                    
                    mouse_x = local_mx;
                    mouse_y = local_my;
                    mouse_btn_left = local_btn;
                    
                    gui_save_cursor_bg();
                    gui_draw_cursor();
                }
            } else {
                // Keyboard data (bit 5 NOT set) - same logic as keyboard_getchar_poll
                uint8_t sc = data;
                
                if(sc == 0xE0) { 
                    e0_prefix = 1; 
                    continue; 
                }
                
                if(sc & 0x80) { 
                    e0_prefix = 0; 
                    continue; 
                }
                
                if(e0_prefix && sc == 0x1C) { 
                    e0_prefix = 0; 
                    if(handle_key) { handle_key('\n'); need_redraw = 1; }
                } else {
                    e0_prefix = 0;
                    if(sc < sizeof(scancode_to_ascii)) {
                        char c = scancode_to_ascii[sc];
                        if(c != 0 && handle_key) { handle_key(c); need_redraw = 1; }
                    }
                }
            }
        }
        
        int mx = local_mx, my = local_my;
        int btn = local_btn;
        int click = (local_btn_old == 0 && btn == 1);
        local_btn_old = btn;
        
        // INJECTED: Close button
        if(click && mx >= wx + ww - 24 && mx < wx + ww - 6 && my >= wy + 4 && my < wy + 22) {
            if(bg_ok) {
                gui_restore_cursor_bg();
                for(int dy = 0; dy < bg_sh; dy++)
                    for(int dx = 0; dx < bg_sw; dx++)
                        fb_putpixel(bg_sx + dx, bg_sy + dy, bg_save[dx][dy]);
            }
            break;
        }
        // INJECTED: Maximize button
        if(click && mx >= wx + ww - 46 && mx < wx + ww - 28 && my >= wy + 4 && my < wy + 22) {
            if(bg_ok) {
                gui_restore_cursor_bg();
                for(int dy = 0; dy < bg_sh; dy++)
                    for(int dx = 0; dx < bg_sw; dx++)
                        fb_putpixel(bg_sx + dx, bg_sy + dy, bg_save[dx][dy]);
            }
            if(maximized) { wx = 100; wy = 50; ww = 800; wh = 520; maximized = 0; }
            else { wx = 0; wy = 0; ww = fb_width; wh = fb_height - 40; maximized = 1; }
            if(ww > 1024) ww = 1024; if(wh > 768) wh = 768;
            bg_sx = wx; bg_sy = wy; bg_sw = ww + 5; bg_sh = wh + 5;
            for(int dy = 0; dy < bg_sh; dy++)
                for(int dx = 0; dx < bg_sw; dx++)
                    bg_save[dx][dy] = fb_getpixel(wx + dx, wy + dy);
            bg_ok = 1;
            need_redraw = 1;
        }
        // INJECTED: Minimize button
        if(click && mx >= wx + ww - 68 && mx < wx + ww - 50 && my >= wy + 4 && my < wy + 22) {
            if(!minimized) {
                if(bg_ok) {
                    gui_restore_cursor_bg();
                    for(int dy = 0; dy < bg_sh; dy++)
                        for(int dx = 0; dx < bg_sw; dx++)
                            fb_putpixel(bg_sx + dx, bg_sy + dy, bg_save[dx][dy]);
                }
                minimized = 1;
            } else {
                minimized = 0;
                if(ww > 1024) ww = 1024; if(wh > 768) wh = 768;
                bg_sx = wx; bg_sy = wy; bg_sw = ww + 5; bg_sh = wh + 5;
                for(int dy = 0; dy < bg_sh; dy++)
                    for(int dx = 0; dx < bg_sw; dx++)
                        bg_save[dx][dy] = fb_getpixel(wx + dx, wy + dy);
                bg_ok = 1;
                need_redraw = 1;
            }
        }
        
        // INJECTED: Drag title bar
        if(!maximized && !minimized && click && my >= wy && my < wy + 24 && mx >= wx && mx < wx + ww - 70) {
            dragging = 1; drag_ox = mx - wx; drag_oy = my - wy;
        }
        if(!btn) dragging = 0;
        if(dragging) {
            int nx = mx - drag_ox, ny = my - drag_oy;
            if(nx < 0) nx = 0; if(ny < 0) ny = 0;
            if(nx + ww > (int)fb_width) nx = fb_width - ww;
            if(ny + wh > (int)fb_height) ny = fb_height - wh;
            if(nx != wx || ny != wy) {
                if(bg_ok) {
                    gui_restore_cursor_bg();
                    for(int dy = 0; dy < bg_sh; dy++)
                        for(int dx = 0; dx < bg_sw; dx++)
                            fb_putpixel(bg_sx + dx, bg_sy + dy, bg_save[dx][dy]);
                }
                wx = nx; wy = ny;
                if(ww > 1024) ww = 1024; if(wh > 768) wh = 768;
                bg_sx = wx; bg_sy = wy; bg_sw = ww + 5; bg_sh = wh + 5;
                for(int dy = 0; dy < bg_sh; dy++)
                    for(int dx = 0; dx < bg_sw; dx++)
                        bg_save[dx][dy] = fb_getpixel(wx + dx, wy + dy);
                bg_ok = 1;
                need_redraw = 1;
            }
        }
        
        if(!minimized) {
            if(need_redraw) {
                gui_draw_window(wx, wy, ww, wh, title);
                if(draw_content) draw_content(wx, wy, ww, wh);
                need_redraw = 0;
                gui_save_cursor_bg();
                gui_draw_cursor();
            }
            if(click && handle_click) handle_click(mx, my);
        }
        
        for(volatile int d = 0; d < 40000; d++) asm volatile("nop");
    }
}
static void gui_draw_icon(int x, int y, int w, int h, uint32_t bg, const char* label) {
    fb_fill_rect(x, y, w, h, bg);
    fb_draw_rect(x, y, w, h, 0x00AA00);
    int text_x = x + (w - strlen(label)*8)/2;
    fb_draw_text(text_x, y + h + 4, label, 0xFFFFFF, 0x001A0A);
}

void gui_draw_desktop(void) {
    fb_fill_rect(0, 0, fb_width, fb_height, 0x001A0A);
    fb_fill_rect(0, fb_height - 40, fb_width, 40, 0x0A0A0A);
    fb_fill_rect(10, fb_height - 35, 30, 30, 0x00AA00);
    fb_fill_rect(14, fb_height - 31, 22, 22, 0x004400);
    
    icon_count = 7;
    icons[0] = (desktop_icon_t){50, 50, 80, 80, "Terminal"};
    icons[1] = (desktop_icon_t){160, 50, 80, 80, "Files"};
    icons[2] = (desktop_icon_t){270, 50, 80, 80, "Text Editor"};
    icons[3] = (desktop_icon_t){380, 50, 80, 80, "Customize"};
    icons[4] = (desktop_icon_t){490, 50, 80, 80, "Runner"};
    icons[5] = (desktop_icon_t){600, 50, 80, 80, "Paint"};
    icons[6] = (desktop_icon_t){710, 50, 80, 80, "Calc"};
    
    for(int i = 0; i < app_count && icon_count < 28; i++) {
        int col = icon_count % 7;
        int row = icon_count / 7;
        icons[icon_count] = (desktop_icon_t){50 + col * 110, 160 + row * 120, 80, 80, ""};
        int j = 0;
        while(app_registry[i].icon_name[j] && j < 31) {
            icons[icon_count].name[j] = app_registry[i].icon_name[j];
            j++;
        }
        icons[icon_count].name[j] = 0;
        icon_count++;
    }
    
    for(int i = 0; i < icon_count; i++)
        gui_draw_icon(icons[i].x, icons[i].y, icons[i].w, icons[i].h, 0x1A1A2E, icons[i].name);
    
    fb_draw_text(200, fb_height - 30, "🐢 TMNT OS Desktop", 0x00FF00, 0x0A0A0A);
}

void gui_save_cursor_bg(void) {
    for(int cy = 0; cy < 16; cy++)
        for(int cx = 0; cx < 16; cx++)
            cursor_bg[cy][cx] = fb_getpixel(mouse_x + cx, mouse_y + cy);
}

void gui_restore_cursor_bg(void) {
    for(int cy = 0; cy < 16; cy++)
        for(int cx = 0; cx < 16; cx++)
            fb_putpixel(mouse_x + cx, mouse_y + cy, cursor_bg[cy][cx]);
}

void gui_draw_cursor(void) {
    // Turtle cursor - 16x16 pixels
    // Shell (dark green circle)
    for(int cy = 0; cy < 14; cy++) {
        for(int cx = 2; cx < 14; cx++) {
            int dx = cx - 8;
            int dy = cy - 7;
            if(dx*dx + dy*dy <= 36) {
                fb_putpixel(mouse_x + cx, mouse_y + cy, 0x006600); // Dark green shell
            }
        }
    }
    
    // Shell pattern (lighter green hexagons)
    for(int cy = 1; cy < 13; cy++) {
        for(int cx = 3; cx < 13; cx++) {
            int dx = cx - 8;
            int dy = cy - 7;
            if(dx*dx + dy*dy <= 30 && (cx + cy) % 3 == 0) {
                fb_putpixel(mouse_x + cx, mouse_y + cy, 0x00AA00); // Pattern
            }
        }
    }
    
    // Shell rim (darker outline)
    for(int cy = 0; cy < 14; cy++) {
        for(int cx = 2; cx < 14; cx++) {
            int dx = cx - 8;
            int dy = cy - 7;
            int dist = dx*dx + dy*dy;
            if(dist >= 30 && dist <= 36) {
                fb_putpixel(mouse_x + cx, mouse_y + cy, 0x004400); // Rim
            }
        }
    }
    
    // Head (extends to top-left from shell)
    fb_putpixel(mouse_x + 0, mouse_y + 6, 0x44AA44);
    fb_putpixel(mouse_x + 1, mouse_y + 5, 0x44AA44);
    fb_putpixel(mouse_x + 1, mouse_y + 6, 0x44AA44);
    fb_putpixel(mouse_x + 2, mouse_y + 4, 0x44AA44);
    fb_putpixel(mouse_x + 2, mouse_y + 5, 0x44AA44);
    
    // Eyes (black dots on head)
    fb_putpixel(mouse_x + 0, mouse_y + 5, 0x000000);
    fb_putpixel(mouse_x + 1, mouse_y + 4, 0x000000);
    
    // Eye highlights (white)
    fb_putpixel(mouse_x + 0, mouse_y + 4, 0xFFFFFF);
    
    // Front legs (top)
    fb_putpixel(mouse_x + 4, mouse_y + 1, 0x44AA44);
    fb_putpixel(mouse_x + 5, mouse_y + 0, 0x44AA44);
    fb_putpixel(mouse_x + 5, mouse_y + 1, 0x44AA44);
    
    // Front legs (bottom)
    fb_putpixel(mouse_x + 4, mouse_y + 12, 0x44AA44);
    fb_putpixel(mouse_x + 5, mouse_y + 13, 0x44AA44);
    fb_putpixel(mouse_x + 5, mouse_y + 12, 0x44AA44);
    
    // Back legs (top)
    fb_putpixel(mouse_x + 10, mouse_y + 1, 0x44AA44);
    fb_putpixel(mouse_x + 11, mouse_y + 0, 0x44AA44);
    fb_putpixel(mouse_x + 11, mouse_y + 1, 0x44AA44);
    
    // Back legs (bottom)
    fb_putpixel(mouse_x + 10, mouse_y + 12, 0x44AA44);
    fb_putpixel(mouse_x + 11, mouse_y + 13, 0x44AA44);
    fb_putpixel(mouse_x + 11, mouse_y + 12, 0x44AA44);
    
    // Tail (bottom-right)
    fb_putpixel(mouse_x + 13, mouse_y + 8, 0x44AA44);
    fb_putpixel(mouse_x + 14, mouse_y + 7, 0x44AA44);
    fb_putpixel(mouse_x + 15, mouse_y + 7, 0x44AA44);
    fb_putpixel(mouse_x + 15, mouse_y + 8, 0x44AA44);
    
    // Mouth (tiny smile on head)
    fb_putpixel(mouse_x + 0, mouse_y + 8, 0x000000);
    fb_putpixel(mouse_x + 1, mouse_y + 9, 0x000000);
}

int gui_hit_icon(void) {
    for(int i = 0; i < icon_count; i++)
        if(mouse_x >= icons[i].x && mouse_x < icons[i].x + icons[i].w &&
           mouse_y >= icons[i].y && mouse_y < icons[i].y + icons[i].h) return i;
    return -1;
}

void gui_handle_click(void) {
    if(!gui_mode) return;
    int hit = gui_hit_icon();
    if(hit < 0) return;
    
    extern int terminal_mode;
    extern void term_fb_clear(void);
    extern void term_fb_print(const char*);
    extern uint32_t term_fg, term_bg;
    
    if(hit < 7) {
        switch(hit) {
            case 0:
                terminal_mode = 1; gui_mode = 0;
                term_fb_clear(); term_fg = 0x00FF00; term_bg = 0x000000;
                term_fb_print("\n🐢 TERMINAL MODE 🐢\n===================\nType 'exit' to return\nType 'files', 'edit', 'runner', 'paint', or 'turtle'\n\n> ");
                break;
            case 1: terminal_mode = 0; gui_mode = 0; file_manager_open(); gui_mode = 1; gui_draw_desktop(); gui_save_cursor_bg(); gui_draw_cursor(); break;
            case 2: terminal_mode = 0; gui_mode = 0; text_editor_open(); gui_mode = 1; gui_draw_desktop(); gui_save_cursor_bg(); gui_draw_cursor(); break;
            case 3: terminal_mode = 1; gui_mode = 0; term_fb_clear(); term_fg = 0x00FF00; term_bg = 0x000000; term_fb_print("\n🐢 CUSTOMIZATION SHELL 🐢\n=========================\nType 'turtle' for commands\n\n> "); break;
            case 4: terminal_mode = 0; gui_mode = 0; runner_game_open(); gui_mode = 1; gui_draw_desktop(); gui_save_cursor_bg(); gui_draw_cursor(); break;
            case 5: terminal_mode = 0; gui_mode = 0; paint_studio_open(); gui_mode = 1; gui_draw_desktop(); gui_save_cursor_bg(); gui_draw_cursor(); break;
            case 6: 
                if(app_count > 0 && app_registry[0].open_func) {
                    terminal_mode = 0; gui_mode = 0;
                    app_registry[0].open_func();
                    gui_mode = 1; gui_draw_desktop(); gui_save_cursor_bg(); gui_draw_cursor();
                }
                break;
        }
        return;
    }
    
    int dyn_index = hit - 7;
    if(dyn_index >= 0 && dyn_index < app_count) {
        if(app_registry[dyn_index].open_func) {
            terminal_mode = 0; gui_mode = 0;
            app_registry[dyn_index].open_func();
            gui_mode = 1; gui_draw_desktop(); gui_save_cursor_bg(); gui_draw_cursor();
        }
    }
}