#include "paint_studio.h"
#include "../../system/string.h"

extern uint32_t* fb;
extern uint32_t fb_width, fb_height, fb_pitch;
extern const unsigned char font_8x16[96][16];

// Window functions from gui.c
extern void gui_draw_window(int x, int y, int w, int h, const char* title);
extern void gui_draw_tmnt_button(int x, int y, int w, int h, const char* text, uint32_t bg);

// ===== EMBEDDED I/O (same logic as input.c but NON-BLOCKING) =====
static inline uint8_t ps_inb(uint16_t port) {
    uint8_t r;
    asm volatile("inb %1,%0":"=a"(r):"Nd"(port));
    return r;
}

// Local mouse state
static int ps_mx = 512, ps_my = 384;
static int ps_btn = 0, ps_btn_old = 0;

// Local cursor background save (same as gui.c cursor_bg)
static uint32_t ps_cursor_bg[12][4];
static int ps_cursor_saved = 0;

static void ps_save_cursor_bg(void) {
    for(int cy = 0; cy < 12; cy++)
        for(int cx = 0; cx < 4; cx++) {
            int sx = ps_mx + cx, sy = ps_my + cy;
            if(sx >= 0 && sx < (int)fb_width && sy >= 0 && sy < (int)fb_height)
                ps_cursor_bg[cy][cx] = fb[sy * (fb_pitch / 4) + sx];
            else
                ps_cursor_bg[cy][cx] = 0;
        }
    ps_cursor_saved = 1;
}

static void ps_restore_cursor_bg(void) {
    if(!ps_cursor_saved) return;
    for(int cy = 0; cy < 12; cy++)
        for(int cx = 0; cx < 4; cx++) {
            int sx = ps_mx + cx, sy = ps_my + cy;
            if(sx >= 0 && sx < (int)fb_width && sy >= 0 && sy < (int)fb_height)
                fb[sy * (fb_pitch / 4) + sx] = ps_cursor_bg[cy][cx];
        }
    ps_cursor_saved = 0;
}

static void ps_draw_cursor(void) {
    for(int cy = 0; cy < 12; cy++)
        for(int cx = 0; cx < 4; cx++) {
            int sx = ps_mx + cx, sy = ps_my + cy;
            if(sx >= 0 && sx < (int)fb_width && sy >= 0 && sy < (int)fb_height)
                fb[sy * (fb_pitch / 4) + sx] = 0xFFFFFF;
        }
    for(int cy = 2; cy < 10; cy++)
        for(int cx = 1; cx < 3; cx++) {
            int sx = ps_mx + cx, sy = ps_my + cy;
            if(sx >= 0 && sx < (int)fb_width && sy >= 0 && sy < (int)fb_height)
                fb[sy * (fb_pitch / 4) + sx] = 0x000000;
        }
}

// Non-blocking input poll - processes ALL pending mouse/keyboard data, returns immediately
static void ps_poll_input(void) {
    int moved = 0;
    
    // Process ALL pending PS/2 data
    int max_loops = 100; // Safety limit
    while(max_loops-- > 0) {
        uint8_t s = ps_inb(0x64);
        if(!(s & 0x01)) break; // No more data
        
        uint8_t data = ps_inb(0x60);
        
        // Check bit 5 of status to know if this was mouse data
        if(s & 0x20) {
            // Mouse data - we need 3 bytes
            static int mcycle = 0;
            static uint8_t mbuf[3];
            mbuf[mcycle++] = data;
            if(mcycle == 3) {
                mcycle = 0;
                
                // Restore old cursor position first (if we had a valid save)
                if(ps_cursor_saved) {
                    ps_restore_cursor_bg();
                }
                
                int dx = mbuf[1];
                int dy = mbuf[2];
                if(mbuf[0] & 0x10) dx -= 256;
                if(mbuf[0] & 0x20) dy -= 256;
                ps_btn = (mbuf[0] & 0x01) ? 1 : 0;
                ps_mx += dx;
                ps_my -= dy;
                if(ps_mx < 0) ps_mx = 0;
                if(ps_my < 0) ps_my = 0;
                if(ps_mx >= (int)fb_width) ps_mx = fb_width - 1;
                if(ps_my >= (int)fb_height) ps_my = fb_height - 1;
                
                // Save background at new position and draw cursor
                ps_save_cursor_bg();
                ps_draw_cursor();
                moved = 1;
            }
        }
        // Keyboard data is just consumed/discarded in paint mode
    }
}

