#ifndef KEYBOARD_H
#define KEYBOARD_H
#include "../../system/types.h"
#define KEYBOARD_PORT 0x60
#define KEYBOARD_BUFFER_SIZE 256

void keyboard_init();
char keyboard_getchar();
int keyboard_getint();
char* keyboard_readline();
void keyboard_handler();
#endif