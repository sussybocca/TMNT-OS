// apps/tmnt_maker/tmnt_maker.c
#include "tmnt_maker.h"
#include "../../external/gui.h"
#include "../../fs/tmnt_fs.h"
#include "../../system/string.h"
#include "../../system/memory.h"
#include "../../apps/tm_lang/tm_lang.h"

extern uint32_t* fb;
extern uint32_t fb_width, fb_height, fb_pitch;
extern const unsigned char font_8x16[96][16];
extern char* keyboard_readline_poll(void);

// ===== APP STATE =====
static char tm_code[4096] = "turtle-appname MyApp\nmask-up green\nsewer-style black\nturtle-window \"My TMNT App\" 600 400\n";
static int code_len = 0;
static char app_name[64] = "MyTMNTApp";
static char icon_name[64] = "MyApp";
static char status_msg[256] = "Ready. Type your TMNT code below.";

// Cursor for code editing
static int cursor_line = 0;
static int cursor_col = 0;
static int scroll_offset = 0;

// ===== DRAWING HELPERS =====
static void mk_putpixel(int x, int y, uint32_t color) {
    if(x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height)
        fb[y * (fb_pitch / 4) + x] = color;
}

static void mk_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for(int dy = 0; dy < h; dy++)
        for(int dx = 0; dx < w; dx++)
            mk_putpixel(x + dx, y + dy, color);
}

static void mk_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for(int i = 0; i < w; i++) { mk_putpixel(x+i, y, color); mk_putpixel(x+i, y+h-1, color); }
    for(int i = 0; i < h; i++) { mk_putpixel(x, y+i, color); mk_putpixel(x+w-1, y+i, color); }
}

static void mk_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    if(c < 32 || c > 126) return;
    const unsigned char* glyph = font_8x16[c - 32];
    for(int row = 0; row < 16; row++) {
        unsigned char line = glyph[row];
        for(int col = 0; col < 8; col++)
            mk_putpixel(x + col, y + row, (line & (0x80 >> col)) ? fg : bg);
    }
}

static void mk_draw_text(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    int cx = x;
    while(*str) {
        if(*str == '\n') { cx = x; y += 16; }
        else { mk_draw_char(cx, y, *str, fg, bg); cx += 8; }
        str++;
    }
}

// ===== BUTTON STATE =====
#define MAX_BTN 16
static int btn_count = 0;
static int btn_x[MAX_BTN], btn_y[MAX_BTN], btn_w[MAX_BTN], btn_h[MAX_BTN];
static char btn_label[MAX_BTN][32];
static int btn_action[MAX_BTN]; // 0=compile, 1=run, 2=clear, 3=save, 4=load

// ===== DRAW FUNCTION =====
static void maker_draw(int wx, int wy, int ww, int wh) {
    // Title area
    mk_fill_rect(wx + 10, wy + 30, ww - 20, 25, 0x0A1A0A);
    mk_draw_rect(wx + 10, wy + 30, ww - 20, 25, 0x00AA00);
    mk_draw_text(wx + 15, wy + 35, "🐢 TMNT App Maker - Create Your Own Apps!", 0x00FF00, 0x0A1A0A);
    
    // App Name field
    mk_draw_text(wx + 15, wy + 65, "App Name:", 0x888888, 0x1A1A2E);
    mk_fill_rect(wx + 100, wy + 62, 200, 20, 0x000000);
    mk_draw_rect(wx + 100, wy + 62, 200, 20, 0x00AA00);
    mk_draw_text(wx + 105, wy + 64, app_name, 0x00FF00, 0x000000);
    
    // Icon Name field
    mk_draw_text(wx + 320, wy + 65, "Icon:", 0x888888, 0x1A1A2E);
    mk_fill_rect(wx + 370, wy + 62, 150, 20, 0x000000);
    mk_draw_rect(wx + 370, wy + 62, 150, 20, 0x00AA00);
    mk_draw_text(wx + 375, wy + 64, icon_name, 0x00FF00, 0x000000);
    
    // Code editor area
    int code_x = wx + 15;
    int code_y = wy + 95;
    int code_w = ww - 30;
    int code_h = wh - 195;
    
    mk_fill_rect(code_x, code_y, code_w, code_h, 0x000000);
    mk_draw_rect(code_x, code_y, code_w, code_h, 0x00AA00);
    mk_draw_text(code_x + 5, code_y + 3, "TMNT Code:", 0x888888, 0x000000);
    
    // Draw code text
    mk_draw_text(code_x + 5, code_y + 20, tm_code, 0x00FF00, 0x000000);
    
    // Draw cursor
    int cl = 0, cc = 0;
    for(int i = 0; i < cursor_line && tm_code[i]; i++) {
        if(tm_code[i] == '\n') { cl++; cc = 0; }
        else cc++;
    }
    mk_fill_rect(code_x + 5 + cc * 8, code_y + 20 + cl * 16, 8, 16, 0x00FF00);
    
    // Status bar
    mk_fill_rect(wx + 10, wy + wh - 60, ww - 20, 25, 0x0A0A0A);
    mk_draw_rect(wx + 10, wy + wh - 60, ww - 20, 25, 0x00AA00);
    mk_draw_text(wx + 15, wy + wh - 55, status_msg, 0xFFFF00, 0x0A0A0A);
    
    // Buttons
    btn_count = 0;
    int btn_y_start = wy + wh - 30;
    
    btn_x[btn_count] = wx + 15; btn_y[btn_count] = btn_y_start;
    btn_w[btn_count] = 100; btn_h[btn_count] = 22;
    strcpy(btn_label[btn_count], "Compile");
    btn_action[btn_count] = 0;
    btn_count++;
    
    btn_x[btn_count] = wx + 125; btn_y[btn_count] = btn_y_start;
    btn_w[btn_count] = 80; btn_h[btn_count] = 22;
    strcpy(btn_label[btn_count], "Run Test");
    btn_action[btn_count] = 1;
    btn_count++;
    
    btn_x[btn_count] = wx + 215; btn_y[btn_count] = btn_y_start;
    btn_w[btn_count] = 80; btn_h[btn_count] = 22;
    strcpy(btn_label[btn_count], "Clear");
    btn_action[btn_count] = 2;
    btn_count++;
    
    btn_x[btn_count] = wx + 305; btn_y[btn_count] = btn_y_start;
    btn_w[btn_count] = 80; btn_h[btn_count] = 22;
    strcpy(btn_label[btn_count], "Save .tm");
    btn_action[btn_count] = 3;
    btn_count++;
    
    btn_x[btn_count] = wx + 395; btn_y[btn_count] = btn_y_start;
    btn_w[btn_count] = 80; btn_h[btn_count] = 22;
    strcpy(btn_label[btn_count], "Load .tm");
    btn_action[btn_count] = 4;
    btn_count++;
    
    // Draw buttons
    for(int i = 0; i < btn_count; i++) {
        uint32_t bg = 0x2A2A3A;
        mk_fill_rect(btn_x[i], btn_y[i], btn_w[i], btn_h[i], bg);
        mk_draw_rect(btn_x[i], btn_y[i], btn_w[i], btn_h[i], 0x00AA00);
        int tx = btn_x[i] + (btn_w[i] - strlen(btn_label[i]) * 8) / 2;
        int ty = btn_y[i] + (btn_h[i] - 16) / 2;
        mk_draw_text(tx, ty, btn_label[i], 0xFFFFFF, bg);
    }
}

