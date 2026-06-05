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

extern int gui_mode;
extern int mouse_x, mouse_y;
extern int mouse_btn_old_left;
extern uint32_t cursor_bg[12][4];
#endif