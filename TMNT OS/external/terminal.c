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

#define PIT_CHANNEL2 0x42
#define PIT_COMMAND  0x43
#define SPEAKER_PORT 0x61

static int music_playing = 0;

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void speaker_on(uint32_t frequency) {
    if(frequency == 0) return;
    uint32_t divisor = 1193180 / frequency;
    outb(PIT_COMMAND, 0xB6);
    outb(PIT_CHANNEL2, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL2, (uint8_t)((divisor >> 8) & 0xFF));
    uint8_t tmp = inb(SPEAKER_PORT);
    outb(SPEAKER_PORT, tmp | 0x03);
}

static void speaker_off(void) {
    uint8_t tmp = inb(SPEAKER_PORT);
    outb(SPEAKER_PORT, tmp & 0xFC);
}

static uint32_t theme_colors[][3] = {
    {0x0A0A0A, 0xFF4444, 0xFF0000},  // red
    {0x0A0A20, 0x4488FF, 0x0000FF},  // blue
    {0x1A0A1A, 0xCC44FF, 0x8800FF},  // purple
    {0x1A1A00, 0xFFAA00, 0xFF8800},  // orange
    {0x0A0A0A, 0x00FF00, 0x00AA00},  // default green
};

/* Background style enum */
#define BG_STYLE_DEFAULT  0
#define BG_STYLE_BRICKS   1
#define BG_STYLE_SEWER    2
#define BG_STYLE_SHELL    3
#define BG_STYLE_MANHOLE  4
#define BG_STYLE_PIZZA    5
#define BG_STYLE_NINJA    6
#define BG_STYLE_RADIOACTIVE 7
#define BG_STYLE_CITY     8
#define BG_STYLE_LAIR     9
static int bg_style = BG_STYLE_DEFAULT;

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
        for(int col = 0; col < 8; col++) {
            if(line & (0x80 >> col)) {
                fb_putpixel(x + col, y + row, fg);
            } else {
                fb_putpixel(x + col, y + row, bg);
            }
        }
    }
}

/* TMNT-themed background generators */
static void bg_draw_default(void) {
    /* Dark gradient with subtle green tint */
    for(int y = 0; y < (int)fb_height; y++) {
        uint8_t g = 10 + (y * 20) / fb_height;
        uint32_t color = (g / 4 << 16) | (g << 8) | (g / 2);
        for(int x = 0; x < (int)fb_width; x++) {
            fb_putpixel(x, y, color);
        }
    }
}

static void bg_draw_bricks(void) {
    /* Brick wall pattern - brown and red bricks */
    int bw = 64, bh = 32;
    for(int y = 0; y < (int)fb_height; y++) {
        int offset = ((y / bh) % 2) * (bw / 2);
        for(int x = 0; x < (int)fb_width; x++) {
            int bx = (x + offset) % bw;
            int by = y % bh;
            if(bx < 2 || bx >= bw - 2 || by < 2 || by >= bh - 2) {
                fb_putpixel(x, y, 0x1A0A00);  /* mortar */
            } else if(((bx/16) + (by/8)) % 3 == 0) {
                fb_putpixel(x, y, 0x4A1A0A);  /* dark brick */
            } else if(((bx/16) + (by/8)) % 3 == 1) {
                fb_putpixel(x, y, 0x6B2A10);  /* medium brick */
            } else {
                fb_putpixel(x, y, 0x5A2008);  /* light brick */
            }
        }
    }
}

static void bg_draw_sewer(void) {
    /* Dark sewer with water reflection and pipes */
    for(int y = 0; y < (int)fb_height; y++) {
        for(int x = 0; x < (int)fb_width; x++) {
            uint8_t dark = 8 + (y % 40 < 20 ? (y % 40) : 40 - (y % 40)) / 4;
            if(y > (int)fb_height * 3 / 4) {
                /* Water at bottom */
                uint8_t wave = dark + ((x + y) % 30 < 15 ? 5 : 0);
                fb_putpixel(x, y, (wave/4 << 16) | (wave << 8) | (wave/2 + 20));
            } else if(x % 100 < 6 && y < (int)fb_height * 3 / 4) {
                fb_putpixel(x, y, 0x333333);  /* pipe */
            } else {
                fb_putpixel(x, y, (dark/4 << 16) | (dark << 8) | (dark/2));
            }
        }
    }
}

