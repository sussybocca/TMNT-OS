// apps/minipack/minipack.c - ENHANCED EDITION
#include "minipack.h"
#include "../../external/gui.h"
#include "../../system/string.h"

extern uint32_t* fb;
extern uint32_t fb_width, fb_height, fb_pitch;
extern const unsigned char font_8x16[96][16];

// ===== GAME STATE =====
#define MAP_W 800
#define MAP_H 500
#define MAX_PACKS 25
#define MAX_CARDS 100
#define INVENTORY_SIZE 100

typedef enum { COMMON, UNCOMMON, RARE, EPIC, LEGENDARY } Rarity;

typedef struct {
    char name[32];
    Rarity rarity;
    int collected;
} Card;

typedef struct {
    int x, y;
    int collected;
    int rare_pack;
    float glow_timer;
} Pack;

typedef struct {
    float x, y;
    float vx, vy;
    int frame;
    int direction;
    int moving;
} Player;

typedef struct {
    float x, y;
    float vx, vy;
    float life;
    uint32_t color;
} Particle;

static Player player = {400, 250, 0, 0, 0, 0, 0};
static Pack packs[MAX_PACKS];
static int pack_count = 0;
static Card cards[MAX_CARDS];
static int cards_collected = 0;
static int show_inventory = 0;
static int show_dialog = 0;
static char dialog_text[256] = "";
static int dialog_timer = 0;
static int score = 0;
static int combo_count = 0;
static int combo_timer = 0;
static Particle particles[50];
static int particle_count = 0;
static char status_msg[128] = "WASD to move | Space near packs to open | I for inventory";
static int frame_counter = 0;

// ===== CARD DATA =====
static const char* card_names[] = {
    "Green Shell", "Blue Mask", "Red Nunchaku", "Purple Katana", "Orange Sai",
    "Turtle Power", "Shredder Helm", "Krang Body", "Bebop Shades", "Rocksteady Horn",
    "Splinter Staff", "April Microphone", "Casey Bat", "Foot Clan Emblem", "TCRI Canister",
    "Mutagen Ooze", "Pizza Slice", "Sewer Map", "Manhole Cover", "Shell Raiser",
    "Donnie Bo", "Leo Blade", "Mikey Chain", "Raph Sai", "Metalhead Core",
    "Fugitoid Chip", "Triceraton Horn", "Neutrino Star", "Dimension X Key", "Technodrome Core",
    "Turtle Comm", "Shell Cell", "Ooze Canister", "Retro mutagen", "Dark Turtle Stone",
    "Silver Sentry", "Gold Shogun", "Platinum Dragon", "Diamond Turtle", "Ruby Shell",
    "Sapphire Wave", "Emerald Strike", "Amethyst Guard", "Obsidian Blade", "Crystal Star",
    "Shadow Stealth", "Light Bringer", "Storm Caller", "Fire Breather", "Ice Shard",
    "Thunder Clap", "Earth Shaker", "Wind Walker", "Water Lord", "Spirit Guide",
    "Ancient One", "Time Twister", "Reality Warp", "Chaos Bringer", "Order Keeper",
    "Cosmic Turtle", "Galactic Shell", "Universal Key", "Infinity Stone", "Eternal Flame",
    "Void Walker", "Dream Weaver", "Night Stalker", "Dawn Breaker", "Dusk Hunter",
    "Star Forger", "Moon Shard", "Sun Core", "Nebula Dust", "Comet Tail",
    "Astro Shell", "Quantum Mask", "Photon Blade", "Plasma Sai", "Laser Nunchaku",
    "Holo Shuriken", "Cyber Shell", "Digital Dojo", "Pixel Sensei", "Binary Blade",
    "Hex Shield", "ROM Armor", "RAM Katana", "CPU Core", "GPU Shell",
    "Neural Link", "Data Stream", "Fire Wall", "Code Breaker", "System Crash",
    "Root Access", "Kernel Panic", "Stack Overflow", "Null Pointer", "Blue Screen",
    "Boot Sector", "Master Boot", "Partition Key", "Format Drive", "Defrag Shell"
};

