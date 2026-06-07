// apps/sound_studio/sound_studio.c
#include "sound_studio.h"
#include "../../external/gui.h"
#include "../../drivers/sb16.h"
#include "../../drivers/pit.h"
#include "../../fs/tmnt_fs.h"
#include "../../system/string.h"

extern uint32_t* fb;
extern uint32_t fb_width, fb_height, fb_pitch;
extern const unsigned char font_8x16[96][16];

// Track list
static char tracks[32][64];
static int track_count = 0;
static int current_track = -1;
static int is_playing = 0;
static char status_msg[256] = "🎵 TMNT Sound Studio - Shell Shock Beats";
static int stop_requested = 0;
static int tracks_played[32]; // Track which songs have been played this session

// Drawing helpers using public GUI functions
static void ss_putpixel(int x, int y, uint32_t color) {
    gui_fb_putpixel(x, y, color);
}

static void ss_fill_rect(int x, int y, int w, int h, uint32_t color) {
    gui_fb_fill_rect(x, y, w, h, color);
}

static void ss_draw_rect(int x, int y, int w, int h, uint32_t color) {
    gui_fb_draw_rect(x, y, w, h, color);
}

static void ss_draw_text(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    gui_fb_draw_text(x, y, str, fg, bg);
}

// Progress bar drawing
static void ss_draw_progress_bar(int x, int y, int w, int h, float progress) {
    ss_fill_rect(x, y, w, h, 0x0A0A0A);
    ss_draw_rect(x, y, w, h, 0x00AA00);
    
    int fill_w = (int)(w * progress);
    if(fill_w > 0) {
        for(int dx = 0; dx < fill_w; dx++) {
            uint8_t g = 100 + (dx * 100 / w);
            uint32_t color = (0x00 << 16) | (g << 8) | 0x00;
            for(int dy = 1; dy < h - 1; dy++) {
                ss_putpixel(x + 1 + dx, y + dy, color);
            }
        }
    }
}

// Button state
#define MAX_BTN 16
static int btn_count = 0;
static int btn_x[MAX_BTN], btn_y[MAX_BTN], btn_w[MAX_BTN], btn_h[MAX_BTN];
static char btn_label[MAX_BTN][32];
static int btn_action[MAX_BTN];

// Check if there are unplayed tracks
static int has_unplayed_tracks(void) {
    for(int i = 0; i < track_count; i++) {
        if(!tracks_played[i]) return 1;
    }
    return 0;
}

// Play the current track
static void play_current_track(void) {
    if(current_track < 0 || current_track >= track_count) return;
    
    // Build path
    char path[128];
    strcpy(path, "/user/music/");
    strcat(path, tracks[current_track]);
    
    if(!fs_file_exists(path)) {
        strcpy(status_msg, "❌ File not found: ");
        strcat(status_msg, tracks[current_track]);
        return;
    }
    
    uint32_t sz = fs_get_file_size(path);
    if(sz == 0) return;
    
    strcpy(status_msg, "🎵 Playing: ");
    strcat(status_msg, tracks[current_track]);
    
    stop_requested = 0;
    is_playing = 1;
    tracks_played[current_track] = 1;
    
    // Play the file in chunks
    uint32_t offset = 0;
    uint8_t chunk[65536];
    
    while(offset < sz && !stop_requested) {
        uint32_t chunk_size = 65536;
        if(offset + chunk_size > sz)
            chunk_size = sz - offset;
        
        int bytes_read = fs_read_file_chunk(path, chunk, chunk_size, offset);
        if(bytes_read <= 0) break;
        
        sb16_play_pcm_single(chunk, bytes_read, 8000);
        offset += bytes_read;
    }
    
    is_playing = 0;
    
    if(!stop_requested) {
        strcpy(status_msg, "✅ Done: ");
        strcat(status_msg, tracks[current_track]);
        
        // Only auto-play next if there are unplayed tracks
        if(has_unplayed_tracks()) {
            // Find next unplayed track
            int next = (current_track + 1) % track_count;
            int tried = 0;
            while(tracks_played[next] && tried < track_count) {
                next = (next + 1) % track_count;
                tried++;
            }
            if(tried < track_count) {
                current_track = next;
                play_current_track();
            } else {
                strcpy(status_msg, "🎉 All tracks played! Select a track to replay.");
            }
        } else {
            strcpy(status_msg, "🎉 All tracks played! Select a track to replay.");
        }
    }
}