static void bg_draw_shell(void) {
    /* Turtle shell hexagon pattern */
    int hs = 30;
    for(int y = 0; y < (int)fb_height; y++) {
        for(int x = 0; x < (int)fb_width; x++) {
            int hx = x % (hs * 2);
            int hy = y % (hs * 2);
            int cx = hx - hs;
            int cy = hy - hs;
            if(abs(cx) + abs(cy) < hs && (cx * cx + cy * cy) < hs * hs) {
                uint32_t colors[] = {0x1A4A0A, 0x2A5A1A, 0x1A3A00, 0x2A4A10};
                fb_putpixel(x, y, colors[((x/hs) + (y/hs)) % 4]);
            } else {
                fb_putpixel(x, y, 0x0A0A0A);
            }
        }
    }
}

static void bg_draw_manhole(void) {
    /* Dark with manhole cover center pattern */
    for(int y = 0; y < (int)fb_height; y++) {
        for(int x = 0; x < (int)fb_width; x++) {
            int cx = x - (int)fb_width / 2;
            int cy = y - (int)fb_height / 2;
            int dist = (cx * cx + cy * cy);
            int radius = fb_width / 3;
            if(dist < radius * radius) {
                int angle = (dist / 100 + (x+y)) % 6;
                uint32_t colors[] = {0x333333, 0x444444, 0x3A3A3A, 0x505050, 0x383838, 0x484848};
                fb_putpixel(x, y, colors[angle]);
            } else {
                fb_putpixel(x, y, 0x0A0A0A);
            }
        }
    }
}

static void bg_draw_pizza(void) {
    /* Pizza-themed - warm yellows, reds, browns */
    for(int y = 0; y < (int)fb_height; y++) {
        for(int x = 0; x < (int)fb_width; x++) {
            uint32_t color;
            if((x / 80 + y / 40) % 2 == 0) {
                color = 0x3A1A00;  /* dark brown crust */
            } else {
                color = 0x4A2A0A;  /* lighter brown */
            }
            /* Pepperoni dots */
            if((x % 120 < 8 || x % 120 > 112) && (y % 60 < 8 || y % 60 > 52)) {
                color = 0x8B0000;
            }
            fb_putpixel(x, y, color);
        }
    }
}

static void bg_draw_ninja(void) {
    /* Dark ninja theme with red accents */
    for(int y = 0; y < (int)fb_height; y++) {
        for(int x = 0; x < (int)fb_width; x++) {
            if((x / 40 + y / 20) % 3 == 0 && abs(x % 80 - 40) < 3) {
                fb_putpixel(x, y, 0x8B0000);  /* red line */
            } else {
                fb_putpixel(x, y, 0x0A0A0A);  /* dark background */
            }
        }
    }
}

static void bg_draw_radioactive(void) {
    /* Green radioactive ooze theme */
    for(int y = 0; y < (int)fb_height; y++) {
        for(int x = 0; x < (int)fb_width; x++) {
            uint8_t g = 5 + (x * y) % 40;
            fb_putpixel(x, y, (g/6 << 16) | (g << 8) | (g/3));
        }
    }
}

static void bg_draw_city(void) {
    /* Night city skyline */
    for(int y = 0; y < (int)fb_height; y++) {
        for(int x = 0; x < (int)fb_width; x++) {
            if(y > (int)fb_height * 3 / 4) {
                fb_putpixel(x, y, 0x1A1A2A);  /* street */
            } else if(x % 120 < 30 && y > (int)fb_height / 2) {
                fb_putpixel(x, y, 0x0A0A1A);  /* buildings */
            } else {
                uint8_t star = (x * y) % 200 == 0 ? 0xFF : 0x0A;
                fb_putpixel(x, y, (star << 16) | (star << 8) | (star));
            }
        }
    }
}

