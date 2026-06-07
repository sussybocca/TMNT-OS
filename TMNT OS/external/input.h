#ifndef INPUT_H
#define INPUT_H
#include "../system/types.h"

char  keyboard_getchar_poll(void);
char* keyboard_readline_poll(void);
char* keyboard_read_multiline(void);
int   keyboard_getint_poll(void);
void  mouse_init(void);

extern int mouse_x, mouse_y;
extern int mouse_btn_left, mouse_btn_old_left;
extern int paint_mode;  // Set to 1 when paint studio is active
#endif