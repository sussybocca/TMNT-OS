// TMNT OS Downloader - COMPLETE with Storage Browsing, Games, UI Selection
#include "downloader.h"
#include "../../drivers/vga/vga.h"
#include "../../drivers/keyboard/keyboard.h"
#include "../../fs/tmnt_fs.h"
#include "../../system/memory.h"
static void find_tmnt_files(const char* path, int* count);
void downloader_launch_menu(void);
#include "../../system/string.h"

static app_catalog_t* catalog = NULL;
static storage_device_t storage_devices[MAX_STORAGE_DEVICES];
static int device_count = 0;
static browse_entry_t browse_entries[MAX_BROWSE_FILES];
static int browse_count = 0;
static char current_browse_path[512] = "/";

// Recursive directory deletion
static void fs_remove_recursive(const char* path) {
    file_list_t* files = fs_list_directory(path);
    if(files) {
        for(int i = 0; i < files->count; i++) {
            char full_path[256];
            strcpy(full_path, path);
            strcat(full_path, "/");
            strcat(full_path, files->names[i]);
            
            file_list_t* sub = fs_list_directory(full_path);
            if(sub && sub->count > 0) {
                fs_remove_recursive(full_path);
            }
            fs_delete_file(full_path);
        }
    }
    fs_delete_file(path);
}

// Bare metal sprintf
static void sprintf(char* str, const char* format, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, format);
    
    while(*format) {
        if(*format == '%') {
            format++;
            switch(*format) {
                case 's': {
                    char* s = __builtin_va_arg(args, char*);
                    while(*s) *str++ = *s++;
                    break;
                }
                case 'd': {
                    int num = __builtin_va_arg(args, int);
                    if(num < 0) { *str++ = '-'; num = -num; }
                    char tmp[16];
                    int i = 0;
                    if(num == 0) tmp[i++] = '0';
                    while(num > 0) { tmp[i++] = '0' + (num % 10); num /= 10; }
                    while(i > 0) *str++ = tmp[--i];
                    break;
                }
                case 'c':
                    *str++ = (char)__builtin_va_arg(args, int);
                    break;
                case 'x': {
                    unsigned int num = __builtin_va_arg(args, unsigned int);
                    char hex[] = "0123456789ABCDEF";
                    char tmp[16];
                    int i = 0;
                    if(num == 0) tmp[i++] = '0';
                    while(num > 0) { tmp[i++] = hex[num & 0xF]; num >>= 4; }
                    while(i > 0) *str++ = tmp[--i];
                    break;
                }
                default:
                    *str++ = *format;
                    break;
            }
        } else {
            *str++ = *format;
        }
        format++;
    }
    *str = '\0';
    __builtin_va_end(args);
}

// Check if file has .tmnt extension
static int is_tmnt_file(const char* filename) {
    int len = strlen(filename);
    if(len < 6) return 0;
    return (filename[len-5] == '.' && 
            filename[len-4] == 't' && 
            filename[len-3] == 'm' && 
            filename[len-2] == 'n' && 
            filename[len-1] == 't');
}

void downloader_init() {
    catalog = (app_catalog_t*)malloc(sizeof(app_catalog_t));
    if(!catalog) return;
    memset(catalog, 0, sizeof(app_catalog_t));
    catalog->magic = 0x54415050;
    catalog->version = 2;  // Version 2 supports games + icons
    
    fs_create_dir("/system");
    fs_create_dir("/system/apps");
    fs_create_dir("/system/apps/downloader");
    fs_create_dir(APP_PACKAGE_PATH);
    fs_create_dir("/user");
    fs_create_dir("/user/storage");
    
    if(fs_file_exists("/system/apps/downloader/catalog.dat")) {
        uint32_t cat_size = fs_get_file_size("/system/apps/downloader/catalog.dat");
        if(cat_size >= sizeof(app_catalog_t)) {
            fs_read_file("/system/apps/downloader/catalog.dat", (uint8_t*)catalog, sizeof(app_catalog_t));
            if(catalog->magic != 0x54415050) {
                memset(catalog->entries, 0, sizeof(app_entry_t) * MAX_APPS);
                catalog->magic = 0x54415050;
                catalog->version = 2;
                catalog->app_count = 0;
            }
        }
    }
    
    // Scan for connected storage devices
    downloader_scan_devices();
}

