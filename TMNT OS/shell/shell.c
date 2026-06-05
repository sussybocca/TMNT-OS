// TMNT OS Command Shell
#include "shell.h"
#include "../drivers/vga/vga.h"
#include "../drivers/keyboard/keyboard.h"
#include "../fs/tmnt_fs.h"
#include "../system/image_injector.h"
#include "../system/memory.h"
#include "../system/string.h"

#define MAX_COMMANDS 128
#define MAX_CMD_LENGTH 256

typedef struct {
    char name[32];
    void (*function)(char* args);
    char description[128];
} command_t;

static command_t commands[MAX_COMMANDS];
static int command_count = 0;

// Forward declarations
void cmd_help(char* args);
void cmd_clear(char* args);
void cmd_ls(char* args);
void cmd_cd(char* args);
void cmd_cat(char* args);
void cmd_mkdir(char* args);
void cmd_rm(char* args);
void cmd_wallpaper(char* args);
void cmd_terminal_bg(char* args);
void cmd_scan_images(char* args);
void cmd_downloader(char* args);
void cmd_settings(char* args);
void cmd_cowabunga(char* args);
void cmd_theme(char* args);
void cmd_about(char* args);

void shell_init() {
    shell_register_command("help", cmd_help, "Show available commands");
    shell_register_command("clear", cmd_clear, "Clear the screen");
    shell_register_command("ls", cmd_ls, "List directory contents");
    shell_register_command("cd", cmd_cd, "Change directory");
    shell_register_command("cat", cmd_cat, "Display file contents");
    shell_register_command("mkdir", cmd_mkdir, "Create directory");
    shell_register_command("rm", cmd_rm, "Remove file");
    shell_register_command("wallpaper", cmd_wallpaper, "Set wallpaper image");
    shell_register_command("termbg", cmd_terminal_bg, "Set terminal background");
    shell_register_command("scan", cmd_scan_images, "Scan for new images");
    shell_register_command("download", cmd_downloader, "Open app downloader");
    shell_register_command("settings", cmd_settings, "Open settings");
    shell_register_command("cowabunga", cmd_cowabunga, "TMNT OS welcome");
    shell_register_command("theme", cmd_theme, "Change theme");
    shell_register_command("about", cmd_about, "About TMNT OS");
    
    vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
    vga_print("\n🐢 Shell ready! Type 'cowabunga' to start! 🐢\n\n");
}

void shell_register_command(const char* name, void (*func)(char*), const char* desc) {
    if(command_count < MAX_COMMANDS) {
        strcpy(commands[command_count].name, name);
        commands[command_count].function = func;
        strcpy(commands[command_count].description, desc);
        command_count++;
    }
}

void shell_process() {
    print_prompt();
    char* input = keyboard_readline();
    if(strlen(input) > 0) execute_command(input);
}

void print_prompt() {
    vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
    vga_print("🐢 ");
    vga_set_fg_color(VGA_COLOR_YELLOW);
    vga_print("tmnt@os");
    vga_set_fg_color(VGA_COLOR_WHITE);
    vga_print(":");
    vga_set_fg_color(VGA_COLOR_LIGHT_BLUE);
    vga_print("~");
    vga_set_fg_color(VGA_COLOR_WHITE);
    vga_print("$ ");
}

void execute_command(char* input) {
    char* command = strtok(input, " ");
    char* args = strtok(NULL, "");
    if(!command) return;
    for(int i = 0; i < command_count; i++) {
        if(strcmp(commands[i].name, command) == 0) {
            commands[i].function(args ? args : "");
            return;
        }
    }
    vga_set_fg_color(VGA_COLOR_LIGHT_RED);
    vga_print("Command not found: ");
    vga_print(command);
    vga_print("\nType 'help' for available commands.\n");
}

void cmd_help(char* args) {
    vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
    vga_print("\n🐢 TMNT OS Commands 🐢\n========================\n\n");
    for(int i = 0; i < command_count; i++) {
        vga_set_fg_color(VGA_COLOR_YELLOW);
        vga_print("  "); vga_print(commands[i].name);
        vga_set_fg_color(VGA_COLOR_WHITE);
        vga_print(" - "); vga_print(commands[i].description);
        vga_print("\n");
    }
    vga_print("\n");
}

