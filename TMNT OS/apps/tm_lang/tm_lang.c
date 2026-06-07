#include "tm_lang.h"
#include "../../drivers/vga/vga.h"
#include "../../external/gui.h"
#include "../../fs/tmnt_fs.h"
#include "../../system/memory.h"
#include "../../system/string.h"

// Keyboard functions from kernel
extern char* keyboard_readline_poll(void);

// GUI framebuffer
extern uint32_t* fb;
extern uint32_t fb_width, fb_height, fb_pitch;
extern const unsigned char font_8x16[96][16];

// Freestanding strncmp replacement
static int tm_strncmp(const char* a, const char* b, int n) {
    while(n-- && *a && *b) {
        if(*a != *b) return *a - *b;
        a++; b++;
    }
    if(n < 0) return 0;
    return *a - *b;
}
#define strncmp tm_strncmp

// ===== TMNT OPCODES =====
#define TM_PRINT      0x01
#define TM_COLOR      0x02
#define TM_BG         0x03
#define TM_RECT       0x04
#define TM_LINE       0x05
#define TM_CIRCLE     0x06
#define TM_WAIT       0x07
#define TM_CLEAR      0x08
#define TM_INPUT      0x09
#define TM_IF         0x0A
#define TM_GOTO       0x0B
#define TM_LABEL      0x0C
#define TM_SOUND      0x0D
#define TM_EXIT       0x0E
#define TM_SHELL      0x0F
#define TM_WINDOW     0x10
#define TM_CANVAS     0x11
#define TM_TEXT       0x12
#define TM_PIXEL      0x13
#define TM_SQUARE     0x14
#define TM_INSTALL    0x15
#define TM_APPNAME    0x16
#define TM_ICONNAME   0x17
#define TM_COMPILE    0x18
#define TM_RUN        0x19
#define TM_BUTTON     0x1A
#define TM_CLICK      0x1B

// ===== TMNT APP STATE =====
static uint32_t tm_fg_color = 0xFFFFFF;
static uint32_t tm_bg_color = 0x000000;
static char tm_vars[26][256];
static int tm_labels[128];
static char tm_label_names[128][32];
static int tm_label_count = 0;

// GUI window state for TMNT apps
static int tm_has_window = 0;
static int tm_win_x = 0, tm_win_y = 0, tm_win_w = 0, tm_win_h = 0;
static char tm_win_title[64] = "";
static char tm_app_name[64] = "";
static char tm_icon_name[64] = "";

// Click handler state
static int tm_button_count = 0;
static int tm_button_x[32], tm_button_y[32], tm_button_w[32], tm_button_h[32];
static char tm_button_label[32][32];
static char tm_button_action[32][64];

// ===== DRAWING HELPERS =====
static void tm_putpixel(int x, int y, uint32_t color) {
    if(x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height)
        fb[y * (fb_pitch / 4) + x] = color;
}

static void tm_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for(int dy = 0; dy < h; dy++)
        for(int dx = 0; dx < w; dx++)
            tm_putpixel(x + dx, y + dy, color);
}

static void tm_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for(int i = 0; i < w; i++) { tm_putpixel(x+i, y, color); tm_putpixel(x+i, y+h-1, color); }
    for(int i = 0; i < h; i++) { tm_putpixel(x, y+i, color); tm_putpixel(x+w-1, y+i, color); }
}

static void tm_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    if(c < 32 || c > 126) return;
    const unsigned char* glyph = font_8x16[c - 32];
    for(int row = 0; row < 16; row++) {
        unsigned char line = glyph[row];
        for(int col = 0; col < 8; col++)
            tm_putpixel(x + col, y + row, (line & (0x80 >> col)) ? fg : bg);
    }
}

static void tm_draw_text(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    int cx = x;
    while(*str) {
        if(*str == '\n') { cx = x; y += 16; }
        else { tm_draw_char(cx, y, *str, fg, bg); cx += 8; }
        str++;
    }
}