void downloader_scan_devices() {
    device_count = 0;
    
    // Scan common storage locations
    const char* device_paths[] = {
        "/dev/sda1", "/dev/sdb1", "/dev/sdc1", "/dev/sdd1",
        "/dev/sr0", "/dev/fd0"
    };
    
    const char* mount_points[] = {
        "/mnt/hdd", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3",
        "/mnt/cdrom", "/mnt/floppy"
    };
    
    for(int i = 0; i < 6 && device_count < MAX_STORAGE_DEVICES; i++) {
        if(fs_file_exists(device_paths[i])) {
            storage_device_t* dev = &storage_devices[device_count];
            strcpy(dev->mount_point, mount_points[i]);
            
            if(i == 0) {
                strcpy(dev->name, "Internal HDD");
                dev->type = DEV_TYPE_HDD;
            } else if(i < 4) {
                sprintf(dev->name, "USB Drive %d", i);
                dev->type = DEV_TYPE_USB;
            } else if(i == 4) {
                strcpy(dev->name, "CD-ROM");
                dev->type = DEV_TYPE_CDROM;
            } else {
                strcpy(dev->name, "Floppy Disk");
                dev->type = DEV_TYPE_FLOPPY;
            }
            
            dev->connected = 1;
            dev->total_size = 0;
            dev->free_size = 0;
            device_count++;
        }
    }
    
    // Also check /user/storage for manually mounted devices
    file_list_t* user_storage = fs_list_directory("/user/storage");
    if(user_storage && user_storage->count > 0) {
        for(int i = 0; i < user_storage->count && device_count < MAX_STORAGE_DEVICES; i++) {
            storage_device_t* dev = &storage_devices[device_count];
            sprintf(dev->mount_point, "/user/storage/%s", user_storage->names[i]);
            sprintf(dev->name, "Storage: %s", user_storage->names[i]);
            dev->type = DEV_TYPE_USB;
            dev->connected = 1;
            device_count++;
        }
    }
}

void downloader_open() {
    vga_clear_screen();
    vga_set_bg_color(VGA_COLOR_GREEN);
    vga_set_fg_color(VGA_COLOR_BLACK);
    vga_draw_rect(0, 0, VGA_WIDTH, 3, VGA_COLOR_GREEN, VGA_COLOR_GREEN);
    vga_print_at("🐢 TMNT DOWNLOADER - Games, Apps & More! 🐢", 8, 1);
    vga_set_bg_color(VGA_COLOR_BLACK);
    vga_set_fg_color(VGA_COLOR_WHITE);
    
    while(1) {
        vga_clear_screen();
        vga_draw_rect(0, 0, VGA_WIDTH, 3, VGA_COLOR_GREEN, VGA_COLOR_GREEN);
        vga_print_at("🐢 TMNT DOWNLOADER - Games, Apps & More! 🐢", 8, 1);
        vga_set_bg_color(VGA_COLOR_BLACK);
        vga_set_fg_color(VGA_COLOR_WHITE);
        
        vga_print_at("════════════════════════ MAIN MENU ════════════════════════", 10, 5);
        vga_print_at("  [1] Browse App Catalog (", 10, 7);
        vga_print_int(catalog->app_count);
        vga_print(" available)");
        vga_print_at("  [2] Browse Storage Devices", 10, 8);
        vga_print_at("  [3] Import from Storage", 10, 9);
        vga_print_at("  [4] Launch Installed App", 10, 10);
        vga_print_at("  [5] Search Apps", 10, 11);
        vga_print_at("  [6] Refresh Devices", 10, 12);
        vga_print_at("  [0] Exit Downloader", 10, 13);
        vga_print_at("═══════════════════════════════════════════════════════════", 10, 15);
        vga_print_at("🐢 Select option: ", 10, 17);
        
        char choice = keyboard_getchar();
        
        switch(choice) {
            case '1':
                downloader_display_catalog();
                break;
            case '2':
                downloader_browse_storage();
                break;
            case '3':
                downloader_import_from_storage();
                break;
            case '4':
                downloader_launch_menu();
                break;
            case '5': {
                vga_print_at("\n🔍 Search: ", 10, 19);
                char* query = keyboard_readline();
                downloader_search(query);
                vga_print_at("Press any key...", 10, 21);
                keyboard_getchar();
                break;
            }
            case '6':
                downloader_scan_devices();
                vga_print_at("✅ Devices refreshed! Found ", 10, 19);
                vga_print_int(device_count);
                vga_print(" device(s)");
                vga_print_at("Press any key...", 10, 20);
                keyboard_getchar();
                break;
            case '0':
                return;
        }
    }
}