static Rarity card_rarities[] = {
    COMMON, UNCOMMON, COMMON, RARE, UNCOMMON,
    RARE, EPIC, RARE, UNCOMMON, COMMON,
    RARE, UNCOMMON, COMMON, COMMON, UNCOMMON,
    RARE, UNCOMMON, COMMON, COMMON, EPIC,
    RARE, EPIC, RARE, RARE, LEGENDARY,
    RARE, EPIC, LEGENDARY, RARE, EPIC,
    EPIC, LEGENDARY, RARE, EPIC, LEGENDARY,
    LEGENDARY, EPIC, LEGENDARY, EPIC, RARE,
    LEGENDARY, EPIC, RARE, EPIC, LEGENDARY,
    LEGENDARY, EPIC, LEGENDARY, EPIC, LEGENDARY,
    RARE, LEGENDARY, EPIC, RARE, LEGENDARY,
    EPIC, LEGENDARY, EPIC, LEGENDARY, EPIC,
    LEGENDARY, EPIC, LEGENDARY, EPIC, LEGENDARY,
    LEGENDARY, EPIC, LEGENDARY, EPIC, RARE,
    LEGENDARY, EPIC, RARE, LEGENDARY, EPIC,
    LEGENDARY, EPIC, LEGENDARY, EPIC, LEGENDARY,
    RARE, EPIC, LEGENDARY, RARE, EPIC,
    LEGENDARY, EPIC, LEGENDARY, EPIC, RARE,
    LEGENDARY, EPIC, RARE, LEGENDARY, EPIC,
    LEGENDARY, EPIC, LEGENDARY, EPIC, LEGENDARY
};

// ===== COLORS =====
static uint32_t rarity_colors[] = {
    0xAAAAAA, 0x00DD00, 0x0088FF, 0xCC44FF, 0xFFAA00
};

static const char* rarity_names[] = {
    "Common", "Uncommon", "Rare", "Epic", "Legendary"
};

// ===== DRAWING HELPERS =====
static void mp_putpixel(int x, int y, uint32_t color) {
    if(x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height)
        fb[y * (fb_pitch / 4) + x] = color;
}

static void mp_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for(int dy = 0; dy < h; dy++)
        for(int dx = 0; dx < w; dx++)
            mp_putpixel(x + dx, y + dy, color);
}

static void mp_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for(int i = 0; i < w; i++) { mp_putpixel(x+i, y, color); mp_putpixel(x+i, y+h-1, color); }
    for(int i = 0; i < h; i++) { mp_putpixel(x, y+i, color); mp_putpixel(x+w-1, y+i, color); }
}

static void mp_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    if(c < 32 || c > 126) return;
    const unsigned char* glyph = font_8x16[c - 32];
    for(int row = 0; row < 16; row++) {
        unsigned char line = glyph[row];
        for(int col = 0; col < 8; col++)
            mp_putpixel(x + col, y + row, (line & (0x80 >> col)) ? fg : bg);
    }
}

static void mp_draw_text(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    int cx = x;
    while(*str) {
        if(*str == '\n') { cx = x; y += 16; }
        else { mp_draw_char(cx, y, *str, fg, bg); cx += 8; }
        str++;
    }
}

// ===== PARTICLE SYSTEM =====
static void spawn_particles(int x, int y, uint32_t color, int count) {
    for(int i = 0; i < count && particle_count < 50; i++) {
        particles[particle_count].x = (float)x;
        particles[particle_count].y = (float)y;
        particles[particle_count].vx = (float)((i % 7) - 3) * 0.5f;
        particles[particle_count].vy = (float)((i / 7) - 2) * 0.5f;
        particles[particle_count].life = 30.0f + (float)(i * 3);
        particles[particle_count].color = color;
        particle_count++;
    }
}

static void update_particles(void) {
    for(int i = 0; i < particle_count; i++) {
        particles[i].x += particles[i].vx;
        particles[i].y += particles[i].vy;
        particles[i].life -= 1.0f;
        if(particles[i].life <= 0) {
            particles[i] = particles[--particle_count];
            i--;
        }
    }
}

