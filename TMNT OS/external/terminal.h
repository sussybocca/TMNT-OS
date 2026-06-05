#include "../system/types.h"
#ifndef TERMINAL_H
#define TERMINAL_H

void term_fb_clear(void);
void term_fb_putchar(char c);
void term_fb_print(const char* str);
void tm_shell_command(char* input);

extern int  terminal_mode;
extern uint32_t term_fg, term_bg;
#endif