static void bg_draw_lair(void) {
    /* Turtle lair - warm browns, tech panels */
    for(int y = 0; y < (int)fb_height; y++) {
        for(int x = 0; x < (int)fb_width; x++) {
            if((x / 100) % 2 == 0 && (y / 60) % 2 == 0) {
                fb_putpixel(x, y, 0x2A1A0A);
            } else if(x % 200 < 4 || y % 120 < 4) {
                fb_putpixel(x, y, 0x00AA00);  /* tech lines */
            } else {
                fb_putpixel(x, y, 0x1A0A00);
            }
        }
    }
}

static void term_draw_background(void) {
    switch(bg_style) {
        case BG_STYLE_BRICKS:    bg_draw_bricks(); break;
        case BG_STYLE_SEWER:     bg_draw_sewer(); break;
        case BG_STYLE_SHELL:     bg_draw_shell(); break;
        case BG_STYLE_MANHOLE:   bg_draw_manhole(); break;
        case BG_STYLE_PIZZA:     bg_draw_pizza(); break;
        case BG_STYLE_NINJA:     bg_draw_ninja(); break;
        case BG_STYLE_RADIOACTIVE: bg_draw_radioactive(); break;
        case BG_STYLE_CITY:      bg_draw_city(); break;
        case BG_STYLE_LAIR:      bg_draw_lair(); break;
        default:                 bg_draw_default(); break;
    }
}

void term_fb_clear(void) {
    term_cursor_x = 8;
    term_cursor_y = 8;
    term_draw_background();
}

void term_fb_putchar(char c) {
    if(!fb) return;
    if(c == '\n') {
        term_cursor_x = 8;
        term_cursor_y += 16;
    }
    else if(c == '\r') {
        term_cursor_x = 8;
    }
    else if(c == '\b') {
        if(term_cursor_x > 8) term_cursor_x -= 8;
    }
    else if(c >= ' ') {
        fb_draw_char(term_cursor_x, term_cursor_y, c, term_fg, term_bg);
        term_cursor_x += 8;
    }
    if(term_cursor_x >= (int)fb_width - 8) {
        term_cursor_x = 8;
        term_cursor_y += 16;
    }
    if(term_cursor_y >= (int)fb_height - 20) {
        uint32_t row_bytes = fb_pitch;
        for(uint32_t i = 0; i < (uint32_t)((fb_height - 16) * row_bytes / 4); i++)
            fb[i] = fb[i + (16 * row_bytes / 4)];
        fb_fill_rect(0, fb_height - 20, fb_width, 20, term_bg);
        term_cursor_y = fb_height - 20;
    }
}

void term_fb_print(const char* str) {
    while(*str) term_fb_putchar(*str++);
}

static uint32_t parse_color(const char* str) {
    uint32_t color = 0;
    for(int i = 0; i < 6 && str[i]; i++) {
        color <<= 4;
        if(str[i] >= '0' && str[i] <= '9') color |= (str[i] - '0');
        else if(str[i] >= 'A' && str[i] <= 'F') color |= (str[i] - 'A' + 10);
        else if(str[i] >= 'a' && str[i] <= 'f') color |= (str[i] - 'a' + 10);
    }
    return color;
}

static void color_to_str(uint32_t color, char* out) {
    char hex[] = "0123456789ABCDEF";
    out[0] = hex[(color >> 20) & 0xF];
    out[1] = hex[(color >> 16) & 0xF];
    out[2] = hex[(color >> 12) & 0xF];
    out[3] = hex[(color >> 8) & 0xF];
    out[4] = hex[(color >> 4) & 0xF];
    out[5] = hex[color & 0xF];
    out[6] = '\0';
}

static void set_theme(int theme_idx) {
    if(theme_idx >= 0 && theme_idx < 5) {
        term_bg = theme_colors[theme_idx][0];
        term_fg = theme_colors[theme_idx][1];
        term_fb_clear();
    }
}

