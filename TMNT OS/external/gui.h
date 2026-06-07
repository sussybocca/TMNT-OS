#ifndef GUI_H
#define GUI_H
#include "../system/types.h"

void gui_draw_desktop(void);
void gui_save_cursor_bg(void);
void gui_restore_cursor_bg(void);
void gui_draw_cursor(void);
int  gui_hit_icon(void);
void gui_handle_click(void);
void gui_init(void);

// App framework - OLD SYSTEM (kept for compatibility)
void gui_register_app(const char* name, int wx, int wy, int ww, int wh,
                      void (*init)(void), void (*draw)(int,int,int,int),
                      void (*click)(int,int), int custom_loop);
void gui_run_app(int index);

// App framework - NEW AUTOMATIC SYSTEM
void gui_register_auto_app(const char* name, const char* icon_name, void (*open_func)(void));
void gui_run_auto_app(const char* title, int wx, int wy, int ww, int wh,
                      void (*draw_content)(int wx, int wy, int ww, int wh),
                      void (*handle_click)(int mx, int my));

// Public drawing functions for apps
void gui_fb_putpixel(int x, int y, uint32_t color);
uint32_t gui_fb_getpixel(int x, int y);
void gui_fb_fill_rect(int x, int y, int w, int h, uint32_t color);
void gui_fb_draw_rect(int x, int y, int w, int h, uint32_t color);
void gui_fb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg);
void gui_fb_draw_text(int x, int y, const char* str, uint32_t fg, uint32_t bg);

// Window drawing functions
void gui_draw_window(int x, int y, int w, int h, const char* title);
void gui_draw_tmnt_button(int x, int y, int w, int h, const char* text, uint32_t bg);

extern int gui_mode;
extern int mouse_x, mouse_y;
extern int mouse_btn_left, mouse_btn_old_left;
extern uint32_t cursor_bg[12][4];
#endif