static void draw_particles(int wx, int wy) {
    for(int i = 0; i < particle_count; i++) {
        int px = wx + (int)particles[i].x;
        int py = wy + (int)particles[i].y;
        int alpha = (int)(particles[i].life * 8.0f);
        if(alpha > 255) alpha = 255;
        if(alpha < 0) alpha = 0;
        uint32_t color = particles[i].color;
        int r = ((color >> 16) & 0xFF) * alpha / 255;
        int g = ((color >> 8) & 0xFF) * alpha / 255;
        int b = (color & 0xFF) * alpha / 255;
        uint32_t faded = (uint32_t)((r << 16) | (g << 8) | b);
        mp_putpixel(px, py, faded);
        mp_putpixel(px+1, py, faded);
        mp_putpixel(px, py+1, faded);
    }
}

// ===== GAME FUNCTIONS =====
static void spawn_packs(void) {
    pack_count = 20;
    for(int i = 0; i < pack_count; i++) {
        packs[i].x = 60 + (i * 53 + i * i * 7) % 720;
        packs[i].y = 40 + (i * 37 + i * 13) % 440;
        packs[i].collected = 0;
        packs[i].rare_pack = (i % 5 == 0) ? 1 : 0;
        packs[i].glow_timer = (float)(i * 1.7f);
    }
}

static const char* get_rarity_name(Rarity r) {
    return rarity_names[r];
}

static uint32_t get_rarity_color(Rarity r) {
    return rarity_colors[r];
}

static void open_pack(void) {
    if(cards_collected >= 100) {
        strcpy(dialog_text, "ALL 100 CARDS COLLECTED! You're the ultimate Turtle Master!");
        show_dialog = 1;
        dialog_timer = 180;
        spawn_particles((int)player.x + 8, (int)player.y + 8, 0xFFAA00, 50);
        return;
    }
    int card_index = ((int)player.x * 73 + (int)player.y * 31 + score * 17) % 100;
    int tries = 0;
    while(cards[card_index].collected && tries < 100) {
        card_index = (card_index + 1) % 100;
        tries++;
    }
    if(tries >= 100) {
        strcpy(dialog_text, "You already have all available cards from this area!");
        show_dialog = 1;
        dialog_timer = 90;
        return;
    }
    cards[card_index].collected = 1;
    cards_collected++;
    score += 10;
    combo_count++;
    combo_timer = 60;
    Rarity r = card_rarities[card_index];
    if(r >= RARE) score += 10;
    if(r >= EPIC) score += 50;
    if(r == LEGENDARY) { score += 200; combo_count += 5; }
    uint32_t particle_color = get_rarity_color(r);
    spawn_particles((int)player.x + 8, (int)player.y + 8, particle_color, 20 + r * 5);
    char msg[256];
    int p = 0;
    const char* rn = get_rarity_name(r);
    while(*rn) msg[p++] = *rn++;
    msg[p++] = ' '; msg[p++] = 'C'; msg[p++] = 'a'; msg[p++] = 'r'; msg[p++] = 'd'; msg[p++] = '!';
    msg[p++] = ' '; msg[p++] = ' ';
    const char* cn = card_names[card_index];
    while(*cn) msg[p++] = *cn++;
    if(combo_count > 1) {
        msg[p++] = ' '; msg[p++] = 'x';
        if(combo_count >= 10) { msg[p++] = '0' + combo_count/10; msg[p++] = '0' + combo_count%10; }
        else msg[p++] = '0' + combo_count;
        msg[p++] = ' '; msg[p++] = 'C'; msg[p++] = 'O'; msg[p++] = 'M'; msg[p++] = 'B'; msg[p++] = 'O'; msg[p++] = '!';
    }
    msg[p] = '\0';
    strcpy(dialog_text, msg);
    show_dialog = 1;
    dialog_timer = 120;
}