static void play_raw_audio(const char* path) {
    if(!fs_file_exists(path)) {
        term_fb_print("❌ Music file not found: ");
        term_fb_print(path);
        term_fb_print("\n");
        return;
    }
    
    uint32_t sz = fs_get_file_size(path);
    if(sz == 0 || sz > 2097152) {
        term_fb_print("❌ Invalid music file size\n");
        return;
    }
    
    uint8_t* audio_data = (uint8_t*)malloc(sz);
    if(!audio_data) {
        term_fb_print("❌ Not enough memory for music\n");
        return;
    }
    
    fs_read_file(path, audio_data, sz);
    
    term_fb_print("🎵 Now playing: ");
    term_fb_print(path);
    term_fb_print("\n   Press any key to stop playback\n");
    
    music_playing = 1;
    
    for(uint32_t i = 0; i < sz && music_playing; i += 1) {
        if((i % 500) == 0) {
            if(inb(0x64) & 0x01) {
                inb(0x60);
                music_playing = 0;
                break;
            }
        }
        
        uint8_t sample = audio_data[i];
        uint32_t freq = 200 + (sample * 1800) / 255;
        
        speaker_on(freq);
        for(volatile uint32_t d = 0; d < 60; d++) asm volatile("nop");
        speaker_off();
        for(volatile uint32_t d = 0; d < 30; d++) asm volatile("nop");
    }
    
    speaker_off();
    free(audio_data);
    
    if(music_playing == 0) {
        term_fb_print("⏹️ Playback stopped\n");
    } else {
        term_fb_print("✅ Playback finished!\n");
    }
    music_playing = 0;
}