// ===== DRAWING FUNCTIONS =====
static void ps_putpixel(int x, int y, uint32_t color) {
    if(x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height)
        fb[y * (fb_pitch / 4) + x] = color;
}

static uint32_t ps_getpixel(int x, int y) {
    if(x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height)
        return fb[y * (fb_pitch / 4) + x];
    return 0;
}

static void ps_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for(int dy = 0; dy < h; dy++)
        for(int dx = 0; dx < w; dx++)
            ps_putpixel(x + dx, y + dy, color);
}

static void ps_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for(int i = 0; i < w; i++) { ps_putpixel(x+i, y, color); ps_putpixel(x+i, y+h-1, color); }
    for(int i = 0; i < h; i++) { ps_putpixel(x, y+i, color); ps_putpixel(x+w-1, y+i, color); }
}

static void ps_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    if(c < 32 || c > 126) return;
    const unsigned char* glyph = font_8x16[c - 32];
    for(int row = 0; row < 16; row++) {
        unsigned char line = glyph[row];
        for(int col = 0; col < 8; col++)
            ps_putpixel(x + col, y + row, (line & (0x80 >> col)) ? fg : bg);
    }
}

static void ps_draw_text(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    int cx = x;
    while(*str) {
        if(*str == '\n') { cx = x; y += 16; }
        else { ps_draw_char(cx, y, *str, fg, bg); cx += 8; }
        str++;
    }
}

// ===== COLOR PALETTE =====
static uint32_t palette[] = {
    0x000000, 0xE8E8D0, 0xFF0000, 0x0000FF,
    0xFF8800, 0x8800FF, 0x00FF00, 0xDDDD00,
    0x00CCCC, 0xCC00CC, 0x8B4513, 0x808080,
    0x004400, 0x444444, 0xCCAA00, 0xFF4444,
};
static int palette_count = 16;
static uint32_t current_color = 0x00FF00;
static uint32_t canvas_bg = 0xE8E8D0;
static int brush_size = 3;
static int current_tool = 0;

// ===== WINDOW STATE =====
static int win_x = 100, win_y = 50;
static int win_w = 800, win_h = 520;
static int win_minimized = 0;
static int win_maximized = 0;
static int dragging = 0;
static int drag_offset_x, drag_offset_y;
static int line_start_x, line_start_y;
static int line_drawing = 0;

// ===== CANVAS =====
#define CANVAS_OFFSET_X 10
#define CANVAS_OFFSET_Y 28
#define CANVAS_W 600
#define CANVAS_H 420
static uint32_t canvas[600][420];
static int canvas_initialized = 0;

static int dirty_min_x, dirty_min_y, dirty_max_x, dirty_max_y;
static int has_dirty = 0;

static void mark_dirty(int x, int y, int w, int h) {
    if(!has_dirty) {
        dirty_min_x = x; dirty_min_y = y;
        dirty_max_x = x + w; dirty_max_y = y + h;
        has_dirty = 1;
    } else {
        if(x < dirty_min_x) dirty_min_x = x;
        if(y < dirty_min_y) dirty_min_y = y;
        if(x + w > dirty_max_x) dirty_max_x = x + w;
        if(y + h > dirty_max_y) dirty_max_y = y + h;
    }
    if(dirty_min_x < 0) dirty_min_x = 0;
    if(dirty_min_y < 0) dirty_min_y = 0;
    if(dirty_max_x > CANVAS_W) dirty_max_x = CANVAS_W;
    if(dirty_max_y > CANVAS_H) dirty_max_y = CANVAS_H;
}

static void canvas_init(void) {
    for(int y = 0; y < CANVAS_H; y++)
        for(int x = 0; x < CANVAS_W; x++)
            canvas[x][y] = canvas_bg;
    canvas_initialized = 1;
    mark_dirty(0, 0, CANVAS_W, CANVAS_H);
}

static void canvas_putpixel(int x, int y, uint32_t color) {
    if(x >= 0 && x < CANVAS_W && y >= 0 && y < CANVAS_H) {
        canvas[x][y] = color;
        mark_dirty(x, y, 1, 1);
    }
}