// Play next unplayed track
static void play_next_track(void) {
    if(track_count == 0) return;
    
    sb16_stop();
    stop_requested = 0;
    is_playing = 0;
    
    // Find next unplayed track
    int next = (current_track + 1) % track_count;
    int tried = 0;
    while(tracks_played[next] && tried < track_count) {
        next = (next + 1) % track_count;
        tried++;
    }
    
    if(tried < track_count) {
        current_track = next;
        play_current_track();
    } else {
        strcpy(status_msg, "🎉 All tracks played! Use Refresh to reset.");
    }
}

// Play previous track
static void play_previous_track(void) {
    if(track_count == 0) return;
    
    sb16_stop();
    stop_requested = 0;
    is_playing = 0;
    
    current_track--;
    if(current_track < 0) current_track = track_count - 1;
    play_current_track();
}

// Draw function - TMNT themed
static void sound_studio_draw(int wx, int wy, int ww, int wh) {
    // Main background - dark sewer theme
    ss_fill_rect(wx + 2, wy + 28, ww - 4, wh - 30, 0x0A1A0E);
    
    // Title area - glowing green
    for(int dy = 0; dy < 28; dy++) {
        uint8_t g = 20 + dy * 4;
        uint32_t color = (0x00 << 16) | (g << 8) | 0x00;
        for(int dx = 4; dx < ww - 4; dx++) {
            ss_putpixel(wx + dx, wy + 30 + dy, color);
        }
    }
    ss_draw_rect(wx + 4, wy + 30, ww - 8, 28, 0x00FF00);
    ss_draw_text(wx + (ww - 220) / 2, wy + 36, "🐢 TMNT SOUND STUDIO 🎵", 0x00FF00, 0x0A2A0A);
    
    // Track list area with brick pattern border
    int list_x = wx + 15;
    int list_y = wy + 70;
    int list_w = ww - 30;
    int list_h = wh - 170;
    
    ss_fill_rect(list_x - 2, list_y - 2, list_w + 4, list_h + 4, 0x003300);
    ss_fill_rect(list_x, list_y, list_w, list_h, 0x0A0A0A);
    ss_draw_rect(list_x - 1, list_y - 1, list_w + 2, list_h + 2, 0x00AA00);
    
    // Track list header
    ss_fill_rect(list_x, list_y - 18, list_w, 18, 0x1A2A0A);
    ss_draw_text(list_x + 5, list_y - 16, "📀 TRACK LIST", 0x00FF00, 0x1A2A0A);
    
    // Draw tracks
    for(int i = 0; i < track_count; i++) {
        uint32_t bg = (i == current_track) ? 0x1A3A0A : 0x0A0A0A;
        uint32_t fg = (i == current_track) ? 0xFFFF00 : 0x00CC00;
        
        if(i % 2 == 0 && i != current_track) bg = 0x0F0F0F;
        
        // Track number with played indicator
        char num_str[8];
        int_to_str(i + 1, num_str);
        char display[128];
        strcpy(display, num_str);
        strcat(display, ". ");
        if(tracks_played[i]) strcat(display, "✓ ");
        strcat(display, tracks[i]);
        
        ss_fill_rect(list_x + 2, list_y + 2 + i * 20, list_w - 4, 19, bg);
        ss_draw_text(list_x + 10, list_y + 4 + i * 20, display, fg, bg);
        
        // Play indicator
        if(is_playing && i == current_track) {
            ss_draw_text(list_x + list_w - 80, list_y + 4 + i * 20, "▶ PLAYING", 0x00FF00, bg);
        }
    }
    
    if(track_count == 0) {
        ss_draw_text(list_x + 10, list_y + 5, "No tracks found in /user/music/", 0x666666, 0x0A0A0A);
        ss_draw_text(list_x + 10, list_y + 25, "Add .raw audio files to play music!", 0x666666, 0x0A0A0A);
    }
    
    // Progress bar
    int prog_y = list_y + list_h + 5;
    float progress = is_playing ? 0.5f : 0.0f;
    ss_draw_progress_bar(wx + 15, prog_y, ww - 30, 16, progress);
    
    // Status bar - TMNT themed
    int status_y = prog_y + 22;
    ss_fill_rect(wx + 10, status_y, ww - 20, 22, 0x0A1A0A);
    ss_draw_rect(wx + 10, status_y, ww - 20, 22, 0x00CC00);
    ss_draw_text(wx + 15, status_y + 3, status_msg, 0xFFFF00, 0x0A1A0A);
    
    // Control buttons - TMNT styled
    btn_count = 0;
    int btn_y_start = wy + wh - 30;
    int btn_spacing = 90;
    
    // Previous button
    btn_x[btn_count] = wx + 15;
    btn_y[btn_count] = btn_y_start;
    btn_w[btn_count] = 80; btn_h[btn_count] = 24;
    strcpy(btn_label[btn_count], "⏮ Prev");
    btn_action[btn_count] = 0;
    btn_count++;
    
    // Play button
    btn_x[btn_count] = wx + 15 + btn_spacing;
    btn_y[btn_count] = btn_y_start;
    btn_w[btn_count] = 80; btn_h[btn_count] = 24;
    strcpy(btn_label[btn_count], "▶ Play");
    btn_action[btn_count] = 1;
    btn_count++;
    
    // Stop button
    btn_x[btn_count] = wx + 15 + btn_spacing * 2;
    btn_y[btn_count] = btn_y_start;
    btn_w[btn_count] = 80; btn_h[btn_count] = 24;
    strcpy(btn_label[btn_count], "⏹ Stop");
    btn_action[btn_count] = 2;
    btn_count++;
    
    // Next button
    btn_x[btn_count] = wx + 15 + btn_spacing * 3;
    btn_y[btn_count] = btn_y_start;
    btn_w[btn_count] = 80; btn_h[btn_count] = 24;
    strcpy(btn_label[btn_count], "⏭ Next");
    btn_action[btn_count] = 3;
    btn_count++;
    
    // Refresh button (also resets played status)
    btn_x[btn_count] = wx + 15 + btn_spacing * 4;
    btn_y[btn_count] = btn_y_start;
    btn_w[btn_count] = 80; btn_h[btn_count] = 24;
    strcpy(btn_label[btn_count], "🔄 Refresh");
    btn_action[btn_count] = 4;
    btn_count++;
    
    // Draw TMNT styled buttons
    for(int i = 0; i < btn_count; i++) {
        // Button shadow
        ss_fill_rect(btn_x[i] + 2, btn_y[i] + 2, btn_w[i], btn_h[i], 0x000000);
        
        // Button gradient
        for(int dy = 0; dy < btn_h[i]; dy++) {
            uint8_t r = 0;
            uint8_t g = 40 + dy * 3;
            uint8_t b = 0;
            uint32_t color = (r << 16) | (g << 8) | b;
            for(int dx = 0; dx < btn_w[i]; dx++) {
                ss_putpixel(btn_x[i] + dx, btn_y[i] + dy, color);
            }
        }
        
        // Button border
        ss_draw_rect(btn_x[i], btn_y[i], btn_w[i], btn_h[i], 0x00FF00);
        
        // Button highlight
        for(int dx = 1; dx < btn_w[i] - 1; dx++) {
            ss_putpixel(btn_x[i] + dx, btn_y[i] + 1, 0x44FF44);
        }
        
        // Text
        int tx = btn_x[i] + (btn_w[i] - strlen(btn_label[i]) * 8) / 2;
        int ty = btn_y[i] + (btn_h[i] - 16) / 2;
        ss_draw_text(tx, ty, btn_label[i], 0xFFFFFF, 0x006600);
    }
}

