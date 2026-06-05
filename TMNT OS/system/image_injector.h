#ifndef IMAGE_INJECTOR_H
#define IMAGE_INJECTOR_H

#include "types.h"
#include "../drivers/vga/vga.h"

#define MAX_IMAGES 256
#define MAX_PATH_LENGTH 256

// Image formats
#define IMAGE_FORMAT_PNG 0
#define IMAGE_FORMAT_JPEG 1
#define IMAGE_FORMAT_ICO 2
#define IMAGE_FORMAT_BMP 3
#define IMAGE_FORMAT_UNKNOWN 255

typedef struct {
    char path[MAX_PATH_LENGTH];
    uint8_t* raw_data;
    uint32_t size;
    int format;
    uint32_t width;
    uint32_t height;
    uint8_t injected;  // 1 if injected into system
} image_registry_t;

typedef struct {
    char filename[MAX_PATH_LENGTH];
    uint32_t size;
    uint8_t* data;
} image_file_t;

void image_injector_init();
void scan_for_user_images();
image_registry_t* load_image(const char* path);
int detect_image_format(uint8_t* data);
void inject_as_wallpaper(const char* path);
void inject_as_terminal_bg(const char* path);
void inject_as_icon(const char* path);
void inject_as_boot_logo(const char* path);
void apply_image_to_system(image_registry_t* img);
void render_image_to_vga(const char* path, int x, int y);

#endif
