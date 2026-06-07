#include "runner.h"
#include "../../system/string.h"
#include "../../system/memory.h"

// External framebuffer and functions
extern uint32_t* fb;
extern uint32_t fb_width, fb_height, fb_pitch;
extern char keyboard_getchar_poll(void);
extern const unsigned char font_8x16[96][16];

// Game state
static int player_x, player_y;
static int enemy_x, enemy_y;
static int player_size = 20;
static int enemy_size = 24;
static int score = 0;
static int game_running = 0;
static int player_speed = 5;
static int enemy_speed = 2;

// Local I/O functions
static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    asm volatile("inb %1,%0":"=a"(r):"Nd"(port));
    return r;
}

// Simple drawing functions (avoid dependency on gui.c)
static void game_putpixel(int x, int y, uint32_t color) {
    if(x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height)
        fb[y * (fb_pitch / 4) + x] = color;
}

static void game_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for(int dy = 0; dy < h; dy++)
        for(int dx = 0; dx < w; dx++)
            game_putpixel(x + dx, y + dy, color);
}

static void game_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for(int i = 0; i < w; i++) {
        game_putpixel(x + i, y, color);
        game_putpixel(x + i, y + h - 1, color);
    }
    for(int i = 0; i < h; i++) {
        game_putpixel(x, y + i, color);
        game_putpixel(x + w - 1, y + i, color);
    }
}

static void game_draw_char(int x, int y, char c, uint32_t color) {
    if(c < 32 || c > 126) return;
    const unsigned char* glyph = font_8x16[c - 32];
    for(int row = 0; row < 16; row++) {
        unsigned char line = glyph[row];
        for(int col = 0; col < 8; col++) {
            if(line & (0x80 >> col))
                game_putpixel(x + col, y + row, color);
        }
    }
}

static void game_draw_text(int x, int y, const char* str, uint32_t color) {
    int cx = x;
    while(*str) {
        if(*str == '\n') { cx = x; y += 16; }
        else { game_draw_char(cx, y, *str, color); cx += 8; }
        str++;
    }
}

static void game_draw_player(void) {
    // Green square (turtle) with shell pattern
    game_fill_rect(player_x, player_y, player_size, player_size, 0x00FF00);
    game_draw_rect(player_x, player_y, player_size, player_size, 0x00AA00);
    // Shell pattern on body
    game_fill_rect(player_x + 4, player_y + 8, 5, 5, 0x006600);
    game_fill_rect(player_x + 11, player_y + 8, 5, 5, 0x006600);
    // Eyes
    game_putpixel(player_x + 5, player_y + 4, 0x000000);
    game_putpixel(player_x + 14, player_y + 4, 0x000000);
    // Mouth
    game_putpixel(player_x + 8, player_y + 16, 0x000000);
    game_putpixel(player_x + 11, player_y + 16, 0x000000);
}

static void game_draw_enemy(void) {
    // Red square (shredder) with armor pattern
    game_fill_rect(enemy_x, enemy_y, enemy_size, enemy_size, 0xFF0000);
    game_draw_rect(enemy_x, enemy_y, enemy_size, enemy_size, 0xCC0000);
    // Armor spikes
    game_putpixel(enemy_x, enemy_y + 6, 0x888888);
    game_putpixel(enemy_x, enemy_y + 12, 0x888888);
    game_putpixel(enemy_x, enemy_y + 18, 0x888888);
    game_putpixel(enemy_x + enemy_size - 1, enemy_y + 6, 0x888888);
    game_putpixel(enemy_x + enemy_size - 1, enemy_y + 12, 0x888888);
    game_putpixel(enemy_x + enemy_size - 1, enemy_y + 18, 0x888888);
    // Angry eyes
    game_fill_rect(enemy_x + 5, enemy_y + 7, 5, 3, 0xFFFFFF);
    game_fill_rect(enemy_x + 14, enemy_y + 7, 5, 3, 0xFFFFFF);
    game_putpixel(enemy_x + 7, enemy_y + 8, 0x000000);
    game_putpixel(enemy_x + 16, enemy_y + 8, 0x000000);
    // Angry mouth
    game_fill_rect(enemy_x + 8, enemy_y + 16, 8, 3, 0x000000);
}