static void draw_player(int wx, int wy) {
    int px = wx + (int)player.x;
    int py = wy + (int)player.y;
    mp_fill_rect(px + 2, py + 14, 12, 4, 0x002200);
    int bounce = (player.moving) ? ((frame_counter / 8) % 2) * 2 : 0;
    mp_fill_rect(px, py - bounce, 16, 16, 0x00AA00);
    mp_draw_rect(px, py - bounce, 16, 16, 0x00FF00);
    mp_fill_rect(px + 4, py + 5 - bounce, 8, 8, 0x006600);
    mp_draw_rect(px + 4, py + 5 - bounce, 8, 8, 0x008800);
    mp_putpixel(px + 8, py + 6 - bounce, 0x004400);
    mp_putpixel(px + 8, py + 10 - bounce, 0x004400);
    mp_putpixel(px + 6, py + 8 - bounce, 0x004400);
    mp_putpixel(px + 10, py + 8 - bounce, 0x004400);
    mp_fill_rect(px + 3, py + 3 - bounce, 4, 4, 0xFFFFFF);
    mp_fill_rect(px + 9, py + 3 - bounce, 4, 4, 0xFFFFFF);
    mp_putpixel(px + 5, py + 4 - bounce, 0x000000);
    mp_putpixel(px + 11, py + 4 - bounce, 0x000000);
    mp_putpixel(px + 7, py + 12 - bounce, 0x000000);
    mp_putpixel(px + 8, py + 12 - bounce, 0x000000);
    mp_fill_rect(px + 1, py + 8 - bounce, 14, 3, 0xFF4400);
    if(player.direction == 1) mp_fill_rect(px + 12, py + 11 - bounce, 4, 6, 0xFF4400);
    else if(player.direction == 0) mp_fill_rect(px, py + 11 - bounce, 4, 6, 0xFF4400);
}

static void draw_packs(int wx, int wy) {
    for(int i = 0; i < pack_count; i++) {
        if(!packs[i].collected) {
            int px = wx + packs[i].x;
            int py = wy + packs[i].y;
            packs[i].glow_timer += 0.1f;
            if(packs[i].rare_pack) {
                mp_fill_rect(px - 1, py - 1, 16, 16, 0x553300);
                mp_fill_rect(px, py, 14, 14, 0x664400);
                mp_draw_rect(px, py, 14, 14, 0xFFAA00);
                mp_putpixel(px + 7, py + 2, 0xFFDD00);
                mp_putpixel(px + 5, py + 4, 0xFFDD00);
                mp_putpixel(px + 9, py + 4, 0xFFDD00);
                mp_putpixel(px + 3, py + 7, 0xFFDD00);
                mp_putpixel(px + 11, py + 7, 0xFFDD00);
                mp_putpixel(px + 5, py + 10, 0xFFDD00);
                mp_putpixel(px + 9, py + 10, 0xFFDD00);
                mp_putpixel(px + 7, py + 12, 0xFFDD00);
            } else {
                mp_fill_rect(px, py, 14, 14, 0x333300);
                mp_draw_rect(px, py, 14, 14, 0x888800);
                mp_putpixel(px + 5, py + 3, 0xCCCC00);
                mp_putpixel(px + 8, py + 3, 0xCCCC00);
                mp_putpixel(px + 7, py + 4, 0xCCCC00);
                mp_putpixel(px + 6, py + 5, 0xCCCC00);
                mp_putpixel(px + 7, py + 7, 0xCCCC00);
                mp_putpixel(px + 7, py + 10, 0xCCCC00);
            }
        }
    }
}

static void draw_minimap(int wx, int wy) {
    int mx = wx + 650;
    int my = wy + 5;
    mp_fill_rect(mx, my, 150, 100, 0x0A0A0A);
    mp_draw_rect(mx, my, 150, 100, 0x004400);
    mp_draw_text(mx + 35, my + 2, "MINIMAP", 0x00AA00, 0x0A0A0A);
    int dot_x = mx + (int)(player.x * 150 / MAP_W);
    int dot_y = my + 10 + (int)(player.y * 85 / MAP_H);
    mp_fill_rect(dot_x - 1, dot_y - 1, 3, 3, 0x00FF00);
    for(int i = 0; i < pack_count; i++) {
        if(!packs[i].collected) {
            int px = mx + (packs[i].x * 150 / MAP_W);
            int py = my + 10 + (packs[i].y * 85 / MAP_H);
            mp_putpixel(px, py, packs[i].rare_pack ? 0xFFAA00 : 0x888800);
        }
    }
}