// Click handler
static void sound_studio_click(int mx, int my) {
    // Check button clicks
    for(int i = 0; i < btn_count; i++) {
        if(mx >= btn_x[i] && mx < btn_x[i] + btn_w[i] &&
           my >= btn_y[i] && my < btn_y[i] + btn_h[i]) {
            
            switch(btn_action[i]) {
                case 0: // Previous
                    play_previous_track();
                    break;
                    
                case 1: // Play
                    play_current_track();
                    break;
                    
                case 2: // Stop
                    stop_requested = 1;
                    sb16_stop();
                    is_playing = 0;
                    strcpy(status_msg, "⏹ Stopped");
                    break;
                    
                case 3: // Next
                    play_next_track();
                    break;
                    
                case 4: // Refresh (reset played status too)
                    {
                        file_list_t* files = fs_list_directory("/user/music");
                        track_count = 0;
                        if(files) {
                            for(int j = 0; j < files->count && track_count < 32; j++) {
                                strcpy(tracks[track_count], files->names[j]);
                                tracks_played[track_count] = 0; // Reset played status
                                track_count++;
                            }
                        }
                        if(track_count > 0 && current_track >= track_count) {
                            current_track = 0;
                        }
                        strcpy(status_msg, "📂 Track list refreshed - ");
                        char count_str[8];
                        int_to_str(track_count, count_str);
                        strcat(status_msg, count_str);
                        strcat(status_msg, " tracks found");
                    }
                    break;
            }
        }
    }
    
    // Check if clicked on a track in the list
    int list_y = 70;
    for(int i = 0; i < track_count; i++) {
        if(my >= list_y + 2 + i * 20 && my < list_y + 2 + i * 20 + 19) {
            if(mx >= 15 && mx < 15 + 500) {
                current_track = i;
                strcpy(status_msg, "📀 Selected: ");
                strcat(status_msg, tracks[i]);
                play_current_track();
            }
        }
    }
}