// ===== TMNT APP DRAW FUNCTION =====
static void tm_app_draw(int wx, int wy, int ww, int wh) {
    tm_win_x = wx; tm_win_y = wy; tm_win_w = ww; tm_win_h = wh;
    
    // Draw canvas background
    tm_fill_rect(wx + 10, wy + 30, ww - 20, wh - 50, tm_bg_color);
    tm_draw_rect(wx + 10, wy + 30, ww - 20, wh - 50, tm_fg_color);
    
    // Draw buttons
    for(int i = 0; i < tm_button_count; i++) {
        int bx = wx + tm_button_x[i];
        int by = wy + tm_button_y[i];
        tm_fill_rect(bx, by, tm_button_w[i], tm_button_h[i], 0x2A2A3A);
        tm_draw_rect(bx, by, tm_button_w[i], tm_button_h[i], tm_fg_color);
        int tx = bx + (tm_button_w[i] - strlen(tm_button_label[i]) * 8) / 2;
        int ty = by + (tm_button_h[i] - 16) / 2;
        tm_draw_text(tx, ty, tm_button_label[i], 0xFFFFFF, 0x2A2A3A);
    }
}

// ===== TMNT APP CLICK HANDLER =====
static void tm_app_click(int mx, int my) {
    int rx = mx - tm_win_x;
    int ry = my - tm_win_y;
    
    for(int i = 0; i < tm_button_count; i++) {
        if(rx >= tm_button_x[i] && rx < tm_button_x[i] + tm_button_w[i] &&
           ry >= tm_button_y[i] && ry < tm_button_y[i] + tm_button_h[i]) {
            // Execute the action for this button
            if(tm_button_action[i][0] != '\0') {
                tm_execute(tm_button_action[i]);
            }
        }
    }
}

// ===== TMNT APP OPEN FUNCTION =====
static void tm_app_open(void) {
    tm_button_count = 0;
    gui_run_auto_app(tm_win_title, 100, 80, 600, 400, tm_app_draw, tm_app_click);
}