static void canvas_flush_dirty(int offset_x, int offset_y) {
    if(!has_dirty) return;
    for(int y = dirty_min_y; y < dirty_max_y; y++)
        for(int x = dirty_min_x; x < dirty_max_x; x++)
            ps_putpixel(offset_x + x, offset_y + y, canvas[x][y]);
    has_dirty = 0;
}

static void canvas_draw_all(int offset_x, int offset_y) {
    for(int y = 0; y < CANVAS_H; y++)
        for(int x = 0; x < CANVAS_W; x++)
            ps_putpixel(offset_x + x, offset_y + y, canvas[x][y]);
    has_dirty = 0;
}

static int is_in_canvas(int mx, int my) {
    int cx = mx - (win_x + CANVAS_OFFSET_X);
    int cy = my - (win_y + CANVAS_OFFSET_Y);
    return (cx >= 0 && cx < CANVAS_W && cy >= 0 && cy < CANVAS_H);
}

static void flood_fill(int sx, int sy, uint32_t target, uint32_t replacement) {
    if(target == replacement) return;
    if(sx < 0 || sx >= CANVAS_W || sy < 0 || sy >= CANVAS_H) return;
    if(canvas[sx][sy] != target) return;
    int stack_x[4096], stack_y[4096];
    int sp = 0;
    stack_x[sp] = sx; stack_y[sp] = sy; sp++;
    int min_x = sx, min_y = sy, max_x = sx, max_y = sy;
    while(sp > 0 && sp < 4095) {
        sp--;
        int x = stack_x[sp], y = stack_y[sp];
        if(x < 0 || x >= CANVAS_W || y < 0 || y >= CANVAS_H) continue;
        if(canvas[x][y] != target) continue;
        canvas[x][y] = replacement;
        if(x < min_x) min_x = x; if(y < min_y) min_y = y;
        if(x > max_x) max_x = x; if(y > max_y) max_y = y;
        stack_x[sp] = x + 1; stack_y[sp] = y; sp++;
        stack_x[sp] = x - 1; stack_y[sp] = y; sp++;
        stack_x[sp] = x; stack_y[sp] = y + 1; sp++;
        stack_x[sp] = x; stack_y[sp] = y - 1; sp++;
    }
    mark_dirty(min_x, min_y, max_x - min_x + 1, max_y - min_y + 1);
}

static void draw_brush(int cx, int cy) {
    for(int dy = -brush_size; dy <= brush_size; dy++)
        for(int dx = -brush_size; dx <= brush_size; dx++)
            if(dx*dx + dy*dy <= brush_size*brush_size)
                canvas_putpixel(cx + dx, cy + dy, current_color);
}

static void draw_eraser(int cx, int cy) {
    for(int dy = -brush_size; dy <= brush_size; dy++)
        for(int dx = -brush_size; dx <= brush_size; dx++)
            if(dx*dx + dy*dy <= brush_size*brush_size)
                canvas_putpixel(cx + dx, cy + dy, canvas_bg);
}

static void draw_line(int x1, int y1, int x2, int y2) {
    int dx = x2 - x1, dy = y2 - y1;
    int adx = (dx < 0) ? -dx : dx, ady = (dy < 0) ? -dy : dy;
    int steps = (adx > ady) ? adx : ady;
    if(steps == 0) steps = 1;
    int fx = x1 * steps, fy = y1 * steps;
    int xinc = dx, yinc = dy;
    for(int i = 0; i <= steps; i++) {
        int px = fx / steps, py = fy / steps;
        for(int bdy = -brush_size; bdy <= brush_size; bdy++)
            for(int bdx = -brush_size; bdx <= brush_size; bdx++)
                if(bdx*bdx + bdy*bdy <= brush_size*brush_size)
                    canvas_putpixel(px + bdx, py + bdy, current_color);
        fx += xinc; fy += yinc;
    }
    mark_dirty(x1 - brush_size, y1 - brush_size, (x2 > x1 ? x2 - x1 : x1 - x2) + brush_size*2, (y2 > y1 ? y2 - y1 : y1 - y2) + brush_size*2);
}

