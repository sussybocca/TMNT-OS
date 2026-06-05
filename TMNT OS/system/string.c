#include "string.h"

int strlen(const char* str) {
    int len = 0;
    while(str[len]) len++;
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* o = dest;
    while((*dest++ = *src++));
    return o;
}

char* strncpy(char* dest, const char* src, int n) {
    char* o = dest;
    while(n-- && (*dest++ = *src++));
    while(n-- > 0) *dest++ = '\0';
    return o;
}

int strcmp(const char* s1, const char* s2) {
    while(*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

char* strcat(char* dest, const char* src) {
    char* o = dest;
    while(*dest) dest++;
    while((*dest++ = *src++));
    return o;
}

char* strchr(const char* str, char c) {
    while(*str) { if(*str == c) return (char*)str; str++; }
    return 0;
}

char* strrchr(const char* str, char c) {
    const char* last = 0;
    while(*str) { if(*str == c) last = str; str++; }
    return (char*)last;
}

char* strstr(const char* haystack, const char* needle) {
    int nl = strlen(needle);
    if(!nl) return (char*)haystack;
    while(*haystack) {
        int i;
        for(i = 0; i < nl && haystack[i] == needle[i]; i++);
        if(i == nl) return (char*)haystack;
        haystack++;
    }
    return 0;
}

char* strtok(char* str, const char* delim) {
    static char* next = 0;
    if(str) next = str;
    if(!next) return 0;
    while(*next && strchr(delim, *next)) next++;
    if(!*next) return 0;
    char* token = next;
    while(*next && !strchr(delim, *next)) next++;
    if(*next) *next++ = '\0';
    return token;
}

void int_to_str(int num, char* str) {
    int i = 0, neg = 0;
    if(num == 0) { str[i++] = '0'; str[i] = '\0'; return; }
    if(num < 0) { neg = 1; num = -num; }
    while(num) { str[i++] = '0' + (num % 10); num /= 10; }
    if(neg) str[i++] = '-';
    str[i] = '\0';
    for(int s = 0, e = i-1; s < e; s++, e--) {
        char t = str[s]; str[s] = str[e]; str[e] = t;
    }
}

int str_to_int(const char* str) {
    int result = 0, sign = 1;
    if(*str == '-') { sign = -1; str++; }
    while(*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return result * sign;
}

int abs(int n) { return n < 0 ? -n : n; }