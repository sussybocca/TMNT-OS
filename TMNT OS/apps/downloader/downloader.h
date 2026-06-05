#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include "../../system/types.h"

#define MAX_APPS 128
#define APP_NAME_MAX 64
#define APP_DESC_MAX 256
#define APP_PACKAGE_PATH "/system/apps/downloader/packages"
#define MAX_STORAGE_DEVICES 8
#define MAX_BROWSE_FILES 512

// Storage device types
#define DEV_TYPE_HDD 0
#define DEV_TYPE_USB 1
#define DEV_TYPE_CDROM 2
#define DEV_TYPE_FLOPPY 3

typedef struct {
    char name[32];
    char mount_point[64];
    uint8_t type;
    uint32_t total_size;
    uint32_t free_size;
    uint8_t connected;
} storage_device_t;

typedef struct {
    char name[APP_NAME_MAX];
    char description[APP_DESC_MAX];
    char version[16];
    uint32_t size;
    uint32_t downloads;
    uint8_t category;  // 0=Utility, 1=Game, 2=Theme, 3=Tool
    uint8_t installed;  // 0=not installed, 1=installed
    uint8_t has_icon;   // 0=no icon, 1=has icon
    uint32_t icon_offset;
    uint32_t icon_size;
    uint32_t package_offset;
    uint32_t package_size;
    char executable[64];  // Path to launch file inside app
} app_entry_t;

typedef struct {
    uint32_t magic;  // 0x54415050 = "TAPP"
    uint32_t version;
    uint32_t app_count;
    uint32_t total_size;
    app_entry_t entries[MAX_APPS];
} app_catalog_t;

typedef struct {
    char filename[256];
    char full_path[512];
    uint32_t size;
    uint8_t is_directory;
    uint8_t is_tmnt_package;  // 1 if .tmnt file
} browse_entry_t;

void downloader_init();
void downloader_open();
void downloader_display_catalog();
void downloader_install_app(const char* name);
void downloader_uninstall_app(const char* name);
void downloader_search(const char* query);
void downloader_add_package(const char* path);
void downloader_remove_package(const char* name);
void downloader_launch_app(const char* name);
void downloader_browse_storage();
void downloader_scan_devices();
void downloader_browse_directory(const char* path);
void downloader_import_from_storage();

#endif