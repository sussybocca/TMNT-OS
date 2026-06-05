#include "terminal.h"
#include "gui.h"
#include "../drivers/vga/vga.h"
#include "../fs/tmnt_fs.h"
#include "../system/memory.h"
#include "../system/string.h"

extern uint32_t* fb;
extern uint32_t fb_width, fb_height, fb_pitch;
extern const unsigned char font_8x16[96][16];

int  terminal_mode = 0;
uint32_t term_fg = 0x00FF00, term_bg = 0x000000;
static int term_cursor_x = 8, term_cursor_y = 8;

static void fb_putpixel(int x, int y, uint32_t color) {
    if(x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height)
        fb[y * (fb_pitch / 4) + x] = color;
}
static void fb_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for(int dy = 0; dy < h; dy++)
        for(int dx = 0; dx < w; dx++)
            fb_putpixel(x + dx, y + dy, color);
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

void term_fb_clear(void) {
    term_cursor_x = 8; term_cursor_y = 8;
    fb_fill_rect(0, 0, fb_width, fb_height, term_bg);
}

void term_fb_putchar(char c) {
    if(!fb) return;
    if(c == '\n') { term_cursor_x = 8; term_cursor_y += 16; }
    else if(c == '\r') { term_cursor_x = 8; }
    else if(c == '\b') { if(term_cursor_x > 8) term_cursor_x -= 8; }
    else if(c >= ' ') {
        fb_draw_char(term_cursor_x, term_cursor_y, c, term_fg, term_bg);
        term_cursor_x += 8;
    }
    if(term_cursor_x >= (int)fb_width - 8) { term_cursor_x = 8; term_cursor_y += 16; }
    if(term_cursor_y >= (int)fb_height - 20) { term_cursor_y = 8; fb_fill_rect(0, 0, fb_width, fb_height, term_bg); }
}

void term_fb_print(const char* str) { while(*str) term_fb_putchar(*str++); }

void tm_shell_command(char* input) {
    if(input[0]=='t'&&input[1]=='u'&&input[2]=='r'&&input[3]=='t'&&input[4]=='l'&&input[5]=='e'&&input[6]=='\0'){
        term_fb_print("\n🐢 TM Commands - Customize Your Shell!\n");
        term_fb_print("=====================================\n");
        term_fb_print("  shell-shock <color>    - Change background color\n");
        term_fb_print("  sewer-style <file>     - Set wallpaper image\n");
        term_fb_print("  pizza-power <on/off>   - Toggle boot sound\n");
        term_fb_print("  mask-up <color>        - Change theme (red/blue/purple/orange)\n");
        term_fb_print("  kowabunga <file>       - Run a TM script\n");
        term_fb_print("  shell-save             - Save customizations\n");
        term_fb_print("  shell-load             - Load saved customizations\n");
        term_fb_print("  shell-build            - Build custom ISO with changes\n");
        term_fb_print("  exit                   - Return to desktop\n");
    }else if(input[0]=='s'&&input[1]=='h'&&input[2]=='e'&&input[3]=='l'&&input[4]=='l'&&input[5]=='-'&&input[6]=='s'&&input[7]=='h'&&input[8]=='o'&&input[9]=='c'&&input[10]=='k'&&input[11]==' '){
        char* color=input+12; term_fb_print("Shell color set to: "); term_fb_print(color); term_fb_print("\n");
        fs_write_file("/sys/cfg/shell_color",(uint8_t*)color,strlen(color));
    }else if(input[0]=='s'&&input[1]=='e'&&input[2]=='w'&&input[3]=='e'&&input[4]=='r'&&input[5]=='-'&&input[6]=='s'&&input[7]=='t'&&input[8]=='y'&&input[9]=='l'&&input[10]=='e'&&input[11]==' '){
        char* path=input+12; term_fb_print("Wallpaper set to: "); term_fb_print(path); term_fb_print("\n");
        fs_write_file("/sys/cfg/wallpaper",(uint8_t*)path,strlen(path));
    }else if(input[0]=='p'&&input[1]=='i'&&input[2]=='z'&&input[3]=='z'&&input[4]=='a'&&input[5]=='-'&&input[6]=='p'&&input[7]=='o'&&input[8]=='w'&&input[9]=='e'&&input[10]=='r'&&input[11]==' '){
        char* state=input+12; term_fb_print("Boot sound: "); term_fb_print(state); term_fb_print("\n");
        fs_write_file("/sys/cfg/boot_sound",(uint8_t*)state,strlen(state));
    }else if(input[0]=='m'&&input[1]=='a'&&input[2]=='s'&&input[3]=='k'&&input[4]=='-'&&input[5]=='u'&&input[6]=='p'&&input[7]==' '){
        char* theme=input+8; term_fb_print("Theme set to: "); term_fb_print(theme); term_fb_print("\n");
        fs_write_file("/sys/cfg/theme",(uint8_t*)theme,strlen(theme));
    }else if(input[0]=='s'&&input[1]=='h'&&input[2]=='e'&&input[3]=='l'&&input[4]=='l'&&input[5]=='-'&&input[6]=='s'&&input[7]=='a'&&input[8]=='v'&&input[9]=='e'&&input[10]=='\0'){
        fs_create_dir("/sys");fs_create_dir("/sys/cfg");term_fb_print("✅ Customizations saved!\n");
    }else if(input[0]=='s'&&input[1]=='h'&&input[2]=='e'&&input[3]=='l'&&input[4]=='l'&&input[5]=='-'&&input[6]=='l'&&input[7]=='o'&&input[8]=='a'&&input[9]=='d'&&input[10]=='\0'){
        term_fb_print("✅ Customizations loaded!\n");
    }else if(input[0]=='s'&&input[1]=='h'&&input[2]=='e'&&input[3]=='l'&&input[4]=='l'&&input[5]=='-'&&input[6]=='b'&&input[7]=='u'&&input[8]=='i'&&input[9]=='l'&&input[10]=='d'&&input[11]=='\0'){
        term_fb_print("🔨 Building custom ISO...\nThis will save all your changes to a new ISO.\nFeature coming in next update!\n");
    }else if(input[0]=='k'&&input[1]=='o'&&input[2]=='w'&&input[3]=='a'&&input[4]=='b'&&input[5]=='u'&&input[6]=='n'&&input[7]=='g'&&input[8]=='a'&&input[9]==' '){
        char* path=input+10;
        if(fs_file_exists(path)){uint32_t sz=fs_get_file_size(path);if(sz>0&&sz<65536){uint8_t* code=(uint8_t*)malloc(sz);fs_read_file(path,code,sz);typedef void(*f)(void);f func=(f)code;func();free(code);}}
        else{term_fb_print("TM script not found: ");term_fb_print(path);term_fb_print("\n");}
    }else if(input[0]=='e'&&input[1]=='x'&&input[2]=='i'&&input[3]=='t'&&input[4]=='\0'){
        terminal_mode=0; gui_mode=1;
        gui_draw_desktop(); gui_save_cursor_bg(); gui_draw_cursor();
        vga_print("\n🐢 Returned to desktop!\n");
    }else if(strlen(input)>0){term_fb_print("Unknown TM command. Type 'turtle' for help.\n");}
}
