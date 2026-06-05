#ifndef VGA_H
#define VGA_H

#include "../../system/types.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

#define VGA_COLOR_BLACK 0
#define VGA_COLOR_BLUE 1
#define VGA_COLOR_GREEN 2
#define VGA_COLOR_CYAN 3
#define VGA_COLOR_RED 4
#define VGA_COLOR_MAGENTA 5
#define VGA_COLOR_BROWN 6
#define VGA_COLOR_LIGHT_GREY 7
#define VGA_COLOR_DARK_GREY 8
#define VGA_COLOR_LIGHT_BLUE 9
#define VGA_COLOR_LIGHT_GREEN 10
#define VGA_COLOR_LIGHT_CYAN 11
#define VGA_COLOR_LIGHT_RED 12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_YELLOW 14
#define VGA_COLOR_WHITE 15

void vga_init();
void vga_clear_screen();
void vga_set_fg_color(uint8_t color);
void vga_set_bg_color(uint8_t color);
void vga_putchar(char c);
void vga_print(const char* str);
void vga_print_int(int num);
void vga_print_at(const char* str, int x, int y);
void vga_print_centered(const char* str, int y);
void vga_draw_pixel(int x, int y, uint8_t fg, uint8_t bg);
void vga_draw_rect(int x, int y, int w, int h, uint8_t fg, uint8_t bg);
void vga_render_image(uint8_t* raw_data, int width, int height, int x, int y);
void vga_scroll();

extern int cursor_x;
extern int cursor_y;

#endif