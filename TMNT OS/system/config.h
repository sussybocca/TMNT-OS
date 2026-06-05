#ifndef CONFIG_H
#define CONFIG_H

#include "types.h"

typedef struct {
    char wallpaper_path[256];
    char terminal_bg_path[256];
    uint8_t theme_color;
    uint8_t terminal_opacity;
    uint32_t boot_time;
    char username[32];
    char hostname[32];
} system_config_t;

extern system_config_t system_config;

void config_init();
void config_save();
void config_load();

#endif