void downloader_browse_storage() {
    vga_clear_screen();
    vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
    vga_print("══════════════════════ STORAGE DEVICES ════════════════════\n\n");
    
    if(device_count == 0) {
        vga_set_fg_color(VGA_COLOR_YELLOW);
        vga_print("  No storage devices detected.\n");
        vga_print("  Connect a USB drive or insert media.\n");
        vga_print("  Files in /user/storage/ will also appear here.\n\n");
        vga_print("  Press any key to return...");
        keyboard_getchar();
        return;
    }
    
    for(int i = 0; i < device_count; i++) {
        storage_device_t* dev = &storage_devices[i];
        
        vga_set_fg_color(VGA_COLOR_YELLOW);
        vga_print("  [");
        vga_print_int(i + 1);
        vga_print("] ");
        
        switch(dev->type) {
            case DEV_TYPE_HDD: vga_print("💾 "); break;
            case DEV_TYPE_USB: vga_print("🔌 "); break;
            case DEV_TYPE_CDROM: vga_print("💿 "); break;
            case DEV_TYPE_FLOPPY: vga_print("💾 "); break;
        }
        
        vga_set_fg_color(VGA_COLOR_LIGHT_CYAN);
        vga_print(dev->name);
        vga_set_fg_color(VGA_COLOR_LIGHT_GREY);
        vga_print(" - ");
        vga_print(dev->mount_point);
        vga_print("\n");
    }
    
    vga_print("\n📁 Select device to browse (0 to return): ");
    int choice = keyboard_getint();
    
    if(choice > 0 && choice <= device_count) {
        strcpy(current_browse_path, storage_devices[choice - 1].mount_point);
        downloader_browse_directory(current_browse_path);
    }
}

void downloader_browse_directory(const char* path) {
    while(1) {
        vga_clear_screen();
        vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
        vga_print("═══════════════════ BROWSING ═══════════════════\n");
        vga_set_fg_color(VGA_COLOR_WHITE);
        vga_print("  📂 ");
        vga_print(path);
        vga_print("\n═════════════════════════════════════════════════\n\n");
        
        file_list_t* files = fs_list_directory(path);
        browse_count = 0;
        
        if(!files || files->count == 0) {
            vga_print("  (empty directory)\n");
        } else {
            // Show parent directory option
            if(strcmp(path, "/") != 0) {
                vga_set_fg_color(VGA_COLOR_YELLOW);
                vga_print("  [0] 📁 .. (parent directory)\n");
            }
            
            for(int i = 0; i < files->count && browse_count < MAX_BROWSE_FILES; i++) {
                char full_path[512];
                sprintf(full_path, "%s/%s", path, files->names[i]);
                
                // Check if directory
                file_list_t* sub = fs_list_directory(full_path);
                int is_dir = (sub && sub->count >= 0);
                
                browse_entry_t* entry = &browse_entries[browse_count];
                strcpy(entry->filename, files->names[i]);
                strcpy(entry->full_path, full_path);
                entry->size = files->sizes[i];
                entry->is_directory = is_dir;
                entry->is_tmnt_package = is_tmnt_file(files->names[i]);
                
                vga_set_fg_color(VGA_COLOR_YELLOW);
                vga_print("  [");
                vga_print_int(browse_count + 1);
                vga_print("] ");
                
                if(is_dir) {
                    vga_set_fg_color(VGA_COLOR_LIGHT_BLUE);
                    vga_print("📁 ");
                } else if(entry->is_tmnt_package) {
                    vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
                    vga_print("📦 ");
                } else {
                    vga_set_fg_color(VGA_COLOR_WHITE);
                    vga_print("📄 ");
                }
                
                vga_print(files->names[i]);
                
                if(!is_dir) {
                    vga_set_fg_color(VGA_COLOR_LIGHT_GREY);
                    vga_print(" (");
                    vga_print_int(files->sizes[i] / 1024);
                    vga_print(" KB)");
                }
                
                vga_print("\n");
                browse_count++;
            }
        }
        
        vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
        vga_print("\n═════════════════════════════════════════════════\n");
        vga_print("📥 [I]nstall .tmnt | [B]ack | [Q]uit | Select #: ");
        
        char* input = keyboard_readline();
        
        if(strlen(input) == 0) continue;
        
        if(input[0] == 'Q' || input[0] == 'q') {
            return;
        } else if(input[0] == 'B' || input[0] == 'b') {
            if(strcmp(path, "/") != 0) {
                // Go to parent
                char parent_path[512];
                strcpy(parent_path, path);
                char* last_slash = strrchr(parent_path, '/');
                if(last_slash) {
                    if(last_slash == parent_path) {
                        strcpy(current_browse_path, "/");
                    } else {
                        *last_slash = '\0';
                        strcpy(current_browse_path, parent_path);
                    }
                }
                downloader_browse_directory(current_browse_path);
                return;
            }
        } else if(input[0] == 'I' || input[0] == 'i') {
            // Install all .tmnt files in current directory
            for(int i = 0; i < browse_count; i++) {
                if(browse_entries[i].is_tmnt_package) {
                    downloader_add_package(browse_entries[i].full_path);
                }
            }
            vga_print("Press any key...");
            keyboard_getchar();
        } else {
            int num = str_to_int(input);
            if(num == 0 && strcmp(path, "/") != 0) {
                // Go to parent
                char parent_path[512];
                strcpy(parent_path, path);
                char* last_slash = strrchr(parent_path, '/');
                if(last_slash) {
                    if(last_slash == parent_path) {
                        strcpy(current_browse_path, "/");
                    } else {
                        *last_slash = '\0';
                        strcpy(current_browse_path, parent_path);
                    }
                }
                downloader_browse_directory(current_browse_path);
                return;
            } else if(num > 0 && num <= browse_count) {
                browse_entry_t* entry = &browse_entries[num - 1];
                if(entry->is_directory) {
                    strcpy(current_browse_path, entry->full_path);
                    downloader_browse_directory(current_browse_path);
                    return;
                } else if(entry->is_tmnt_package) {
                    downloader_add_package(entry->full_path);
                    vga_print("Press any key...");
                    keyboard_getchar();
                }
            }
        }
    }
}

