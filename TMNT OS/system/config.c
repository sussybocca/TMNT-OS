#include "config.h"
#include "../fs/tmnt_fs.h"
#include "string.h"

system_config_t system_config;

void config_init() {
    strcpy(system_config.wallpaper_path, "/system/assets/wallpapers/default.tmnt");
    strcpy(system_config.terminal_bg_path, "/system/assets/terminal/default.tmnt");
    system_config.theme_color = 2;
    system_config.terminal_opacity = 80;
    system_config.boot_time = 0;
    strcpy(system_config.username, "turtle");
    strcpy(system_config.hostname, "tmnt-os");
    
    config_load();
}

void config_save() {
    fs_write_file("/system/config/system.cfg", 
                  (uint8_t*)&system_config, 
                  sizeof(system_config_t));
}

void config_load() {
    if(fs_file_exists("/system/config/system.cfg")) {
        fs_read_file("/system/config/system.cfg", 
                     (uint8_t*)&system_config, 
                     sizeof(system_config_t));
    }
}