// ===== CLICK HANDLER =====
static void maker_click(int mx, int my) {
    for(int i = 0; i < btn_count; i++) {
        if(mx >= btn_x[i] && mx < btn_x[i] + btn_w[i] &&
           my >= btn_y[i] && my < btn_y[i] + btn_h[i]) {
            
            switch(btn_action[i]) {
                case 0: // Compile
                    if(strlen(app_name) == 0) {
                        strcpy(status_msg, "Error: Set an app name first!");
                    } else {
                        int result = tm_compile_to_app(tm_code, app_name, icon_name);
                        if(result == 0) {
                            strcpy(status_msg, "✅ App compiled! Reboot to see it on desktop.");
                        } else {
                            strcpy(status_msg, "❌ Compile failed. Check your code.");
                        }
                    }
                    break;
                    
                case 1: // Run Test
                    strcpy(status_msg, "🚀 Running app...");
                    tm_execute(tm_code);
                    strcpy(status_msg, "App closed. Ready.");
                    break;
                    
                case 2: // Clear
                    tm_code[0] = '\0';
                    code_len = 0;
                    cursor_line = 0;
                    cursor_col = 0;
                    strcpy(status_msg, "Code cleared.");
                    break;
                    
                case 3: // Save .tm
                    {
                        char path[128] = "/user/";
                        strcat(path, app_name);
                        strcat(path, ".tm");
                        fs_write_file(path, (uint8_t*)tm_code, strlen(tm_code));
                        strcpy(status_msg, "💾 Saved to ");
                        strcat(status_msg, path);
                    }
                    break;
                    
                case 4: // Load .tm
                    {
                        char path[128] = "/user/";
                        strcat(path, app_name);
                        strcat(path, ".tm");
                        if(fs_file_exists(path)) {
                            uint32_t sz = fs_get_file_size(path);
                            if(sz < 4096) {
                                fs_read_file(path, (uint8_t*)tm_code, sz);
                                tm_code[sz] = '\0';
                                code_len = sz;
                                strcpy(status_msg, "📂 Loaded from ");
                                strcat(status_msg, path);
                            }
                        } else {
                            strcpy(status_msg, "❌ File not found: ");
                            strcat(status_msg, path);
                        }
                    }
                    break;
            }
        }
    }
}

// ===== OPEN FUNCTION =====
void tmnt_maker_open(void) {
    // Initialize code length
    code_len = strlen(tm_code);
    
    // Set default name if empty
    if(app_name[0] == '\0') strcpy(app_name, "MyTMNTApp");
    if(icon_name[0] == '\0') strcpy(icon_name, "MyApp");
    
    gui_run_auto_app("🐢 TMNT App Maker", 50, 30, 750, 500, maker_draw, maker_click);
}