void downloader_import_from_storage() {
    vga_clear_screen();
    vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
    vga_print("══════════════ IMPORT FROM STORAGE ═══════════════\n\n");
    
    // Scan all storage devices for .tmnt files
    int total_found = 0;
    
    for(int d = 0; d < device_count; d++) {
        vga_set_fg_color(VGA_COLOR_YELLOW);
        vga_print("Scanning ");
        vga_print(storage_devices[d].name);
        vga_print("...\n");
        
        // Recursively find .tmnt files
        find_tmnt_files(storage_devices[d].mount_point, &total_found);
    }
    
    // Also scan /user/assets
    vga_print("Scanning /user/assets...\n");
    find_tmnt_files("/user/assets", &total_found);
    
    if(total_found == 0) {
        vga_set_fg_color(VGA_COLOR_YELLOW);
        vga_print("\nNo .tmnt packages found on any device.\n");
        vga_print("Place .tmnt files on a USB drive or in /user/assets/\n");
    } else {
        vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
        vga_print("\n✅ Found and imported ");
        vga_print_int(total_found);
        vga_print(" package(s)!\n");
    }
    
    vga_print("\nPress any key to return...");
    keyboard_getchar();
}

// Recursively find .tmnt files
static void find_tmnt_files(const char* path, int* count) {
    file_list_t* files = fs_list_directory(path);
    if(!files) return;
    
    for(int i = 0; i < files->count; i++) {
        char full_path[512];
        sprintf(full_path, "%s/%s", path, files->names[i]);
        
        if(is_tmnt_file(files->names[i])) {
            vga_print("  📦 Found: ");
            vga_print(full_path);
            vga_print("\n");
            downloader_add_package(full_path);
            (*count)++;
        } else {
            // Check if directory
            file_list_t* sub = fs_list_directory(full_path);
            if(sub && sub->count >= 0) {
                find_tmnt_files(full_path, count);
            }
        }
    }
}