static void draw_dialog(int wx, int wy) {
    if(show_dialog && dialog_timer > 0) {
        int dx = wx + 50;
        int dy = wy + 280;
        mp_fill_rect(dx + 2, dy + 2, 500, 70, 0x000000);
        for(int i = 0; i < 70; i++) {
            uint8_t g = 20 + i/3;
            mp_fill_rect(dx, dy + i, 500, 1, (g/4 << 16) | (g << 8) | (g/2));
        }
        mp_draw_rect(dx, dy, 500, 70, 0xFFAA00);
        mp_draw_rect(dx + 1, dy + 1, 498, 68, 0x664400);
        mp_draw_text(dx + 15, dy + 8, dialog_text, 0xFFDD00, 0x0A1A0A);
        mp_draw_text(dx + 15, dy + 28, "Press any key to continue...", 0x88AA88, 0x0A1A0A);
        if(combo_timer > 0) {
            char combo[32];
            combo[0] = 'C'; combo[1] = 'O'; combo[2] = 'M'; combo[3] = 'B'; combo[4] = 'O';
            combo[5] = ':'; combo[6] = ' '; combo[7] = 'x';
            int cn = combo_count;
            int ci = 8;
            if(cn >= 10) { combo[ci++] = '0' + cn/10; combo[ci++] = '0' + cn%10; }
            else combo[ci++] = '0' + cn;
            combo[ci] = '\0';
            mp_draw_text(dx + 350, dy + 50, combo, 0xFF4444, 0x0A1A0A);
        }
    }
}

static void draw_progress_bar(int wx, int wy, int x, int y, int w, int h, int current, int max, uint32_t fg, uint32_t bg) {
    mp_fill_rect(wx + x, wy + y, w, h, bg);
    mp_draw_rect(wx + x, wy + y, w, h, fg);
    int fill = (current * w) / max;
    if(fill > 0) mp_fill_rect(wx + x + 1, wy + y + 1, fill - 1, h - 2, fg);
}

static void draw_inventory(int wx, int wy) {
    if(!show_inventory) return;
    mp_fill_rect(wx + 80, wy + 30, 480, 380, 0x0A0A2A);
    mp_draw_rect(wx + 80, wy + 30, 480, 380, 0x00AAFF);
    mp_draw_rect(wx + 82, wy + 32, 476, 376, 0x004488);
    mp_draw_text(wx + 200, wy + 38, "CARD COLLECTION", 0x00AAFF, 0x0A0A2A);
    char info[32];
    int p = 0;
    int n = cards_collected;
    if(n >= 100) { info[p++] = '1'; info[p++] = '0'; info[p++] = '0'; }
    else if(n >= 10) { info[p++] = '0' + n/10; info[p++] = '0' + n%10; }
    else { info[p++] = '0' + n; }
    info[p++] = '/'; info[p++] = '1'; info[p++] = '0'; info[p++] = '0';
    info[p] = '\0';
    mp_draw_text(wx + 300, wy + 58, info, 0xFFFFFF, 0x0A0A2A);
    draw_progress_bar(wx, wy, 100, 75, 440, 14, cards_collected, 100, 0x00FF00, 0x002200);
    char score_str[32];
    score_str[0] = 'S'; score_str[1] = 'c'; score_str[2] = 'o'; score_str[3] = 'r'; score_str[4] = 'e'; score_str[5] = ':'; score_str[6] = ' ';
    int sn = score;
    int si = 7;
    char temp[16]; int ti = 0;
    if(sn == 0) temp[ti++] = '0';
    while(sn > 0) { temp[ti++] = '0' + sn%10; sn /= 10; }
    while(ti > 0) score_str[si++] = temp[--ti];
    score_str[si] = '\0';
    mp_draw_text(wx + 100, wy + 95, score_str, 0xFFFF00, 0x0A0A2A);
    int counts[5] = {0};
    for(int i = 0; i < 100; i++) {
        if(cards[i].collected) counts[card_rarities[i]]++;
    }
    mp_draw_text(wx + 300, wy + 95, "Rarity Breakdown:", 0xFFFFFF, 0x0A0A2A);
    for(int r = 0; r < 5; r++) {
        char line[32];
        int lp = 0;
        const char* rn = rarity_names[r];
        while(*rn) line[lp++] = *rn++;
        line[lp++] = ':'; line[lp++] = ' ';
        if(counts[r] >= 10) { line[lp++] = '0' + counts[r]/10; line[lp++] = '0' + counts[r]%10; }
        else line[lp++] = '0' + counts[r];
        line[lp] = '\0';
        mp_draw_text(wx + 300, wy + 115 + r * 18, line, get_rarity_color((Rarity)r), 0x0A0A2A);
    }
    mp_draw_text(wx + 100, wy + 210, "Collected Cards:", 0xFFFFFF, 0x0A0A2A);
    int start_y = 230;
    int shown = 0;
    for(int i = 0; i < 100 && shown < 10; i++) {
        if(cards[i].collected) {
            uint32_t color = get_rarity_color(card_rarities[i]);
            mp_draw_text(wx + 105, wy + start_y + shown * 16, card_names[i], color, 0x0A0A2A);
            shown++;
        }
    }
    if(shown == 0) mp_draw_text(wx + 105, wy + start_y, "No cards yet! Find packs!", 0x888888, 0x0A0A2A);
    mp_draw_text(wx + 200, wy + 395, "Press I to close inventory", 0x888888, 0x0A0A2A);
}

