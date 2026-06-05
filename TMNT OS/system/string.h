#ifndef STRING_H
#define STRING_H
#include "types.h"

int strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, int n);
int strcmp(const char* str1, const char* str2);
char* strcat(char* dest, const char* src);
char* strchr(const char* str, char c);
char* strrchr(const char* str, char c);
char* strstr(const char* haystack, const char* needle);
char* strtok(char* str, const char* delim);
void int_to_str(int num, char* str);
int str_to_int(const char* str);
int abs(int n);
#endif