void downloader_launch_menu() {
    vga_clear_screen();
    vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
    vga_print("══════════════════ INSTALLED APPS ═══════════════════\n\n");
    
    int installed_count = 0;
    int installed_indices[MAX_APPS];
    
    for(uint32_t i = 0; i < catalog->app_count; i++) {
        if(catalog->entries[i].installed) {
            installed_indices[installed_count] = i;
            installed_count++;
            
            vga_set_fg_color(VGA_COLOR_YELLOW);
            vga_print("  [");
            vga_print_int(installed_count);
            vga_print("] ");
            
            app_entry_t* app = &catalog->entries[i];
            
            switch(app->category) {
                case 0: vga_print("🛠️ "); break;
                case 1: vga_print("🎮 "); break;
                case 2: vga_print("🎨 "); break;
                case 3: vga_print("🔧 "); break;
            }
            
            vga_set_fg_color(VGA_COLOR_LIGHT_CYAN);
            vga_print(app->name);
            vga_print(" v");
            vga_print(app->version);
            vga_print("\n");
        }
    }
    
    if(installed_count == 0) {
        vga_set_fg_color(VGA_COLOR_YELLOW);
        vga_print("  No apps installed yet. Install some first!\n\n");
        vga_print("  Press any key to return...");
        keyboard_getchar();
        return;
    }
    
    vga_print("\n🎮 Select app to launch (0 to cancel): ");
    int choice = keyboard_getint();
    
    if(choice > 0 && choice <= installed_count) {
        app_entry_t* app = &catalog->entries[installed_indices[choice - 1]];
        downloader_launch_app(app->name);
    }
}

void downloader_launch_app(const char* name) {
    app_entry_t* app = NULL;
    for(uint32_t i = 0; i < catalog->app_count; i++) {
        if(strcmp(catalog->entries[i].name, name) == 0) {
            app = &catalog->entries[i];
            break;
        }
    }
    
    if(!app || !app->installed) {
        vga_print("❌ App not installed\n");
        return;
    }
    
    vga_clear_screen();
    vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
    vga_print("🚀 Launching ");
    vga_print(app->name);
    vga_print("...\n\n");
    
    // Build path to app executable
    char launch_path[256];
    if(app->executable[0] != '\0') {
        sprintf(launch_path, "/apps/%s/%s", app->name, app->executable);
    } else {
        sprintf(launch_path, "/apps/%s/startup", app->name);
    }
    
    // Check if executable exists
    if(!fs_file_exists(launch_path)) {
        sprintf(launch_path, "/apps/%s/main", app->name);
        if(!fs_file_exists(launch_path)) {
            vga_set_fg_color(VGA_COLOR_LIGHT_RED);
            vga_print("❌ No executable found for this app\n");
            vga_print("App files are in /apps/");
            vga_print(app->name);
            vga_print("\nPress any key...");
            keyboard_getchar();
            return;
        }
    }
    
    // Read and execute app
    uint32_t app_size = fs_get_file_size(launch_path);
    if(app_size == 0) {
        vga_print("❌ Empty app file\n");
        keyboard_getchar();
        return;
    }
    
    uint8_t* app_code = (uint8_t*)malloc(app_size);
    fs_read_file(launch_path, app_code, app_size);
    
    // Execute app in its own context
    // For games and apps, we pass control to the app
    vga_print("═══════════════════════════════════════\n");
    vga_print("  Running: ");
    vga_print(app->name);
    vga_print("\n═══════════════════════════════════════\n\n");
    
    // Call app entry point
    typedef void (*app_main_t)(void);
    app_main_t app_main = (app_main_t)app_code;
    app_main();
    
    free(app_code);
    
    vga_print("\n\n📱 App finished. Press any key...");
    keyboard_getchar();
}

void downloader_display_catalog() {
    vga_clear_screen();
    vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
    vga_print("═══════════════════════════════════════════════════════════\n");
    vga_print("  🐢 TMNT Downloader - ");
    vga_print_int(catalog->app_count);
    vga_print(" Apps Available 🐢\n");
    vga_print("═══════════════════════════════════════════════════════════\n\n");
    
    if(catalog->app_count == 0) {
        vga_print("  No apps yet. Import from storage!\n");
        vga_print("  Use option 3 from main menu.\n");
    }
    
    for(uint32_t i = 0; i < catalog->app_count; i++) {
        app_entry_t* app = &catalog->entries[i];
        
        vga_set_fg_color(VGA_COLOR_YELLOW);
        vga_print("  [");
        vga_print_int(i + 1);
        vga_print("] ");
        
        switch(app->category) {
            case 0: vga_print("🛠️ "); break;
            case 1: vga_print("🎮 "); break;
            case 2: vga_print("🎨 "); break;
            case 3: vga_print("🔧 "); break;
        }
        
        vga_set_fg_color(VGA_COLOR_LIGHT_CYAN);
        vga_print(app->name);
        vga_print(" v");
        vga_print(app->version);
        
        if(app->installed) {
            vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
            vga_print(" [INSTALLED]");
        } else {
            vga_set_fg_color(VGA_COLOR_WHITE);
            vga_print(" [");
            vga_print_int(app->size / 1024);
            vga_print(" KB]");
        }
        vga_print("\n     ");
        vga_set_fg_color(VGA_COLOR_WHITE);
        vga_print(app->description);
        vga_print("\n\n");
    }
    
    vga_print("📥 [I]nstall | [U]ninstall | [L]aunch | [B]ack | Select #: ");
    
    char* input = keyboard_readline();
    if(strlen(input) == 0) return;
    
    if(input[0] == 'B' || input[0] == 'b') return;
    
    if(input[0] == 'I' || input[0] == 'i') {
        vga_print("Install which app? ");
        char* name = keyboard_readline();
        downloader_install_app(name);
        keyboard_getchar();
    } else if(input[0] == 'U' || input[0] == 'u') {
        vga_print("Uninstall which app? ");
        char* name = keyboard_readline();
        downloader_uninstall_app(name);
        keyboard_getchar();
    } else if(input[0] == 'L' || input[0] == 'l') {
        vga_print("Launch which app? ");
        char* name = keyboard_readline();
        downloader_launch_app(name);
    } else {
        int num = str_to_int(input);
        if(num > 0 && num <= (int)catalog->app_count) {
            app_entry_t* app = &catalog->entries[num - 1];
            if(app->installed) {
                downloader_launch_app(app->name);
            } else {
                downloader_install_app(app->name);
                keyboard_getchar();
            }
        }
    }
}