void tm_shell_command(char* input) {
    if(input[0]=='t'&&input[1]=='u'&&input[2]=='r'&&input[3]=='t'&&input[4]=='l'&&input[5]=='e'&&input[6]=='\0'){
        term_fg = 0x00FF00; term_fb_print("\n🐢 TM Commands - Customize Your Shell!\n");
        term_fg = 0xFFFF00; term_fb_print("=====================================\n");
        term_fb_print("  shell-shock <hexcolor>  - Change background color (RRGGBB)\n");
        term_fb_print("  sewer-style <file>      - Set wallpaper image file\n");
        term_fb_print("  pizza-power <on/off>    - Toggle boot sound\n");
        term_fb_print("  mask-up <color>         - Change theme (red/blue/purple/orange)\n");
        term_fb_print("  kowabunga <file>        - Run a TM script\n");
        term_fb_print("  shell-save              - Save customizations\n");
        term_fb_print("  shell-load              - Load saved customizations\n");
        term_fb_print("  shell-build             - Build custom ISO with changes\n");
        term_fb_print("  play                    - Play first available music file\n");
        term_fb_print("  play <file>             - Play specific music file\n");
        term_fb_print("  stop                    - Stop music playback\n");
        term_fb_print("  music-list              - List available music\n");
        term_fb_print("  bg-style <0-9>          - Change background style\n");
        term_fb_print("  debug-fs                - Show filesystem debug info\n");
        term_fb_print("  exit                    - Return to desktop\n");
    }
    else if(input[0]=='b'&&input[1]=='g'&&input[2]=='-'&&input[3]=='s'&&input[4]=='t'&&input[5]=='y'&&input[6]=='l'&&input[7]=='e'&&input[8]==' '){
        int style = input[9] - '0';
        if(style >= 0 && style <= 9) {
            bg_style = style;
            term_fb_clear();
            term_fb_print("🎨 Background style changed!\n");
        }
    }
    else if(input[0]=='d'&&input[1]=='e'&&input[2]=='b'&&input[3]=='u'&&input[4]=='g'&&input[5]=='-'&&input[6]=='f'&&input[7]=='s'&&input[8]=='\0'){
        term_fb_print("🔍 Filesystem Debug Info:\n");
        term_fb_print("━━━━━━━━━━━━━━━━━━━━━━━━\n");
        char sz[16];
        term_fb_print("Files in /user/music/: ");
        file_list_t* f = fs_list_directory("/user/music");
        if(f) {
            int_to_str(f->count, sz);
            term_fb_print(sz);
            term_fb_print(" files\n");
            for(int i = 0; i < f->count; i++) {
                term_fb_print("  - ");
                term_fb_print(f->names[i]);
                term_fb_print(" (");
                int_to_str(f->sizes[i], sz);
                term_fb_print(sz);
                term_fb_print(" bytes)\n");
            }
        }
        term_fb_print("\nChecking tmnt_theme.raw: ");
        if(fs_file_exists("/user/music/tmnt_theme.raw")) {
            term_fb_print("EXISTS (");
            int_to_str(fs_get_file_size("/user/music/tmnt_theme.raw"), sz);
            term_fb_print(sz);
            term_fb_print(" bytes)\n");
        } else {
            term_fb_print("NOT FOUND\n");
        }
        term_fb_print("\nAll files in filesystem:\n");
        file_list_t* all = fs_list_directory("/");
        if(all && all->count > 0) {
            int_to_str(all->count, sz);
            term_fb_print(sz);
            term_fb_print(" entries at root\n");
            for(int i = 0; i < all->count; i++) {
                term_fb_print("  /");
                term_fb_print(all->names[i]);
                term_fb_print("\n");
            }
        }
    }
    else if(input[0]=='p'&&input[1]=='l'&&input[2]=='a'&&input[3]=='y'&&input[4]=='\0'){
        fs_create_dir("/user");
        fs_create_dir("/user/music");
        if(fs_file_exists("/user/music/tmnt_theme.raw")) {
            play_raw_audio("/user/music/tmnt_theme.raw");
        } else if(fs_file_exists("/user/music/tmnt-theme.raw")) {
            play_raw_audio("/user/music/tmnt-theme.raw");
        } else {
            file_list_t* files = fs_list_directory("/user/music");
            if(files && files->count > 0) {
                char full_path[256];
                strcpy(full_path, "/user/music/");
                strcat(full_path, files->names[0]);
                play_raw_audio(full_path);
            } else {
                term_fb_print("❌ No music files found in /user/music/\n");
                term_fb_print("   Add .raw audio files to /user/music/\n");
            }
        }
    }
    else if(input[0]=='p'&&input[1]=='l'&&input[2]=='a'&&input[3]=='y'&&input[4]==' '){
        char* path = input + 5;
        play_raw_audio(path);
    }
    else if(input[0]=='s'&&input[1]=='t'&&input[2]=='o'&&input[3]=='p'&&input[4]=='\0'){
        music_playing = 0;
        speaker_off();
        term_fb_print("⏹️ Playback stopped\n");
    }
    else if(input[0]=='m'&&input[1]=='u'&&input[2]=='s'&&input[3]=='i'&&input[4]=='c'&&input[5]=='-'&&input[6]=='l'&&input[7]=='i'&&input[8]=='s'&&input[9]=='t'&&input[10]=='\0'){
        fs_create_dir("/user");
        fs_create_dir("/user/music");
        term_fg = 0xFFFF00; term_fb_print("\n🎵 Available Music Tracks:\n");
        term_fg = 0x00AA00; term_fb_print("━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        if(fs_file_exists("/user/music/tmnt_theme.raw")) {
            uint32_t sz = fs_get_file_size("/user/music/tmnt_theme.raw");
            term_fg = 0xCCCCCC; term_fb_print("  1. 🎵 TMNT Theme (90s Hip-Hop)\n");
            term_fg = 0x888888; term_fb_print("     Path: /user/music/tmnt_theme.raw (");
            char size_str[16]; int_to_str(sz, size_str); term_fb_print(size_str); term_fb_print(" bytes)\n");
        }
        file_list_t* files = fs_list_directory("/user/music");
        if(files && files->count > 0) {
            for(int i = 0; i < files->count; i++) {
                term_fg = 0xCCCCCC; term_fb_print("  🎵 ");
                term_fb_print(files->names[i]);
                term_fg = 0x888888; term_fb_print(" (");
                char size_str[16]; int_to_str(files->sizes[i], size_str); term_fb_print(size_str); term_fb_print(" bytes)\n");
            }
        }
        term_fb_print("\n📀 Usage: 'play' for first track\n");
    }
    else if(input[0]=='s'&&input[1]=='h'&&input[2]=='e'&&input[3]=='l'&&input[4]=='l'&&input[5]=='-'&&input[6]=='s'&&input[7]=='h'&&input[8]=='o'&&input[9]=='c'&&input[10]=='k'&&input[11]==' '){
        char* color_str = input + 12;
        if(color_str[0] && color_str[1] && color_str[2] && color_str[3] && color_str[4] && color_str[5]) {
            uint32_t new_bg = parse_color(color_str);
            term_bg = new_bg;
            term_fg = 0x00FF00;
            term_fb_clear();
            term_fb_print("🐢 Background color changed to #");
            term_fb_print(color_str);
            term_fb_print("\n");
            fs_create_dir("/sys"); fs_create_dir("/sys/cfg");
            fs_write_file("/sys/cfg/shell_color", (uint8_t*)color_str, strlen(color_str));
        } else term_fb_print("❌ Usage: shell-shock <RRGGBB>\n");
    }
    else if(input[0]=='s'&&input[1]=='e'&&input[2]=='w'&&input[3]=='e'&&input[4]=='r'&&input[5]=='-'&&input[6]=='s'&&input[7]=='t'&&input[8]=='y'&&input[9]=='l'&&input[10]=='e'&&input[11]==' '){
        char* path = input + 12;
        if(path[0] != '\0') {
            if(fs_file_exists(path)) {
                term_fb_print("🖼️ Wallpaper set to: ");
                term_fb_print(path); term_fb_print("\n");
                fs_create_dir("/sys"); fs_create_dir("/sys/cfg");
                fs_write_file("/sys/cfg/wallpaper", (uint8_t*)path, strlen(path));
            } else { term_fb_print("❌ Wallpaper file not found\n"); }
        }
    }
    else if(input[0]=='p'&&input[1]=='i'&&input[2]=='z'&&input[3]=='z'&&input[4]=='a'&&input[5]=='-'&&input[6]=='p'&&input[7]=='o'&&input[8]=='w'&&input[9]=='e'&&input[10]=='r'&&input[11]==' '){
        char* state = input + 12;
        if(state[0]=='o'&&state[1]=='n') { term_fb_print("🔊 Boot sound: ON\n"); fs_create_dir("/sys"); fs_create_dir("/sys/cfg"); fs_write_file("/sys/cfg/boot_sound", (uint8_t*)"on", 2); }
        else if(state[0]=='o'&&state[1]=='f'&&state[2]=='f') { term_fb_print("🔇 Boot sound: OFF\n"); fs_create_dir("/sys"); fs_create_dir("/sys/cfg"); fs_write_file("/sys/cfg/boot_sound", (uint8_t*)"off", 3); }
        else term_fb_print("❌ Usage: pizza-power <on/off>\n");
    }
    else if(input[0]=='m'&&input[1]=='a'&&input[2]=='s'&&input[3]=='k'&&input[4]=='-'&&input[5]=='u'&&input[6]=='p'&&input[7]==' '){
        char* theme = input + 8; int theme_idx = -1;
        if(theme[0]=='r'&&theme[1]=='e'&&theme[2]=='d') theme_idx = 0; else if(theme[0]=='b'&&theme[1]=='l'&&theme[2]=='u'&&theme[3]=='e') theme_idx = 1;
        else if(theme[0]=='p'&&theme[1]=='u'&&theme[2]=='r'&&theme[3]=='p'&&theme[4]=='l'&&theme[5]=='e') theme_idx = 2; else if(theme[0]=='o'&&theme[1]=='r'&&theme[2]=='a'&&theme[3]=='n'&&theme[4]=='g'&&theme[5]=='e') theme_idx = 3;
        if(theme_idx >= 0) { set_theme(theme_idx); term_fb_print("🎭 Theme set!\n"); fs_create_dir("/sys"); fs_create_dir("/sys/cfg"); fs_write_file("/sys/cfg/theme", (uint8_t*)theme, strlen(theme)); }
        else term_fb_print("❌ Unknown theme!\n");
    }
    else if(input[0]=='s'&&input[1]=='h'&&input[2]=='e'&&input[3]=='l'&&input[4]=='l'&&input[5]=='-'&&input[6]=='s'&&input[7]=='a'&&input[8]=='v'&&input[9]=='e'&&input[10]=='\0'){
        fs_create_dir("/sys"); fs_create_dir("/sys/cfg"); char color_str[7];
        color_to_str(term_bg, color_str); fs_write_file("/sys/cfg/shell_color", (uint8_t*)color_str, 6);
        color_to_str(term_fg, color_str); fs_write_file("/sys/cfg/shell_fg", (uint8_t*)color_str, 6);
        term_fb_print("✅ Saved!\n");
    }
    else if(input[0]=='s'&&input[1]=='h'&&input[2]=='e'&&input[3]=='l'&&input[4]=='l'&&input[5]=='-'&&input[6]=='l'&&input[7]=='o'&&input[8]=='a'&&input[9]=='d'&&input[10]=='\0'){
        int loaded = 0;
        if(fs_file_exists("/sys/cfg/shell_color")) { uint8_t cb[7]; uint32_t sz = fs_get_file_size("/sys/cfg/shell_color"); if(sz==6){fs_read_file("/sys/cfg/shell_color",cb,6);cb[6]=0;term_bg=parse_color((char*)cb);loaded++;} }
        if(fs_file_exists("/sys/cfg/shell_fg")) { uint8_t cb[7]; uint32_t sz = fs_get_file_size("/sys/cfg/shell_fg"); if(sz==6){fs_read_file("/sys/cfg/shell_fg",cb,6);cb[6]=0;term_fg=parse_color((char*)cb);loaded++;} }
        if(loaded>0){term_fb_clear();term_fb_print("✅ Loaded!\n");} else term_fb_print("📁 No saved config\n");
    }
    else if(input[0]=='s'&&input[1]=='h'&&input[2]=='e'&&input[3]=='l'&&input[4]=='l'&&input[5]=='-'&&input[6]=='b'&&input[7]=='u'&&input[8]=='i'&&input[9]=='l'&&input[10]=='d'&&input[11]=='\0'){
        term_fb_print("🔨 Building custom ISO...\n");
        term_fb_print("Feature coming in next update!\n");
    }
    else if(input[0]=='k'&&input[1]=='o'&&input[2]=='w'&&input[3]=='a'&&input[4]=='b'&&input[5]=='u'&&input[6]=='n'&&input[7]=='g'&&input[8]=='a'&&input[9]==' '){
        char* path = input + 10;
        if(fs_file_exists(path)) { uint32_t sz = fs_get_file_size(path); if(sz>0&&sz<65536){uint8_t* code=(uint8_t*)malloc(sz);fs_read_file(path,code,sz);typedef void(*f)(void);f func=(f)code;func();free(code);term_fb_print("\n✅ Script finished!\n");} }
        else term_fb_print("❌ TM script not found\n");
    }
    else if(input[0]=='e'&&input[1]=='x'&&input[2]=='i'&&input[3]=='t'&&input[4]=='\0'){
        terminal_mode = 0; gui_mode = 1;
        gui_draw_desktop(); gui_save_cursor_bg(); gui_draw_cursor();
        vga_print("\n🐢 Returned to desktop!\n");
    }
    else if(strlen(input) > 0) {
        term_fb_print("Unknown TM command. Type 'turtle' for help.\n");
    }
}