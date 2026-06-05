#ifndef SHELL_H
#define SHELL_H

#include "../system/types.h"

void shell_init();
void shell_process();
void shell_register_command(const char* name, void (*func)(char*), const char* desc);
void print_prompt();
void execute_command(char* input);

#endif