static void game_move_enemy(void) {
    // Simple AI: move toward player
    if(enemy_x < player_x) enemy_x += enemy_speed;
    if(enemy_x > player_x) enemy_x -= enemy_speed;
    if(enemy_y < player_y) enemy_y += enemy_speed;
    if(enemy_y > player_y) enemy_y -= enemy_speed;
    
    // Keep enemy on screen
    if(enemy_x < 0) enemy_x = 0;
    if(enemy_x > (int)fb_width - enemy_size) enemy_x = fb_width - enemy_size;
    if(enemy_y < 0) enemy_y = 0;
    if(enemy_y > (int)fb_height - enemy_size) enemy_y = fb_height - enemy_size;
}

static int game_check_collision(void) {
    // Simple AABB collision with slight padding for fairness
    int pad = 4;
    if(player_x + pad < enemy_x + enemy_size - pad &&
       player_x + player_size - pad > enemy_x + pad &&
       player_y + pad < enemy_y + enemy_size - pad &&
       player_y + player_size - pad > enemy_y + pad) {
        return 1;
    }
    return 0;
}

static void game_draw_hud(void) {
    char score_str[32] = "Score: ";
    char num[16];
    int n = score;
    int i = 0;
    if(n == 0) num[i++] = '0';
    while(n > 0) { num[i++] = '0' + (n % 10); n /= 10; }
    num[i] = '\0';
    // Reverse
    for(int j = 0; j < i/2; j++) { char t = num[j]; num[j] = num[i-1-j]; num[i-1-j] = t; }
    
    int len = 7;
    for(int j = 0; num[j]; j++) {
        score_str[len++] = num[j];
    }
    score_str[len] = '\0';
    
    // Draw HUD background
    game_fill_rect(0, 0, fb_width, 20, 0x000000);
    game_draw_text(10, 2, score_str, 0x00FF00);
    
    char speed_str[32] = "Speed: ";
    int speed_num = enemy_speed;
    char speed_num_str[8];
    int si = 0;
    if(speed_num == 0) speed_num_str[si++] = '0';
    while(speed_num > 0) { speed_num_str[si++] = '0' + (speed_num % 10); speed_num /= 10; }
    speed_num_str[si] = '\0';
    for(int j = 0; j < si/2; j++) { char t = speed_num_str[j]; speed_num_str[j] = speed_num_str[si-1-j]; speed_num_str[si-1-j] = t; }
    
    int slen = 7;
    for(int j = 0; speed_num_str[j]; j++) {
        speed_str[slen++] = speed_num_str[j];
    }
    speed_str[slen] = '\0';
    
    game_draw_text(fb_width - 250, 2, speed_str, 0xFF4444);
    game_draw_text(fb_width - 160, 2, "WASD move  Q=quit", 0x888888);
}