void downloader_install_app(const char* name) {
    app_entry_t* app = NULL;
    for(uint32_t i = 0; i < catalog->app_count; i++) {
        if(strcmp(catalog->entries[i].name, name) == 0) {
            app = &catalog->entries[i];
            break;
        }
    }
    
    if(!app) {
        vga_set_fg_color(VGA_COLOR_LIGHT_RED);
        vga_print("❌ App not found\n");
        return;
    }
    
    if(app->installed) {
        vga_set_fg_color(VGA_COLOR_YELLOW);
        vga_print("⚠️  Already installed - use Launch instead!\n");
        return;
    }
    
    vga_set_fg_color(VGA_COLOR_LIGHT_CYAN);
    vga_print("📦 Installing ");
    vga_print(app->name);
    vga_print("...\n");
    
    char pkg_path[256];
    sprintf(pkg_path, "%s/%s.tmnt", APP_PACKAGE_PATH, app->name);
    
    uint32_t pkg_size = fs_get_file_size(pkg_path);
    if(pkg_size == 0) {
        vga_print("❌ Package missing\n");
        return;
    }
    
    uint8_t* pkg_data = (uint8_t*)malloc(pkg_size);
    if(!pkg_data) { vga_print("❌ Out of memory\n"); return; }
    
    fs_read_file(pkg_path, pkg_data, pkg_size);
    
    if(*(uint32_t*)pkg_data != 0x544D4E54) {
        vga_print("❌ Invalid package\n");
        free(pkg_data);
        return;
    }
    
    uint32_t file_count = *(uint32_t*)(pkg_data + 4);
    uint32_t offset = 8;
    
    // Skip metadata (name, description, version, category)
    offset += 1 + pkg_data[offset];  // name
    offset += 1 + pkg_data[offset];  // description
    offset += 1 + pkg_data[offset];  // version
    
    // Read executable path if version 2
    if(catalog->version >= 2) {
        uint8_t exe_len = pkg_data[offset++];
        memcpy(app->executable, pkg_data + offset, exe_len);
        app->executable[exe_len] = '\0';
        offset += exe_len;
    }
    
    offset++;  // category
    
    char app_dir[256];
    sprintf(app_dir, "/apps/%s", app->name);
    fs_create_dir(app_dir);
    
    for(uint32_t i = 0; i < file_count && offset < pkg_size; i++) {
        uint8_t name_len = pkg_data[offset++];
        char fname[64];
        memcpy(fname, pkg_data + offset, name_len);
        fname[name_len] = '\0';
        offset += name_len;
        
        uint32_t fsize = *(uint32_t*)(pkg_data + offset);
        offset += 4;
        
        char full_path[256];
        sprintf(full_path, "%s/%s", app_dir, fname);
        
        // Create subdirectories
        char* slash = strrchr(fname, '/');
        if(slash) {
            *slash = '\0';
            char dir_path[256];
            sprintf(dir_path, "%s/%s", app_dir, fname);
            char build_path[256];
            strcpy(build_path, app_dir);
            char* token = strtok(fname, "/");
            while(token) {
                strcat(build_path, "/");
                strcat(build_path, token);
                fs_create_dir(build_path);
                token = strtok(NULL, "/");
            }
        }
        
        fs_write_file(full_path, pkg_data + offset, fsize);
        vga_print("   ✓ ");
        vga_print(fname);
        vga_print(" (");
        vga_print_int(fsize);
        vga_print(" bytes)\n");
        offset += fsize;
    }
    
    free(pkg_data);
    app->installed = 1;
    app->downloads++;
    fs_write_file("/system/apps/downloader/catalog.dat", (uint8_t*)catalog, sizeof(app_catalog_t));
    
    vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
    vga_print("✅ Installed! Use Launch to run it!\n");
}