void cmd_clear(char* args) { vga_clear_screen(); }

void cmd_ls(char* args) {
    const char* path = (args && strlen(args) > 0) ? args : "/";
    file_list_t* files = fs_list_directory(path);
    if(!files || files->count == 0) {
        vga_set_fg_color(VGA_COLOR_LIGHT_GREY);
        vga_print("(empty directory)\n");
        return;
    }
    vga_set_fg_color(VGA_COLOR_LIGHT_CYAN);
    vga_print("Contents of "); vga_print(path); vga_print(":\n");
    for(int i = 0; i < files->count; i++) {
        vga_set_fg_color(VGA_COLOR_WHITE);
        vga_print("  📄 "); vga_print(files->names[i]);
        vga_set_fg_color(VGA_COLOR_LIGHT_GREY);
        vga_print(" ("); vga_print_int(files->sizes[i]); vga_print(" bytes)\n");
    }
}

void cmd_cd(char* args) {
    if(!args || strlen(args) == 0) { vga_print("Usage: cd <path>\n"); return; }
    vga_print("Changed to: "); vga_print(args); vga_print("\n");
}

void cmd_cat(char* args) {
    if(!args || strlen(args) == 0) { vga_print("Usage: cat <file>\n"); return; }
    if(!fs_file_exists(args)) { vga_print("File not found\n"); return; }
    uint32_t sz = fs_get_file_size(args);
    if(sz == 0) { vga_print("(empty)\n"); return; }
    uint8_t* buf = (uint8_t*)malloc(sz + 1);
    if(!buf) return;
    fs_read_file(args, buf, sz);
    buf[sz] = '\0';
    vga_print((char*)buf);
    vga_print("\n");
    free(buf);
}

void cmd_cowabunga(char* args) {
    vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
    vga_print("\n  ╔══════════════════════════════════════╗\n");
    vga_print("  ║     🐢 COWABUNGA! 🐢               ║\n");
    vga_print("  ║    Welcome to TMNT OS!              ║\n");
    vga_print("  ║  Drop your images in:              ║\n");
    vga_print("  ║  /user/assets/                     ║\n");
    vga_print("  ║  - wallpapers/                     ║\n");
    vga_print("  ║  - terminal/                       ║\n");
    vga_print("  ║  - icons/                          ║\n");
    vga_print("  ║  - boot_logo/                      ║\n");
    vga_print("  ║  Type 'help' for commands          ║\n");
    vga_print("  ╚══════════════════════════════════════╝\n");
    vga_print("         🍕 Turtle Power! 🍕\n\n");
}

void cmd_wallpaper(char* args) {
    if(!args || strlen(args) == 0) {
        vga_print("Usage: wallpaper <image_path>\n");
        return;
    }
    inject_as_wallpaper(args);
}

void cmd_terminal_bg(char* args) {
    if(!args || strlen(args) == 0) { vga_print("Usage: termbg <image_path>\n"); return; }
    inject_as_terminal_bg(args);
}

void cmd_scan_images(char* args) { scan_for_user_images(); }

void cmd_downloader(char* args) {
    extern void downloader_open(void);
    downloader_open();
}

void cmd_settings(char* args) { vga_print("Settings coming soon!\n"); }

void cmd_theme(char* args) {
    vga_print("Available themes: green, purple, red, blue, orange\n");
}

void cmd_about(char* args) {
    vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
    vga_print("\nTMNT OS v1.0\nTeenage Mutant Ninja Turtles Operating System\nBuilt from scratch with Turtle Power!\n\n");
}

void cmd_mkdir(char* args) {
    if(!args) return;
    if(fs_create_dir(args) == -1) vga_print("Failed to create directory\n");
    else vga_print("Directory created\n");
}

void cmd_rm(char* args) {
    if(!args) return;
    if(fs_delete_file(args) == 0) vga_print("File removed\n");
    else vga_print("Failed to remove file\n");
}