void runner_game_open(void) {
    // Draw title screen
    game_fill_rect(0, 0, fb_width, fb_height, 0x000000);
    game_draw_text(fb_width/2 - 100, fb_height/2 - 60, "🐢 TURTLE RUNNER 🐢", 0x00FF00);
    game_draw_text(fb_width/2 - 120, fb_height/2 - 20, "Escape the Shredder!", 0xFF0000);
    game_draw_text(fb_width/2 - 100, fb_height/2 + 10, "WASD to move, Q to quit", 0xFFFFFF);
    game_draw_text(fb_width/2 - 80, fb_height/2 + 40, "Press any key to start", 0xFFFF00);
    
    // Wait for keypress to start
    while(1) {
        if(inb(0x64) & 0x01) {
            inb(0x60);
            break;
        }
    }
    
    // Initialize game
    player_x = fb_width / 2 - player_size / 2;
    player_y = fb_height / 2 - player_size / 2;
    enemy_x = 50;
    enemy_y = 50;
    score = 0;
    enemy_speed = 2;
    game_running = 1;
    
    // Game loop
    while(game_running) {
        // Clear screen
        game_fill_rect(0, 20, fb_width, fb_height - 20, 0x0A0A2A);
        
        // Draw game border
        game_draw_rect(0, 20, fb_width, fb_height - 20, 0x333333);
        
        // Draw game objects
        game_draw_player();
        game_draw_enemy();
        game_draw_hud();
        
        // Move enemy
        game_move_enemy();
        
        // Check collision
        if(game_check_collision()) {
            game_running = 0;
            game_fill_rect(0, 0, fb_width, fb_height, 0x000000);
            game_draw_text(fb_width/2 - 60, fb_height/2 - 30, "GAME OVER!", 0xFF0000);
            
            char final_score[32] = "Final Score: ";
            char num[16];
            int n = score;
            int i = 0;
            if(n == 0) num[i++] = '0';
            while(n > 0) { num[i++] = '0' + (n % 10); n /= 10; }
            num[i] = '\0';
            for(int j = 0; j < i/2; j++) { char t = num[j]; num[j] = num[i-1-j]; num[i-1-j] = t; }
            int len = 13;
            for(int j = 0; num[j]; j++) {
                final_score[len++] = num[j];
            }
            final_score[len] = '\0';
            
            game_draw_text(fb_width/2 - 80, fb_height/2, final_score, 0x00FF00);
            game_draw_text(fb_width/2 - 90, fb_height/2 + 30, "Press any key to exit", 0xFFFFFF);
        }
        
        // Increase score (survival time)
        if(game_running) {
            score++;
            if(score % 50 == 0 && enemy_speed < 8) {
                enemy_speed++; // Enemy gets faster every 50 points, max speed 8
            }
            // Add bonus points for being far from enemy
            int dx = player_x - enemy_x;
            int dy = player_y - enemy_y;
            if(dx < 0) dx = -dx;
            if(dy < 0) dy = -dy;
            if(dx > 200 || dy > 150) {
                score += 2; // Extra points for keeping distance
            }
        }
        
        // Handle input
        char key = 0;
        // Check for keyboard input
        if(inb(0x64) & 0x01) {
            uint8_t sc = inb(0x60);
            if(!(sc & 0x80)) { // Key press only
                static const char sc_to_ascii[] = {
                    0,0,'1','2','3','4','5','6','7','8','9','0','-','=',0,0,
                    'q','w','e','r','t','y','u','i','o','p','[',']',0,0,
                    'a','s','d','f','g','h','j','k','l',';',0,0,0,
                    0,'z','x','c','v','b','n','m',',','.','/',0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
                };
                if(sc < sizeof(sc_to_ascii)) key = sc_to_ascii[sc];
            }
        }
        
        // Process movement
        if(game_running) {
            switch(key) {
                case 'w': case 'W': player_y -= player_speed; break;
                case 's': case 'S': player_y += player_speed; break;
                case 'a': case 'A': player_x -= player_speed; break;
                case 'd': case 'D': player_x += player_speed; break;
                case 'q': case 'Q': game_running = 0; break;
            }
            
            // Keep player on screen (within border)
            if(player_x < 1) player_x = 1;
            if(player_x > (int)fb_width - player_size - 1) player_x = fb_width - player_size - 1;
            if(player_y < 21) player_y = 21;
            if(player_y > (int)fb_height - player_size - 1) player_y = fb_height - player_size - 1;
        }
        
        // Simple delay loop for consistent game speed
        for(volatile int d = 0; d < 80000; d++) { asm volatile("nop"); }
        
        // Exit on game over
        if(!game_running) {
            // Wait for keypress to exit
            while(1) {
                if(inb(0x64) & 0x01) {
                    inb(0x60);
                    break;
                }
            }
            break;
        }
    }
}