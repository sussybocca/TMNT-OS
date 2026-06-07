#include "turtlenotes.h"
#include "../../external/gui.h"
#include "../../system/string.h"

extern uint32_t* fb;
extern uint32_t fb_width, fb_height, fb_pitch;
extern const unsigned char font_8x16[96][16];

// Drawing functions - draw directly to framebuffer like paint_studio does
static void tn_putpixel(int x, int y, uint32_t color) {
    if(x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height)
        fb[y * (fb_pitch / 4) + x] = color;
}

static void tn_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for(int dy = 0; dy < h; dy++)
        for(int dx = 0; dx < w; dx++)
            tn_putpixel(x + dx, y + dy, color);
}

static void tn_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for(int i = 0; i < w; i++) { tn_putpixel(x+i, y, color); tn_putpixel(x+i, y+h-1, color); }
    for(int i = 0; i < h; i++) { tn_putpixel(x, y+i, color); tn_putpixel(x+w-1, y+i, color); }
}

static void tn_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    if(c < 32 || c > 126) return;
    const unsigned char* glyph = font_8x16[c - 32];
    for(int row = 0; row < 16; row++) {
        unsigned char line = glyph[row];
        for(int col = 0; col < 8; col++)
            tn_putpixel(x + col, y + row, (line & (0x80 >> col)) ? fg : bg);
    }
}

static void tn_draw_text(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    int cx = x;
    while(*str) {
        if(*str == '\n') { cx = x; y += 16; }
        else { tn_draw_char(cx, y, *str, fg, bg); cx += 8; }
        str++;
    }
}

static char notes_text[1024] = "Welcome to TurtleNotes!\nType your notes here...\n\n";
static int cursor_pos = 0;

static void turtlenotes_draw(int wx, int wy, int ww, int wh) {
    // Draw notepad background
    tn_fill_rect(wx + 10, wy + 30, ww - 20, wh - 50, 0x000000);
    tn_draw_rect(wx + 10, wy + 30, ww - 20, wh - 50, 0x00AA00);
    
    // Draw text
    tn_draw_text(wx + 15, wy + 35, notes_text, 0x00FF00, 0x000000);
    
    // Draw cursor
    int line = 0, col = 0;
    for(int i = 0; i < cursor_pos && i < 1023; i++) {
        if(notes_text[i] == '\n') { line++; col = 0; }
        else col++;
    }
    tn_fill_rect(wx + 15 + col * 8, wy + 35 + line * 16, 8, 16, 0x00FF00);
}

static void turtlenotes_click(int mx, int my) {
    // Handle clicks
}

void turtlenotes_open(void) {
    // gui_run_auto_app injects window frame, title bar, close/min/max buttons,
    // keyboard polling, mouse tracking, and dragging automatically.
    gui_run_auto_app("🐢 TurtleNotes", 100, 80, 600, 400, turtlenotes_draw, turtlenotes_click);
}