static void draw_rect_tool(int x1, int y1, int x2, int y2) {
    int rx = (x1 < x2) ? x1 : x2, ry = (y1 < y2) ? y1 : y2;
    int rw = (x2 > x1) ? (x2 - x1) : (x1 - x2), rh = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    for(int x = rx; x <= rx + rw; x++)
        for(int bdy = -brush_size; bdy <= brush_size; bdy++) {
            canvas_putpixel(x, ry + bdy, current_color);
            canvas_putpixel(x, ry + rh + bdy, current_color);
        }
    for(int y = ry; y <= ry + rh; y++)
        for(int bdx = -brush_size; bdx <= brush_size; bdx++) {
            canvas_putpixel(rx + bdx, y, current_color);
            canvas_putpixel(rx + rw + bdx, y, current_color);
        }
    mark_dirty(rx - brush_size, ry - brush_size, rw + brush_size*2, rh + brush_size*2);
}

static uint32_t bg_save[1024][768];
static int bg_saved = 0;
static int bg_save_x, bg_save_y, bg_save_w, bg_save_h;

static void save_background(int x, int y, int w, int h) {
    if(w > 1024) w = 1024; if(h > 768) h = 768;
    bg_save_x = x; bg_save_y = y; bg_save_w = w; bg_save_h = h;
    for(int dy = 0; dy < h && dy < 768; dy++)
        for(int dx = 0; dx < w && dx < 1024; dx++)
            bg_save[dx][dy] = ps_getpixel(x + dx, y + dy);
    bg_saved = 1;
}

static void restore_background(void) {
    if(!bg_saved) return;
    // Restore cursor background first so it doesn't get overwritten
    ps_restore_cursor_bg();
    for(int dy = 0; dy < bg_save_h; dy++)
        for(int dx = 0; dx < bg_save_w; dx++)
            ps_putpixel(bg_save_x + dx, bg_save_y + dy, bg_save[dx][dy]);
    bg_saved = 0;
    ps_cursor_saved = 0;
}