// ===== MAIN DRAW FUNCTION =====
static void minipack_draw(int wx, int wy, int ww, int wh) {
    mp_fill_rect(wx + 8, wy + 28, ww - 16, wh - 53, 0x0A2A0A);
    mp_draw_rect(wx + 8, wy + 28, ww - 16, wh - 53, 0x00AA00);
    mp_draw_rect(wx + 10, wy + 30, ww - 20, wh - 57, 0x004400);
    for(int i = 0; i < MAP_W; i += 40)
        for(int j = 0; j < MAP_H; j += 40)
            mp_putpixel(wx + 12 + i, wy + 32 + j, 0x0A3A0A);
    int cx = wx + 12, cy = wy + 32;
    mp_fill_rect(cx, cy, 8, 8, 0x00AA00);
    mp_fill_rect(cx + MAP_W - 8, cy, 8, 8, 0x00AA00);
    mp_fill_rect(cx, cy + MAP_H - 8, 8, 8, 0x00AA00);
    mp_fill_rect(cx + MAP_W - 8, cy + MAP_H - 8, 8, 8, 0x00AA00);
    update_particles();
    draw_packs(wx + 12, wy + 32);
    draw_particles(wx + 12, wy + 32);
    draw_player(wx + 12, wy + 32);
    draw_minimap(wx, wy);
    draw_dialog(wx, wy);
    draw_inventory(wx, wy);
    mp_fill_rect(wx + 10, wy + wh - 25, ww - 20, 20, 0x0A1A0A);
    mp_draw_rect(wx + 10, wy + wh - 25, ww - 20, 20, 0x004400);
    char hud[64];
    int p = 0;
    hud[p++] = 'C'; hud[p++] = 'a'; hud[p++] = 'r'; hud[p++] = 'd'; hud[p++] = 's'; hud[p++] = ':'; hud[p++] = ' ';
    int n = cards_collected;
    if(n >= 100) { hud[p++] = '1'; hud[p++] = '0'; hud[p++] = '0'; }
    else if(n >= 10) { hud[p++] = '0' + n/10; hud[p++] = '0' + n%10; }
    else { hud[p++] = '0' + n; }
    hud[p++] = '/'; hud[p++] = '1'; hud[p++] = '0'; hud[p++] = '0';
    hud[p] = '\0';
    mp_draw_text(wx + 20, wy + wh - 22, hud, 0x00FF00, 0x0A1A0A);
    char scr[32];
    scr[0] = 'S'; scr[1] = ':'; scr[2] = ' ';
    int sn = score, si = 3;
    char st2[16]; int sti2 = 0;
    if(sn == 0) st2[sti2++] = '0';
    while(sn > 0) { st2[sti2++] = '0' + sn%10; sn /= 10; }
    while(sti2 > 0) scr[si++] = st2[--sti2];
    scr[si] = '\0';
    mp_draw_text(wx + 160, wy + wh - 22, scr, 0xFFFF00, 0x0A1A0A);
    if(combo_timer > 0) {
        char cmb[16];
        cmb[0] = 'x'; cmb[1] = ' ';
        int cn2 = combo_count, ci2 = 2;
        if(cn2 >= 10) { cmb[ci2++] = '0' + cn2/10; cmb[ci2++] = '0' + cn2%10; }
        else cmb[ci2++] = '0' + cn2;
        cmb[ci2] = '\0';
        mp_draw_text(wx + 250, wy + wh - 22, cmb, 0xFF4444, 0x0A1A0A);
    }
    mp_draw_text(wx + 320, wy + wh - 22, status_msg, 0x888888, 0x0A1A0A);
}