// ===== PARSING FUNCTIONS =====
static int tm_parse_number(const char* str) {
    int result = 0;
    int sign = 1;
    if(*str == '-') { sign = -1; str++; }
    while(*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return result * sign;
}

static uint32_t tm_parse_color_name(const char* name) {
    if(strncmp(name, "green", 5) == 0) return 0x00FF00;
    else if(strncmp(name, "red", 3) == 0) return 0xFF0000;
    else if(strncmp(name, "blue", 4) == 0) return 0x0000FF;
    else if(strncmp(name, "purple", 6) == 0) return 0x800080;
    else if(strncmp(name, "orange", 6) == 0) return 0xFFA500;
    else if(strncmp(name, "white", 5) == 0) return 0xFFFFFF;
    else if(strncmp(name, "yellow", 6) == 0) return 0xFFFF00;
    else if(strncmp(name, "black", 5) == 0) return 0x000000;
    else if(strncmp(name, "gray", 4) == 0) return 0x808080;
    return 0xFFFFFF;
}

static void tm_parse_print(const char* line) {
    const char* text = line;
    while(*text && *text != ' ') text++;
    if(*text == ' ') text++;
    
    if(text[0] == '$' && text[1] >= 'A' && text[1] <= 'Z') {
        if(tm_has_window) {
            tm_draw_text(tm_win_x + 15, tm_win_y + 50, tm_vars[text[1] - 'A'], tm_fg_color, tm_bg_color);
        } else {
            vga_print(tm_vars[text[1] - 'A']);
        }
    } else {
        if(tm_has_window) {
            tm_draw_text(tm_win_x + 15, tm_win_y + 50, text, tm_fg_color, tm_bg_color);
        } else {
            vga_print(text);
        }
    }
}

static void tm_parse_color(const char* line) {
    const char* color = line;
    while(*color && *color != ' ') color++;
    if(*color == ' ') color++;
    tm_fg_color = tm_parse_color_name(color);
    if(!tm_has_window) {
        if(tm_fg_color == 0x00FF00) vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
        else if(tm_fg_color == 0xFF0000) vga_set_fg_color(VGA_COLOR_LIGHT_RED);
        else vga_set_fg_color(VGA_COLOR_WHITE);
    }
}

static void tm_parse_bg(const char* line) {
    const char* color = line;
    while(*color && *color != ' ') color++;
    if(*color == ' ') color++;
    tm_bg_color = tm_parse_color_name(color);
    if(!tm_has_window) {
        if(tm_bg_color == 0x000000) vga_set_bg_color(VGA_COLOR_BLACK);
        else vga_set_bg_color(VGA_COLOR_BLACK);
    }
}

static void tm_parse_input(const char* line) {
    const char* ptr = line;
    while(*ptr && *ptr != ' ') ptr++;
    if(*ptr == ' ') ptr++;
    
    if(ptr[0] == '$' && ptr[1] >= 'A' && ptr[1] <= 'Z') {
        int var_idx = ptr[1] - 'A';
        ptr += 2;
        if(*ptr == ' ') ptr++;
        
        vga_print(ptr);
        char* input = keyboard_readline_poll();
        strcpy(tm_vars[var_idx], input);
    }
}

static void tm_parse_window(const char* line) {
    const char* ptr = line;
    while(*ptr && *ptr != ' ') ptr++;
    if(*ptr == ' ') ptr++;
    
    // Parse title (in quotes)
    if(*ptr == '"') {
        ptr++;
        int i = 0;
        while(*ptr && *ptr != '"' && i < 63) {
            tm_win_title[i++] = *ptr++;
        }
        tm_win_title[i] = '\0';
        if(*ptr == '"') ptr++;
    }
    
    // Parse width
    while(*ptr && *ptr == ' ') ptr++;
    int w = tm_parse_number(ptr);
    while(*ptr && *ptr != ' ') ptr++;
    if(*ptr == ' ') ptr++;
    
    // Parse height
    int h = tm_parse_number(ptr);
    
    tm_win_w = w > 0 ? w : 600;
    tm_win_h = h > 0 ? h : 400;
    tm_has_window = 1;
}

static void tm_parse_button(const char* line) {
    if(tm_button_count >= 32) return;
    
    const char* ptr = line;
    while(*ptr && *ptr != ' ') ptr++;
    if(*ptr == ' ') ptr++;
    
    // Parse x y w h
    tm_button_x[tm_button_count] = tm_parse_number(ptr);
    while(*ptr && *ptr != ' ') ptr++; if(*ptr == ' ') ptr++;
    tm_button_y[tm_button_count] = tm_parse_number(ptr);
    while(*ptr && *ptr != ' ') ptr++; if(*ptr == ' ') ptr++;
    tm_button_w[tm_button_count] = tm_parse_number(ptr);
    while(*ptr && *ptr != ' ') ptr++; if(*ptr == ' ') ptr++;
    tm_button_h[tm_button_count] = tm_parse_number(ptr);
    while(*ptr && *ptr != ' ') ptr++; if(*ptr == ' ') ptr++;
    
    // Parse label (in quotes)
    if(*ptr == '"') {
        ptr++;
        int i = 0;
        while(*ptr && *ptr != '"' && i < 31) {
            tm_button_label[tm_button_count][i++] = *ptr++;
        }
        tm_button_label[tm_button_count][i] = '\0';
        if(*ptr == '"') ptr++;
    }
    
    tm_button_action[tm_button_count][0] = '\0';
    tm_button_count++;
}

static void tm_parse_canvas(const char* line) {
    const char* ptr = line;
    while(*ptr && *ptr != ' ') ptr++;
    if(*ptr == ' ') ptr++;
    
    int x = tm_parse_number(ptr);
    while(*ptr && *ptr != ' ') ptr++; if(*ptr == ' ') ptr++;
    int y = tm_parse_number(ptr);
    while(*ptr && *ptr != ' ') ptr++; if(*ptr == ' ') ptr++;
    int w = tm_parse_number(ptr);
    while(*ptr && *ptr != ' ') ptr++; if(*ptr == ' ') ptr++;
    int h = tm_parse_number(ptr);
    while(*ptr && *ptr != ' ') ptr++; if(*ptr == ' ') ptr++;
    
    uint32_t color = tm_parse_color_name(ptr);
    
    if(tm_has_window) {
        tm_fill_rect(tm_win_x + x, tm_win_y + y, w, h, color);
    }
}

static void tm_parse_square(const char* line) {
    const char* ptr = line;
    while(*ptr && *ptr != ' ') ptr++;
    if(*ptr == ' ') ptr++;
    
    int x = tm_parse_number(ptr);
    while(*ptr && *ptr != ' ') ptr++; if(*ptr == ' ') ptr++;
    int y = tm_parse_number(ptr);
    while(*ptr && *ptr != ' ') ptr++; if(*ptr == ' ') ptr++;
    int w = tm_parse_number(ptr);
    while(*ptr && *ptr != ' ') ptr++; if(*ptr == ' ') ptr++;
    int h = tm_parse_number(ptr);
    
    if(tm_has_window) {
        tm_draw_rect(tm_win_x + x, tm_win_y + y, w, h, tm_fg_color);
    }
}

static void tm_parse_text(const char* line) {
    const char* ptr = line;
    while(*ptr && *ptr != ' ') ptr++;
    if(*ptr == ' ') ptr++;
    
    int x = tm_parse_number(ptr);
    while(*ptr && *ptr != ' ') ptr++; if(*ptr == ' ') ptr++;
    int y = tm_parse_number(ptr);
    while(*ptr && *ptr != ' ') ptr++; if(*ptr == ' ') ptr++;
    
    // Parse text (in quotes)
    char text[256];
    if(*ptr == '"') {
        ptr++;
        int i = 0;
        while(*ptr && *ptr != '"' && i < 255) {
            text[i++] = *ptr++;
        }
        text[i] = '\0';
    }
    
    if(tm_has_window) {
        tm_draw_text(tm_win_x + x, tm_win_y + y, text, tm_fg_color, tm_bg_color);
    }
}

static void tm_parse_pixel(const char* line) {
    const char* ptr = line;
    while(*ptr && *ptr != ' ') ptr++;
    if(*ptr == ' ') ptr++;
    
    int x = tm_parse_number(ptr);
    while(*ptr && *ptr != ' ') ptr++; if(*ptr == ' ') ptr++;
    int y = tm_parse_number(ptr);
    
    if(tm_has_window) {
        tm_putpixel(tm_win_x + x, tm_win_y + y, tm_fg_color);
    }
}

static void tm_parse_install(const char* line) {
    const char* ptr = line;
    while(*ptr && *ptr != ' ') ptr++;
    if(*ptr == ' ') ptr++;
    
    // Parse app name
    int i = 0;
    while(*ptr && *ptr != ' ' && i < 63) {
        tm_app_name[i++] = *ptr++;
    }
    tm_app_name[i] = '\0';
}

static void tm_parse_appname(const char* line) {
    const char* ptr = line;
    while(*ptr && *ptr != ' ') ptr++;
    if(*ptr == ' ') ptr++;
    
    int i = 0;
    while(*ptr && *ptr != ' ' && *ptr != '\n' && i < 63) {
        tm_app_name[i++] = *ptr++;
    }
    tm_app_name[i] = '\0';
}

static void tm_parse_iconname(const char* line) {
    const char* ptr = line;
    while(*ptr && *ptr != ' ') ptr++;
    if(*ptr == ' ') ptr++;
    
    int i = 0;
    while(*ptr && *ptr != ' ' && *ptr != '\n' && i < 63) {
        tm_icon_name[i++] = *ptr++;
    }
    tm_icon_name[i] = '\0';
}

static void tm_parse_compile(const char* line) {
    // Compile current state to a .TME file
    if(tm_app_name[0] == '\0') {
        vga_print("Error: Use 'turtle-name' first to set app name\n");
        return;
    }
    if(tm_icon_name[0] == '\0') {
        // Use app name as icon name
        strcpy(tm_icon_name, tm_app_name);
    }
    
    char path[128];
    int p = 0;
    path[p++] = '/'; path[p++] = 'a'; path[p++] = 'p'; path[p++] = 'p';
    path[p++] = 's'; path[p++] = '/';
    int n = 0;
    while(tm_app_name[n] && p < 120) path[p++] = tm_app_name[n++];
    path[p++] = '/';
    n = 0;
    while(tm_app_name[n] && p < 120) path[p++] = tm_app_name[n++];
    path[p++] = '.'; path[p++] = 't'; path[p++] = 'm'; path[p++] = 'e';
    path[p] = '\0';
    
    fs_create_dir("/apps");
    char dirpath[64] = "/apps/";
    strcat(dirpath, tm_app_name);
    fs_create_dir(dirpath);
    
    // Create the TME file with app metadata
    uint8_t tme_data[512];
    int pos = 0;
    
    // Magic: TMAPP
    tme_data[pos++] = 'T'; tme_data[pos++] = 'M'; tme_data[pos++] = 'A';
    tme_data[pos++] = 'P'; tme_data[pos++] = 'P';
    
    // App name length + name
    int name_len = strlen(tm_app_name);
    tme_data[pos++] = name_len;
    for(int i = 0; i < name_len; i++) tme_data[pos++] = tm_app_name[i];
    
    // Icon name length + name
    int icon_len = strlen(tm_icon_name);
    tme_data[pos++] = icon_len;
    for(int i = 0; i < icon_len; i++) tme_data[pos++] = tm_icon_name[i];
    
    // Window title
    int title_len = strlen(tm_win_title);
    tme_data[pos++] = title_len;
    for(int i = 0; i < title_len; i++) tme_data[pos++] = tm_win_title[i];
    
    // Window dimensions
    tme_data[pos++] = (tm_win_w >> 8) & 0xFF;
    tme_data[pos++] = tm_win_w & 0xFF;
    tme_data[pos++] = (tm_win_h >> 8) & 0xFF;
    tme_data[pos++] = tm_win_h & 0xFF;
    
    fs_write_file(path, tme_data, pos);
    vga_print("✅ App compiled: ");
    vga_print(path);
    vga_print("\nReboot to see it on the desktop!\n");
}

// ===== MAIN EXECUTION ENGINE =====
void tm_execute(const char* source) {
    char lines[256][256];
    int line_count = 0;
    
    // Split source into lines
    int src_pos = 0;
    int line_start = 0;
    while(source[src_pos]) {
        if(source[src_pos] == '\n' || source[src_pos] == '\r') {
            int len = src_pos - line_start;
            if(len > 0 && len < 256) {
                for(int i = 0; i < len; i++) lines[line_count][i] = source[line_start + i];
                lines[line_count][len] = '\0';
                line_count++;
            }
            line_start = src_pos + 1;
            if(source[src_pos] == '\r' && source[src_pos+1] == '\n') src_pos++;
        }
        src_pos++;
    }
    if(src_pos > line_start) {
        int len = src_pos - line_start;
        if(len > 0 && len < 256) {
            for(int i = 0; i < len; i++) lines[line_count][i] = source[line_start + i];
            lines[line_count][len] = '\0';
            line_count++;
        }
    }
    
    // Find labels
    tm_label_count = 0;
    for(int i = 0; i < line_count; i++) {
        if(lines[i][0] == ':') {
            strcpy(tm_label_names[tm_label_count], lines[i] + 1);
            tm_labels[tm_label_count] = i;
            tm_label_count++;
        }
    }
    
    // Execute lines
    int current_line = 0;
    while(current_line < line_count) {
        char* line = lines[current_line];
        
        if(line[0] == '\0' || line[0] == ':') {
            current_line++;
            continue;
        }
        
        // OLD COMMANDS (backward compatible)
        if(strncmp(line, "cowabunga ", 10) == 0) tm_parse_print(line);
        else if(strncmp(line, "shell-shock ", 12) == 0) tm_parse_print(line);
        else if(strncmp(line, "mask-up ", 8) == 0) tm_parse_color(line);
        else if(strncmp(line, "sewer-style ", 12) == 0) tm_parse_bg(line);
        else if(strncmp(line, "ask ", 4) == 0) tm_parse_input(line);
        else if(strncmp(line, "pizza-time ", 11) == 0) {
            int ms = tm_parse_number(line + 11);
            for(volatile int i = 0; i < ms * 1000; i++);
        }
        else if(strncmp(line, "turtle-vanish", 13) == 0) vga_clear_screen();
        else if(strncmp(line, "shell-save", 10) == 0) break;
        else if(strncmp(line, "sewer-surf ", 11) == 0) {
            for(int i = 0; i < tm_label_count; i++) {
                if(strcmp(line + 11, tm_label_names[i]) == 0) {
                    current_line = tm_labels[i];
                    break;
                }
            }
        }
        
        // NEW GUI WINDOW COMMANDS
        else if(strncmp(line, "turtle-window ", 14) == 0) tm_parse_window(line);
        else if(strncmp(line, "turtle-canvas ", 14) == 0) tm_parse_canvas(line);
        else if(strncmp(line, "turtle-text ", 12) == 0) tm_parse_text(line);
        else if(strncmp(line, "turtle-pixel ", 13) == 0) tm_parse_pixel(line);
        else if(strncmp(line, "turtle-square ", 14) == 0) tm_parse_square(line);
        else if(strncmp(line, "turtle-button ", 14) == 0) tm_parse_button(line);
        else if(strncmp(line, "turtle-appname ", 15) == 0) tm_parse_appname(line);
        else if(strncmp(line, "turtle-icon ", 12) == 0) tm_parse_iconname(line);
        else if(strncmp(line, "turtle-compile", 14) == 0) tm_parse_compile(line);
        else if(strncmp(line, "turtle-run", 10) == 0) {
            if(tm_has_window) tm_app_open();
        }
        else if(strncmp(line, "turtle-install ", 15) == 0) tm_parse_install(line);
        
        current_line++;
    }
    
    // If a window was created, open it automatically
    if(tm_has_window) {
        tm_app_open();
    }
}

// ===== FILE EXECUTION =====
void tm_run_file(const char* filepath) {
    if(!fs_file_exists(filepath)) {
        vga_print("TM file not found: ");
        vga_print(filepath);
        vga_print("\n");
        return;
    }
    
    uint32_t sz = fs_get_file_size(filepath);
    if(sz == 0 || sz > 65536) {
        vga_print("Invalid file size\n");
        return;
    }
    
    uint8_t* source = (uint8_t*)malloc(sz + 1);
    fs_read_file(filepath, source, sz);
    source[sz] = '\0';
    
    // Reset state
    tm_has_window = 0;
    tm_button_count = 0;
    tm_fg_color = 0xFFFFFF;
    tm_bg_color = 0x000000;
    tm_app_name[0] = '\0';
    tm_icon_name[0] = '\0';
    
    // Check for TME format
    if(sz > 5 && source[0] == 'T' && source[1] == 'M' && source[2] == 'A' && source[3] == 'P' && source[4] == 'P') {
        int pos = 5;
        
        // Read app name
        int name_len = source[pos++];
        for(int i = 0; i < name_len && i < 63; i++) tm_app_name[i] = source[pos++];
        tm_app_name[name_len] = '\0';
        
        // Read icon name
        int icon_len = source[pos++];
        for(int i = 0; i < icon_len && i < 63; i++) tm_icon_name[i] = source[pos++];
        tm_icon_name[icon_len] = '\0';
        
        // Read window title
        int title_len = source[pos++];
        for(int i = 0; i < title_len && i < 63; i++) tm_win_title[i] = source[pos++];
        tm_win_title[title_len] = '\0';
        
        // Read window dimensions
        tm_win_w = (source[pos++] << 8) | source[pos++];
        tm_win_h = (source[pos++] << 8) | source[pos++];
        
        tm_has_window = 1;
        tm_app_open();
    } else if(sz > 8 && source[0] == 'T' && source[1] == 'M' && source[2] == 'G' && source[3] == 'I') {
        // Legacy TMGI format
        uint32_t offset = 9;
        uint8_t name_len = source[8];
        offset += name_len;
        uint32_t code_size = source[offset] | (source[offset+1] << 8) | 
                            (source[offset+2] << 16) | (source[offset+3] << 24);
        offset += 4;
        
        char* code = (char*)(source + offset);
        code[code_size] = '\0';
        tm_execute(code);
    } else {
        tm_execute((char*)source);
    }
    
    free(source);
}

// ===== COMPILE TO STANDALONE APP =====
int tm_compile_to_app(const char* source, const char* app_name, const char* icon_name) {
    // Reset state
    tm_has_window = 0;
    tm_button_count = 0;
    tm_fg_color = 0xFFFFFF;
    tm_bg_color = 0x000000;
    strcpy(tm_app_name, app_name);
    strcpy(tm_icon_name, icon_name);
    
    // Execute the source to set up the window state
    tm_execute(source);
    
    if(!tm_has_window) return -1;
    
    // Now compile
    tm_parse_compile("");
    return 0;
}

int tm_install_app(const char* tme_path) {
    if(!fs_file_exists(tme_path)) return -1;
    
    uint32_t sz = fs_get_file_size(tme_path);
    if(sz == 0 || sz > 512) return -1;
    
    uint8_t* data = (uint8_t*)malloc(sz);
    fs_read_file(tme_path, data, sz);
    
    if(data[0] != 'T' || data[1] != 'M' || data[2] != 'A' || data[3] != 'P' || data[4] != 'P') {
        free(data);
        return -1;
    }
    
    // Extract app name from TME
    int pos = 5;
    int name_len = data[pos++];
    char app_name[64];
    for(int i = 0; i < name_len && i < 63; i++) app_name[i] = data[pos++];
    app_name[name_len] = '\0';
    
    // Create app directory and copy
    char dirpath[64] = "/apps/";
    strcat(dirpath, app_name);
    fs_create_dir(dirpath);
    
    char destpath[128];
    strcpy(destpath, dirpath);
    strcat(destpath, "/");
    strcat(destpath, app_name);
    strcat(destpath, ".tme");
    
    fs_write_file(destpath, data, sz);
    free(data);
    
    return 0;
}