void downloader_uninstall_app(const char* name) {
    app_entry_t* app = NULL;
    for(uint32_t i = 0; i < catalog->app_count; i++) {
        if(strcmp(catalog->entries[i].name, name) == 0) {
            app = &catalog->entries[i];
            break;
        }
    }
    
    if(!app || !app->installed) {
        vga_print("❌ Not installed\n");
        return;
    }
    
    vga_print("🗑️  Uninstalling ");
    vga_print(app->name);
    vga_print("...\n");
    
    char app_dir[256];
    sprintf(app_dir, "/apps/%s", app->name);
    fs_remove_recursive(app_dir);
    
    app->installed = 0;
    fs_write_file("/system/apps/downloader/catalog.dat", (uint8_t*)catalog, sizeof(app_catalog_t));
    
    vga_print("✅ Uninstalled\n");
}

void downloader_search(const char* query) {
    vga_clear_screen();
    vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
    vga_print("🔍 Search: ");
    vga_print(query);
    vga_print("\n═══════════════════════════════════════\n\n");
    
    int found = 0;
    for(uint32_t i = 0; i < catalog->app_count; i++) {
        int name_len = strlen(catalog->entries[i].name);
        int desc_len = strlen(catalog->entries[i].description);
        int query_len = strlen(query);
        int match = 0;
        
        for(int j = 0; j <= name_len - query_len && !match; j++) {
            int k;
            for(k = 0; k < query_len; k++) {
                char c1 = catalog->entries[i].name[j + k];
                char c2 = query[k];
                if(c1 >= 'A' && c1 <= 'Z') c1 += 32;
                if(c2 >= 'A' && c2 <= 'Z') c2 += 32;
                if(c1 != c2) break;
            }
            if(k == query_len) match = 1;
        }
        
        for(int j = 0; j <= desc_len - query_len && !match; j++) {
            int k;
            for(k = 0; k < query_len; k++) {
                char c1 = catalog->entries[i].description[j + k];
                char c2 = query[k];
                if(c1 >= 'A' && c1 <= 'Z') c1 += 32;
                if(c2 >= 'A' && c2 <= 'Z') c2 += 32;
                if(c1 != c2) break;
            }
            if(k == query_len) match = 1;
        }
        
        if(match) {
            found++;
            vga_set_fg_color(VGA_COLOR_LIGHT_CYAN);
            vga_print("  📦 ");
            vga_print(catalog->entries[i].name);
            if(catalog->entries[i].installed) {
                vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
                vga_print(" [INSTALLED]");
            }
            vga_print("\n     ");
            vga_set_fg_color(VGA_COLOR_WHITE);
            vga_print(catalog->entries[i].description);
            vga_print("\n\n");
        }
    }
    
    if(!found) vga_print("  No results found.\n");
}

void downloader_show_info(const char* name) {
    app_entry_t* app = NULL;
    for(uint32_t i = 0; i < catalog->app_count; i++) {
        if(strcmp(catalog->entries[i].name, name) == 0) {
            app = &catalog->entries[i];
            break;
        }
    }
    
    if(!app) { vga_print("App not found\n"); return; }
    
    vga_clear_screen();
    vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
    vga_print("═══════════════════════════════════════\n  📦 ");
    vga_print(app->name);
    vga_print("\n═══════════════════════════════════════\n\n");
    vga_set_fg_color(VGA_COLOR_WHITE);
    vga_print("  Version:    "); vga_print(app->version);
    vga_print("\n  Size:       "); vga_print_int(app->size / 1024); vga_print(" KB");
    vga_print("\n  Category:   ");
    switch(app->category) {
        case 0: vga_print("🛠️ Utility"); break;
        case 1: vga_print("🎮 Game"); break;
        case 2: vga_print("🎨 Theme"); break;
        case 3: vga_print("🔧 Tool"); break;
    }
    vga_print("\n  Downloads:  "); vga_print_int(app->downloads);
    vga_print("\n  Status:     ");
    if(app->installed) {
        vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
        vga_print("✓ Installed - Ready to Launch!");
    } else {
        vga_set_fg_color(VGA_COLOR_YELLOW);
        vga_print("○ Not installed");
    }
    vga_set_fg_color(VGA_COLOR_WHITE);
    vga_print("\n  Executable: "); vga_print(app->executable[0] ? app->executable : "startup");
    vga_print("\n  Description:\n  "); vga_print(app->description);
    vga_print("\n");
}

