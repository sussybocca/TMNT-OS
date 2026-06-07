#include "calculator.h"
#include "external/gui.h"
#include "system/string.h"

// Calculator state
static int display_value;
static int stored_value;
static char current_op;
static int clear_display;
static char display_str[32];

// Current window position (updated by draw)
static int calc_wx, calc_wy;

// Button layout
typedef struct {
    int x, y, w, h;
    char label[8];
    int is_op;
} calc_btn_t;

#define BTN_W 50
#define BTN_H 35
static calc_btn_t buttons[20];
static int btn_count;

static void num_to_str(int n, char* out) {
    int i = 0;
    if(n == 0) { out[0] = '0'; out[1] = 0; return; }
    if(n < 0) { out[i++] = '-'; n = -n; }
    int temp[16], ti = 0;
    while(n > 0) { temp[ti++] = n % 10; n /= 10; }
    while(ti > 0) { out[i++] = '0' + temp[--ti]; }
    out[i] = 0;
}

void calculator_init(void) {
    display_value = 0;
    stored_value = 0;
    current_op = 0;
    clear_display = 1;
    display_str[0] = '0'; display_str[1] = 0;
    calc_wx = 0; calc_wy = 0;
    
    btn_count = 0;
    buttons[btn_count++] = (calc_btn_t){10, 80, BTN_W, BTN_H, "7", 0};
    buttons[btn_count++] = (calc_btn_t){65, 80, BTN_W, BTN_H, "8", 0};
    buttons[btn_count++] = (calc_btn_t){120, 80, BTN_W, BTN_H, "9", 0};
    buttons[btn_count++] = (calc_btn_t){175, 80, BTN_W, BTN_H, "/", 1};
    buttons[btn_count++] = (calc_btn_t){10, 120, BTN_W, BTN_H, "4", 0};
    buttons[btn_count++] = (calc_btn_t){65, 120, BTN_W, BTN_H, "5", 0};
    buttons[btn_count++] = (calc_btn_t){120, 120, BTN_W, BTN_H, "6", 0};
    buttons[btn_count++] = (calc_btn_t){175, 120, BTN_W, BTN_H, "*", 1};
    buttons[btn_count++] = (calc_btn_t){10, 160, BTN_W, BTN_H, "1", 0};
    buttons[btn_count++] = (calc_btn_t){65, 160, BTN_W, BTN_H, "2", 0};
    buttons[btn_count++] = (calc_btn_t){120, 160, BTN_W, BTN_H, "3", 0};
    buttons[btn_count++] = (calc_btn_t){175, 160, BTN_W, BTN_H, "-", 1};
    buttons[btn_count++] = (calc_btn_t){10, 200, BTN_W, BTN_H, "0", 0};
    buttons[btn_count++] = (calc_btn_t){65, 200, BTN_W, BTN_H, "C", 1};
    buttons[btn_count++] = (calc_btn_t){120, 200, BTN_W, BTN_H, "=", 1};
    buttons[btn_count++] = (calc_btn_t){175, 200, BTN_W, BTN_H, "+", 1};
    buttons[btn_count++] = (calc_btn_t){10, 240, 105, BTN_H, "AC", 1};
    buttons[btn_count++] = (calc_btn_t){120, 240, 105, BTN_H, "Del", 1};
}

void calculator_draw(int wx, int wy, int ww, int wh) {
    // Store window position for click handler
    calc_wx = wx;
    calc_wy = wy;
    
    // Display background
    gui_fb_fill_rect(wx + 10, wy + 30, ww - 20, 45, 0x0A2A0A);
    gui_fb_draw_rect(wx + 10, wy + 30, ww - 20, 45, 0x00AA00);
    
    // Display text
    num_to_str(display_value, display_str);
    gui_fb_draw_text(wx + 20, wy + 45, display_str, 0x00FF00, 0x0A2A0A);
    
    // Draw buttons
    for(int i = 0; i < btn_count; i++) {
        int bx = wx + buttons[i].x;
        int by = wy + buttons[i].y;
        uint32_t bg = buttons[i].is_op ? 0x444400 : 0x2A2A3A;
        gui_fb_fill_rect(bx, by, buttons[i].w, buttons[i].h, bg);
        gui_fb_draw_rect(bx, by, buttons[i].w, buttons[i].h, 0x00AA00);
        int tx = bx + (buttons[i].w - strlen(buttons[i].label) * 8) / 2;
        int ty = by + (buttons[i].h - 16) / 2;
        gui_fb_draw_text(tx, ty, buttons[i].label, 0xFFFFFF, bg);
    }
}

void calculator_click(int mx, int my) {
    // Convert absolute mouse coords to window-relative
    int rx = mx - calc_wx;
    int ry = my - calc_wy;
    
    for(int i = 0; i < btn_count; i++) {
        if(rx >= buttons[i].x && rx < buttons[i].x + buttons[i].w &&
           ry >= buttons[i].y && ry < buttons[i].y + buttons[i].h) {
            
            char* label = buttons[i].label;
            
            // Number buttons 0-9
            if(label[0] >= '0' && label[0] <= '9' && label[1] == 0) {
                int digit = label[0] - '0';
                if(clear_display) {
                    display_value = digit;
                    clear_display = 0;
                } else {
                    if(display_value >= 0 && display_value < 100000000) {
                        display_value = display_value * 10 + digit;
                    } else if(display_value < 0 && display_value > -100000000) {
                        display_value = display_value * 10 - digit;
                    }
                }
                return;
            }
            
            // Operators
            if(label[0] == '+') { stored_value = display_value; current_op = '+'; clear_display = 1; return; }
            if(label[0] == '-') { stored_value = display_value; current_op = '-'; clear_display = 1; return; }
            if(label[0] == '*') { stored_value = display_value; current_op = '*'; clear_display = 1; return; }
            if(label[0] == '/') { stored_value = display_value; current_op = '/'; clear_display = 1; return; }
            
            // Equals
            if(label[0] == '=') {
                if(current_op == '+') display_value = stored_value + display_value;
                else if(current_op == '-') display_value = stored_value - display_value;
                else if(current_op == '*') display_value = stored_value * display_value;
                else if(current_op == '/') {
                    if(display_value != 0) display_value = stored_value / display_value;
                    else display_value = 0;
                }
                current_op = 0;
                clear_display = 1;
                return;
            }
            
            // Clear (C) - clear current entry
            if(label[0] == 'C') {
                display_value = 0;
                clear_display = 1;
                return;
            }
            
            // All Clear (AC) - clear everything
            if(label[0] == 'A' && label[1] == 'C') {
                display_value = 0;
                stored_value = 0;
                current_op = 0;
                clear_display = 1;
                return;
            }
            
            // Delete (Del) - delete last digit
            if(label[0] == 'D') {
                display_value = display_value / 10;
                return;
            }
        }
    }
}