void paint_studio_open(void) {
    if(!canvas_initialized) canvas_init();
    
    // Init cursor at current mouse position
    extern int mouse_x, mouse_y;
    ps_mx = mouse_x;
    ps_my = mouse_y;
    ps_save_cursor_bg();
    ps_draw_cursor();
    
    save_background(win_x, win_y, win_w + 5, win_h + 5);
    
    int prev_minimized = 0, prev_win_x = win_x, prev_win_y = win_y;
    int need_full_redraw = 1, need_toolbar_redraw = 1;
    int prev_tool = -1, prev_brush_size = -1;
    uint32_t prev_color = 0;
    ps_btn_old = ps_btn;
    
    while(1) {
        // Poll input (non-blocking - processes all pending mouse/keyboard data)
        ps_poll_input();
        
        if(current_tool != prev_tool || brush_size != prev_brush_size || current_color != prev_color) {
            need_toolbar_redraw = 1;
            prev_tool = current_tool; prev_brush_size = brush_size; prev_color = current_color;
        }
        
        if(win_minimized && !prev_minimized) {
            restore_background();
            prev_minimized = 1;
            for(volatile int d = 0; d < 200000; d++) asm volatile("nop");
            continue;
        }
        if(!win_minimized && prev_minimized) {
            ps_save_cursor_bg();
            ps_draw_cursor();
            save_background(win_x, win_y, win_w + 5, win_h + 5);
            need_full_redraw = 1; need_toolbar_redraw = 1; prev_minimized = 0;
            ps_btn_old = ps_btn;
        }
        
        if(win_x != prev_win_x || win_y != prev_win_y) {
            restore_background();
            ps_save_cursor_bg();
            ps_draw_cursor();
            save_background(win_x, win_y, win_w + 5, win_h + 5);
            need_full_redraw = 1; need_toolbar_redraw = 1;
            prev_win_x = win_x; prev_win_y = win_y;
        }
        
        if(!win_minimized) {
            if(need_full_redraw) {
                gui_draw_window(win_x, win_y, win_w, win_h, "🐢 TMNT Paint Studio 🐢");
                ps_draw_rect(win_x + CANVAS_OFFSET_X - 1, win_y + CANVAS_OFFSET_Y - 1, CANVAS_W + 2, CANVAS_H + 2, 0x004400);
                canvas_draw_all(win_x + CANVAS_OFFSET_X, win_y + CANVAS_OFFSET_Y);
                need_full_redraw = 0; need_toolbar_redraw = 1;
                // Redraw cursor on top
                ps_save_cursor_bg();
                ps_draw_cursor();
            } else {
                canvas_flush_dirty(win_x + CANVAS_OFFSET_X, win_y + CANVAS_OFFSET_Y);
            }
            
            if(need_toolbar_redraw) {
                int tx = win_x + CANVAS_OFFSET_X + CANVAS_W + 10, ty = win_y + 30;
                ps_fill_rect(tx - 5, ty - 5, 175, win_h - 40, 0x1A1A2E);
                ps_draw_rect(tx - 5, ty - 5, 175, win_h - 40, 0x00AA00);
                ps_draw_text(tx, ty, "TOOLS:", 0x00FF00, 0x1A1A2E);
                gui_draw_tmnt_button(tx, ty+20, 80, 25, "Brush", current_tool==0?0x006600:0x333333);
                gui_draw_tmnt_button(tx+85, ty+20, 80, 25, "Eraser", current_tool==1?0x006600:0x333333);
                gui_draw_tmnt_button(tx, ty+50, 80, 25, "Line", current_tool==2?0x006600:0x333333);
                gui_draw_tmnt_button(tx+85, ty+50, 80, 25, "Rect", current_tool==3?0x006600:0x333333);
                gui_draw_tmnt_button(tx, ty+80, 80, 25, "Fill", current_tool==4?0x006600:0x333333);
                ps_draw_text(tx, ty+115, "SIZE:", 0x00FF00, 0x1A1A2E);
                char ss[4]; ss[0]='0'+brush_size; ss[1]=0;
                ps_draw_text(tx+50, ty+115, ss, 0xDDDDDD, 0x1A1A2E);
                gui_draw_tmnt_button(tx, ty+135, 30, 20, "-", 0x333333);
                gui_draw_tmnt_button(tx+35, ty+135, 30, 20, "+", 0x333333);
                ps_draw_text(tx, ty+165, "COLORS:", 0x00FF00, 0x1A1A2E);
                for(int i=0;i<palette_count;i++){
                    int px=tx+(i%4)*40, py=ty+185+(i/4)*30;
                    ps_fill_rect(px,py,30,20,palette[i]);
                    ps_draw_rect(px,py,30,20,0x888888);
                    if(palette[i]==current_color) ps_draw_rect(px-1,py-1,32,22,0x00FF00);
                }
                ps_draw_text(tx, ty+330, "CURRENT:", 0x00FF00, 0x1A1A2E);
                ps_fill_rect(tx, ty+350, 60, 40, current_color);
                ps_draw_rect(tx, ty+350, 60, 40, 0x888888);
                gui_draw_tmnt_button(tx, ty+405, 80, 25, "Clear", 0x8B0000);
                gui_draw_tmnt_button(tx+85, ty+405, 80, 25, "Exit", 0x444444);
                need_toolbar_redraw = 0;
                // Redraw cursor
                ps_save_cursor_bg();
                ps_draw_cursor();
            }
        }
        
        int mx = ps_mx, my = ps_my;
        int btn_now = ps_btn;
        int btn_click = (ps_btn_old == 0 && btn_now == 1);
        ps_btn_old = btn_now;
        
        int close_x = win_x + win_w - 24, close_y = win_y + 4;
        if(btn_click && mx >= close_x && mx < close_x+18 && my >= close_y && my < close_y+18) {
            restore_background();
            break;
        }
        
        int max_x = win_x + win_w - 46;
        if(btn_click && mx >= max_x && mx < max_x+18 && my >= close_y && my < close_y+18) {
            restore_background();
            if(win_maximized){win_x=100;win_y=50;win_w=800;win_h=520;win_maximized=0;}
            else{win_x=0;win_y=0;win_w=fb_width;win_h=fb_height-40;win_maximized=1;}
            ps_save_cursor_bg();
            ps_draw_cursor();
            save_background(win_x,win_y,win_w+5,win_h+5);
            need_full_redraw=1;
        }
        
        int min_x = win_x + win_w - 68;
        if(btn_click && mx >= min_x && mx < min_x+18 && my >= close_y && my < close_y+18) {
            if(!win_minimized){restore_background();win_minimized=1;}
            else{win_minimized=0;ps_save_cursor_bg();ps_draw_cursor();save_background(win_x,win_y,win_w+5,win_h+5);need_full_redraw=1;}
        }
        
        if(!win_maximized && !win_minimized && btn_click && my >= win_y && my < win_y+24 && mx >= win_x && mx < win_x+win_w-70) {
            dragging=1;drag_offset_x=mx-win_x;drag_offset_y=my-win_y;
        }
        if(!btn_now) dragging=0;
        if(dragging){
            int nx=mx-drag_offset_x, ny=my-drag_offset_y;
            if(nx<0)nx=0;if(ny<0)ny=0;
            if(nx+win_w>(int)fb_width)nx=fb_width-win_w;
            if(ny+win_h>(int)fb_height)ny=fb_height-win_h;
            if(nx!=win_x||ny!=win_y){
                restore_background();
                win_x=nx;win_y=ny;
                ps_save_cursor_bg();
                ps_draw_cursor();
                save_background(win_x,win_y,win_w+5,win_h+5);
                need_full_redraw=1;
            }
        }
        
        if(!win_minimized){
            int tbx=win_x+CANVAS_OFFSET_X+CANVAS_W+10, tby=win_y+50;
            if(btn_click){
                if(mx>=tbx&&mx<tbx+80&&my>=tby&&my<tby+25) current_tool=0;
                if(mx>=tbx+85&&mx<tbx+165&&my>=tby&&my<tby+25) current_tool=1;
                if(mx>=tbx&&mx<tbx+80&&my>=tby+30&&my<tby+55) current_tool=2;
                if(mx>=tbx+85&&mx<tbx+165&&my>=tby+30&&my<tby+55) current_tool=3;
                if(mx>=tbx&&mx<tbx+80&&my>=tby+60&&my<tby+85) current_tool=4;
                if(mx>=tbx&&mx<tbx+30&&my>=tby+115&&my<tby+135&&brush_size>1) brush_size--;
                if(mx>=tbx+35&&mx<tbx+65&&my>=tby+115&&my<tby+135&&brush_size<9) brush_size++;
                for(int i=0;i<palette_count;i++){
                    int px=tbx+(i%4)*40, py=tby+165+(i/4)*30;
                    if(mx>=px&&mx<px+30&&my>=py&&my<py+20) current_color=palette[i];
                }
                if(mx>=tbx&&mx<tbx+80&&my>=tby+385&&my<tby+410){canvas_init();need_full_redraw=1;}
                if(mx>=tbx+85&&mx<tbx+165&&my>=tby+385&&my<tby+410){restore_background();break;}
            }
            
            if(is_in_canvas(mx,my)){
                int cx=mx-(win_x+CANVAS_OFFSET_X), cy=my-(win_y+CANVAS_OFFSET_Y);
                if(btn_now&&!btn_click&&(current_tool==0||current_tool==1)){
                    if(current_tool==0) draw_brush(cx,cy); else draw_eraser(cx,cy);
                    ps_save_cursor_bg();
                    canvas_flush_dirty(win_x+CANVAS_OFFSET_X, win_y+CANVAS_OFFSET_Y);
                    ps_draw_cursor();
                }
                if(btn_click){
                    if(current_tool==0){draw_brush(cx,cy);ps_save_cursor_bg();canvas_flush_dirty(win_x+CANVAS_OFFSET_X,win_y+CANVAS_OFFSET_Y);ps_draw_cursor();}
                    else if(current_tool==1){draw_eraser(cx,cy);ps_save_cursor_bg();canvas_flush_dirty(win_x+CANVAS_OFFSET_X,win_y+CANVAS_OFFSET_Y);ps_draw_cursor();}
                    else if(current_tool==2){line_start_x=cx;line_start_y=cy;line_drawing=1;}
                    else if(current_tool==3){line_start_x=cx;line_start_y=cy;line_drawing=2;}
                    else if(current_tool==4){flood_fill(cx,cy,canvas[cx][cy],current_color);ps_save_cursor_bg();canvas_flush_dirty(win_x+CANVAS_OFFSET_X,win_y+CANVAS_OFFSET_Y);ps_draw_cursor();}
                }
                if(!btn_now&&line_drawing==1){draw_line(line_start_x,line_start_y,cx,cy);line_drawing=0;ps_save_cursor_bg();canvas_flush_dirty(win_x+CANVAS_OFFSET_X,win_y+CANVAS_OFFSET_Y);ps_draw_cursor();}
                if(!btn_now&&line_drawing==2){draw_rect_tool(line_start_x,line_start_y,cx,cy);line_drawing=0;ps_save_cursor_bg();canvas_flush_dirty(win_x+CANVAS_OFFSET_X,win_y+CANVAS_OFFSET_Y);ps_draw_cursor();}
            }
        }
        
        for(volatile int d=0;d<40000;d++) asm volatile("nop");
    }
}