void downloader_add_package(const char* path) {
    if(!fs_file_exists(path)) {
        vga_print("❌ File not found: "); vga_print(path); vga_print("\n");
        return;
    }
    
    uint32_t file_size = fs_get_file_size(path);
    if(file_size < 8) { vga_print("❌ Package too small\n"); return; }
    
    uint8_t* pkg_data = (uint8_t*)malloc(file_size);
    if(!pkg_data) { vga_print("❌ Out of memory\n"); return; }
    
    fs_read_file(path, pkg_data, file_size);
    
    if(*(uint32_t*)pkg_data != 0x544D4E54) {
        vga_print("❌ Invalid .tmnt package\n");
        free(pkg_data);
        return;
    }
    
    if(catalog->app_count >= MAX_APPS) {
        vga_print("❌ Catalog full\n");
        free(pkg_data);
        return;
    }
    
    uint32_t offset = 8;
    uint8_t name_len = pkg_data[offset++];
    
    app_entry_t* app = &catalog->entries[catalog->app_count];
    memset(app, 0, sizeof(app_entry_t));
    
    memcpy(app->name, pkg_data + offset, name_len);
    app->name[name_len] = '\0';
    offset += name_len;
    
    uint8_t desc_len = pkg_data[offset++];
    memcpy(app->description, pkg_data + offset, desc_len);
    app->description[desc_len] = '\0';
    offset += desc_len;
    
    uint8_t ver_len = pkg_data[offset++];
    memcpy(app->version, pkg_data + offset, ver_len);
    app->version[ver_len] = '\0';
    offset += ver_len;
    
    // Read executable path for version 2 packages
    if(catalog->version >= 2 && offset < file_size) {
        uint8_t exe_len = pkg_data[offset++];
        if(exe_len > 0 && exe_len < 64) {
            memcpy(app->executable, pkg_data + offset, exe_len);
            app->executable[exe_len] = '\0';
            offset += exe_len;
        }
    }
    
    app->category = pkg_data[offset++];
    app->size = file_size;
    app->package_size = file_size;
    app->installed = 0;
    app->downloads = 0;
    
    char dest_path[256];
    sprintf(dest_path, "%s/%s.tmnt", APP_PACKAGE_PATH, app->name);
    fs_write_file(dest_path, pkg_data, file_size);
    
    free(pkg_data);
    catalog->app_count++;
    fs_write_file("/system/apps/downloader/catalog.dat", (uint8_t*)catalog, sizeof(app_catalog_t));
    
    vga_print("✅ Added '"); vga_print(app->name); vga_print("' (");
    vga_print_int(catalog->app_count); vga_print(" total)\n");
}

void downloader_remove_package(const char* name) {
    app_entry_t* app = NULL;
    int app_index = -1;
    
    for(uint32_t i = 0; i < catalog->app_count; i++) {
        if(strcmp(catalog->entries[i].name, name) == 0) {
            app = &catalog->entries[i];
            app_index = i;
            break;
        }
    }
    
    if(!app) { vga_print("App not found\n"); return; }
    if(app->installed) { vga_print("Uninstall first\n"); return; }
    
    // Remove package file
    char pkg_path[256];
    sprintf(pkg_path, "%s/%s.tmnt", APP_PACKAGE_PATH, app->name);
    fs_delete_file(pkg_path);
    
    // Remove from catalog by shifting entries
    for(int i = app_index; i < (int)catalog->app_count - 1; i++) {
        memcpy(&catalog->entries[i], &catalog->entries[i + 1], sizeof(app_entry_t));
    }
    catalog->app_count--;
    
    fs_write_file("/system/apps/downloader/catalog.dat", (uint8_t*)catalog, sizeof(app_catalog_t));
    
    vga_print("✅ Removed from store\n");
}