// Keyboard handler
static void sound_studio_key(char key) {
    switch(key) {
        case ' ': // Space - Play
            play_current_track();
            break;
            
        case 'n': // Next track
        case 'N':
            play_next_track();
            break;
            
        case 'p': // Previous track
        case 'P':
            play_previous_track();
            break;
            
        case 's': // Stop
        case 'S':
            stop_requested = 1;
            sb16_stop();
            is_playing = 0;
            strcpy(status_msg, "⏹ Stopped");
            break;
            
        case 'r': // Reset played tracks
        case 'R':
            for(int i = 0; i < track_count; i++) {
                tracks_played[i] = 0;
            }
            strcpy(status_msg, "🔄 Play history reset - All tracks available");
            break;
    }
}

// Open function
void sound_studio_open(void) {
    // Reset state
    is_playing = 0;
    stop_requested = 0;
    
    // Load track list
    file_list_t* files = fs_list_directory("/user/music");
    track_count = 0;
    if(files) {
        for(int i = 0; i < files->count && track_count < 32; i++) {
            strcpy(tracks[track_count], files->names[i]);
            tracks_played[track_count] = 0;
            track_count++;
        }
    }
    
    if(track_count > 0) {
        current_track = 0;
        strcpy(status_msg, "📀 Ready - ");
        char count_str[8];
        int_to_str(track_count, count_str);
        strcat(status_msg, count_str);
        strcat(status_msg, " tracks loaded. Click Play to start!");
    } else {
        current_track = -1;
        strcpy(status_msg, "📂 No tracks found. Add .raw files to /user/music/");
    }
    
    gui_run_auto_app("🐢 TMNT Sound Studio", 60, 40, 600, 460, sound_studio_draw, sound_studio_click, sound_studio_key);
}