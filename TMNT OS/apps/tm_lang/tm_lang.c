#include "tm_lang.h"
#include "../../drivers/vga/vga.h"
#include "../../fs/tmnt_fs.h"
#include "../../system/memory.h"
#include "../../system/string.h"

// Keyboard functions from kernel
extern char* keyboard_readline_poll(void);

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

#define TM_PRINT    0x01
#define TM_COLOR    0x02
#define TM_BG       0x03
#define TM_RECT     0x04
#define TM_LINE     0x05
#define TM_CIRCLE   0x06
#define TM_WAIT     0x07
#define TM_CLEAR    0x08
#define TM_INPUT    0x09
#define TM_IF       0x0A
#define TM_GOTO     0x0B
#define TM_LABEL    0x0C
#define TM_SOUND    0x0D
#define TM_EXIT     0x0E
#define TM_SHELL    0x0F

static uint32_t tm_fg_color = 0xFFFFFF;
static uint32_t tm_bg_color = 0x000000;
static char tm_vars[26][256];
static int tm_labels[128];
static char tm_label_names[128][32];
static int tm_label_count = 0;

static void tm_parse_print(const char* line) {
    const char* text = line;
    while(*text && *text != ' ') text++;
    if(*text == ' ') text++;
    
    if(text[0] == '$' && text[1] >= 'A' && text[1] <= 'Z') {
        vga_print(tm_vars[text[1] - 'A']);
    } else {
        vga_print(text);
    }
}

static void tm_parse_color(const char* line) {
    const char* color = line;
    while(*color && *color != ' ') color++;
    if(*color == ' ') color++;
    
    if(strncmp(color, "green", 5) == 0) tm_fg_color = 0x00FF00;
    else if(strncmp(color, "red", 3) == 0) tm_fg_color = 0xFF0000;
    else if(strncmp(color, "blue", 4) == 0) tm_fg_color = 0x0000FF;
    else if(strncmp(color, "purple", 6) == 0) tm_fg_color = 0x800080;
    else if(strncmp(color, "orange", 6) == 0) tm_fg_color = 0xFFA500;
    else if(strncmp(color, "white", 5) == 0) tm_fg_color = 0xFFFFFF;
    else if(strncmp(color, "yellow", 6) == 0) tm_fg_color = 0xFFFF00;
}

static void tm_parse_bg(const char* line) {
    const char* color = line;
    while(*color && *color != ' ') color++;
    if(*color == ' ') color++;
    
    if(strncmp(color, "black", 5) == 0) tm_bg_color = 0x000000;
    else if(strncmp(color, "green", 5) == 0) tm_bg_color = 0x001A0A;
    else if(strncmp(color, "purple", 6) == 0) tm_bg_color = 0x1A0020;
}

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

void tm_execute(const char* source) {
    char lines[256][256];
    int line_count = 0;
    
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
    
    tm_label_count = 0;
    for(int i = 0; i < line_count; i++) {
        if(lines[i][0] == ':') {
            strcpy(tm_label_names[tm_label_count], lines[i] + 1);
            tm_labels[tm_label_count] = i;
            tm_label_count++;
        }
    }
    
    int current_line = 0;
    while(current_line < line_count) {
        char* line = lines[current_line];
        
        if(line[0] == '\0' || line[0] == ':') {
            current_line++;
            continue;
        }
        
        if(strncmp(line, "cowabunga ", 10) == 0) {
            tm_parse_print(line);
        } else if(strncmp(line, "shell-shock ", 12) == 0) {
            tm_parse_print(line);
        } else if(strncmp(line, "mask-up ", 8) == 0) {
            tm_parse_color(line);
        } else if(strncmp(line, "sewer-style ", 12) == 0) {
            tm_parse_bg(line);
        } else if(strncmp(line, "ask ", 4) == 0) {
            tm_parse_input(line);
        } else if(strncmp(line, "pizza-time ", 11) == 0) {
            int ms = tm_parse_number(line + 11);
            for(volatile int i = 0; i < ms * 1000; i++);
        } else if(strncmp(line, "turtle-vanish", 13) == 0) {
            vga_clear_screen();
        } else if(strncmp(line, "shell-save", 10) == 0) {
            vga_print("✅ Program finished!\n");
            break;
        } else if(strncmp(line, "sewer-surf ", 11) == 0) {
            for(int i = 0; i < tm_label_count; i++) {
                if(strcmp(line + 11, tm_label_names[i]) == 0) {
                    current_line = tm_labels[i];
                    break;
                }
            }
        }
        
        current_line++;
    }
}

void tm_run_file(const char* filepath) {
    if(!fs_file_exists(filepath)) {
        vga_print("TM file not found: ");
        vga_print(filepath);
        vga_print("\n");
        return;
    }
    
    uint32_t sz = fs_get_file_size(filepath);
    if(sz == 0 || sz > 4096) {
        vga_print("Invalid file size\n");
        return;
    }
    
    uint8_t* source = (uint8_t*)malloc(sz + 1);
    fs_read_file(filepath, source, sz);
    source[sz] = '\0';
    
    if(sz > 8 && source[0] == 'T' && source[1] == 'M' && source[2] == 'G' && source[3] == 'I') {
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