// ===== CLICK HANDLER =====
static void minipack_click(int mx, int my) {
    int gx = mx - 12;
    int gy = my - 32;
    for(int i = 0; i < pack_count; i++) {
        if(!packs[i].collected && gx >= packs[i].x - 4 && gx < packs[i].x + 18 && gy >= packs[i].y - 4 && gy < packs[i].y + 18) {
            packs[i].collected = 1;
            open_pack();
            return;
        }
    }
}

// ===== KEYBOARD HANDLER =====
static void minipack_key(char key) {
    if(show_dialog && dialog_timer > 0) { show_dialog = 0; dialog_timer = 0; return; }
    float speed = 3.0f;
    player.moving = 0;
    switch(key) {
        case 'w': case 'W': player.vy = -speed; player.direction = 1; player.moving = 1; break;
        case 's': case 'S': player.vy = speed; player.direction = 0; player.moving = 1; break;
        case 'a': case 'A': player.vx = -speed; player.direction = 2; player.moving = 1; break;
        case 'd': case 'D': player.vx = speed; player.direction = 3; player.moving = 1; break;
        case ' ':
            for(int i = 0; i < pack_count; i++) {
                if(!packs[i].collected) {
                    float dx = player.x - packs[i].x; if(dx < 0) dx = -dx;
                    float dy = player.y - packs[i].y; if(dy < 0) dy = -dy;
                    if(dx < 25 && dy < 25) { packs[i].collected = 1; open_pack(); break; }
                }
            }
            break;
        case 'i': case 'I': show_inventory = !show_inventory; break;
    }
    player.x += player.vx; player.y += player.vy;
    if(player.x < 0) player.x = 0;
    if(player.y < 0) player.y = 0;
    if(player.x > MAP_W - 16) player.x = MAP_W - 16;
    if(player.y > MAP_H - 16) player.y = MAP_H - 16;
    player.vx *= 0.3f; player.vy *= 0.3f;
    player.frame = (player.frame + 1) % 4;
    frame_counter++;
    if(dialog_timer > 0) { dialog_timer--; if(dialog_timer == 0) show_dialog = 0; }
    if(combo_timer > 0) { combo_timer--; if(combo_timer == 0) combo_count = 0; }
}

// ===== OPEN FUNCTION =====
void minipack_open(void) {
    for(int i = 0; i < 100; i++) {
        int j = 0;
        const char* name = card_names[i];
        while(name[j] && j < 31) { cards[i].name[j] = name[j]; j++; }
        cards[i].name[j] = '\0';
        cards[i].rarity = card_rarities[i];
        cards[i].collected = 0;
    }
    spawn_packs();
    player.x = 400; player.y = 250;
    player.vx = 0; player.vy = 0;
    player.frame = 0; player.direction = 0; player.moving = 0;
    score = 0; cards_collected = 0;
    show_inventory = 0; show_dialog = 0; dialog_timer = 0;
    combo_count = 0; combo_timer = 0;
    particle_count = 0; frame_counter = 0;
    gui_run_auto_app("MiniPack - Card Collector!", 30, 20, MAP_W + 35, MAP_H + 55, minipack_draw, minipack_click, minipack_key);
}