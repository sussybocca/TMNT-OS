#include "drivers/vga/vga.h"
#include "drivers/sb16.h"
#include "drivers/pit.h"
#include "system/string.h"
#include "system/memory.h"
#include "fs/tmnt_fs.h"
#include "../external/gui.h"
#include "../external/input.h"
#include "../external/terminal.h"

extern void file_manager_open(void);
extern void text_editor_open(void);
extern void runner_game_open(void);
extern void paint_studio_open(void);

// TMNT Bot AI Background Service
extern void tmnt_bot_svc_init(void);
extern void tmnt_bot_svc_learn_input(const char* input);
extern void tmnt_bot_svc_learn_output(const char* output);

typedef struct {
    uint32_t flags; uint32_t mem_lower; uint32_t mem_upper;
    uint32_t boot_device; uint32_t cmdline; uint32_t mods_count;
    uint32_t mods_addr; uint32_t syms[4]; uint32_t mmap_length;
    uint32_t mmap_addr; uint32_t drives_length; uint32_t drives_addr;
    uint32_t config_table; uint32_t boot_loader_name; uint32_t apm_table;
    uint32_t vbe_control_info; uint32_t vbe_mode_info;
    uint16_t vbe_mode; uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off; uint16_t vbe_interface_len;
    uint64_t framebuffer_addr; uint32_t framebuffer_pitch;
    uint32_t framebuffer_width; uint32_t framebuffer_height;
    uint8_t framebuffer_bpp; uint8_t framebuffer_type;
} __attribute__((packed)) multiboot_info_t;

uint32_t* fb = NULL;
uint32_t fb_width = 0, fb_height = 0, fb_pitch = 0;

static inline uint8_t inb(uint16_t port) { uint8_t r; asm volatile("inb %1,%0":"=a"(r):"Nd"(port)); return r; }
static inline void outb(uint16_t port, uint8_t v) { asm volatile("outb %0,%1"::"a"(v),"Nd"(port)); }

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    (void)magic;
    outb(0x64,0x60); outb(0x60,0x47);
    memory_init(); fs_init(); vga_init();
    pit_init();
    sb16_init();

    // Initialize TMNT Bot AI Background Service (watches all user input/output)
    tmnt_bot_svc_init();

    int grub_terminal=0, grub_safe=0, grub_recovery=0;
    if(mbi && (mbi->flags&(1<<2)) && mbi->cmdline) {
        char* cmdline = (char*)((unsigned int)mbi->cmdline);
        if(cmdline[0]=='t'&&cmdline[1]=='e'&&cmdline[2]=='r'&&cmdline[3]=='m') grub_terminal=1;
        else if(cmdline[0]=='s'&&cmdline[1]=='a'&&cmdline[2]=='f'&&cmdline[3]=='e') grub_safe=1;
        else if(cmdline[0]=='r'&&cmdline[1]=='e'&&cmdline[2]=='c') grub_recovery=1;
    }

    if(mbi && (mbi->flags&(1<<12)) && mbi->framebuffer_type==1 && !grub_recovery) {
        fb = (uint32_t*)((unsigned int)mbi->framebuffer_addr);
        fb_width  = mbi->framebuffer_width;
        fb_height = mbi->framebuffer_height;
        fb_pitch  = mbi->framebuffer_pitch;
        mouse_init();
        gui_init();

        if(grub_terminal) {
            terminal_mode=1; gui_mode=0;
            term_fb_clear(); term_fg=0x00FF00; term_bg=0x000000;
            term_fb_print("\n🐢 TERMINAL MODE 🐢\n===================\nTMNT OS Deep Customization Terminal\nType 'turtle' for TM commands\nType 'exit' to return\n\n");
        } else if(grub_safe) {
            gui_mode=1; terminal_mode=0;
            gui_draw_desktop();
            gui_save_cursor_bg(); gui_draw_cursor();
        } else {
            gui_mode=1; terminal_mode=0;
            gui_draw_desktop();
            gui_save_cursor_bg(); gui_draw_cursor();
        }
    } else if(grub_recovery) {
        gui_mode=0; terminal_mode=0;
        if(fb) {
            term_fb_clear(); term_fg=0xFF4444; term_bg=0x000000;
            term_fb_print("\n🐢 RECOVERY MODE 🐢\n==================\nSystem recovery shell\nType 'help' for commands\n\n");
        } else {
            vga_clear_screen(); vga_set_bg_color(VGA_COLOR_BLACK);
            vga_set_fg_color(VGA_COLOR_LIGHT_RED);
            vga_print("\n🐢 RECOVERY MODE 🐢\nType 'help'\n\n");
        }
    }

    vga_set_bg_color(VGA_COLOR_GREEN); vga_set_fg_color(VGA_COLOR_WHITE);
    vga_print_centered("TMNT OS v1.0 - Shell Shock!",10);
    vga_set_fg_color(VGA_COLOR_YELLOW);
    vga_print_centered("Select mode with mouse or type 'term'/'gui'",11);

    while(1) {
        if(terminal_mode) {
            if(fb) term_fb_print("🐢 tmnt-shell> ");
            else { vga_set_fg_color(VGA_COLOR_LIGHT_GREEN); vga_print("🐢 tmnt-shell> "); vga_set_fg_color(VGA_COLOR_WHITE); }
            char* input = keyboard_readline_poll();
            if(input[0]=='e'&&input[1]=='x'&&input[2]=='i'&&input[3]=='t'&&input[4]=='\0') {
                // Learn exit command
                tmnt_bot_svc_learn_input(input);
                terminal_mode=0; gui_mode=1;
                if(fb) { gui_draw_desktop(); gui_save_cursor_bg(); gui_draw_cursor(); }
                tmnt_bot_svc_learn_output("Returned to desktop");
                vga_print("\n🐢 Desktop!\n");
            } else {
                // Learn terminal command and its result
                tmnt_bot_svc_learn_input(input);
                tm_shell_command(input);
                tmnt_bot_svc_learn_output("Command executed in terminal");
            }
        } else {
            vga_set_fg_color(VGA_COLOR_LIGHT_GREEN);
            vga_print("\n🐢 tmnt@os:~$ ");
            vga_set_fg_color(VGA_COLOR_WHITE);
            char* input = keyboard_readline_poll();

            // Send input to AI background service for learning
            tmnt_bot_svc_learn_input(input);

            if(input[0]=='t'&&input[1]=='e'&&input[2]=='r'&&input[3]=='m'&&input[4]=='\0') {
                terminal_mode=1; gui_mode=0;
                if(fb) { term_fb_clear(); term_fg=0x00FF00; term_bg=0x000000; term_fb_print("🐢 TERMINAL MODE - Type 'exit' to return\n"); }
                else   { vga_clear_screen(); vga_print("🐢 TERMINAL MODE\n"); }
                tmnt_bot_svc_learn_output("Switched to terminal mode");
            } else if(input[0]=='f'&&input[1]=='i'&&input[2]=='l'&&input[3]=='e'&&input[4]=='s'&&input[5]=='\0') {
                vga_clear_screen(); vga_set_bg_color(VGA_COLOR_BLACK); vga_set_fg_color(VGA_COLOR_WHITE);
                file_manager_open();
                if(fb) { gui_draw_desktop(); gui_save_cursor_bg(); gui_draw_cursor(); }
                tmnt_bot_svc_learn_output("Opened file manager");
            } else if(input[0]=='e'&&input[1]=='d'&&input[2]=='i'&&input[3]=='t'&&input[4]=='\0') {
                vga_clear_screen(); vga_set_bg_color(VGA_COLOR_BLACK); vga_set_fg_color(VGA_COLOR_WHITE);
                text_editor_open();
                if(fb) { gui_draw_desktop(); gui_save_cursor_bg(); gui_draw_cursor(); }
                tmnt_bot_svc_learn_output("Opened text editor");
            } else if(input[0]=='h'&&input[1]=='e'&&input[2]=='l'&&input[3]=='p'&&input[4]=='\0') {
                vga_print("Commands: help,clear,cowabunga,ls,mkdir,rm,run,edit,cat,term,gui,files,edit\n");
                tmnt_bot_svc_learn_output("Displayed help menu with available commands");
            } else if(input[0]=='g'&&input[1]=='u'&&input[2]=='i'&&input[3]=='\0') {
                if(fb) { gui_draw_desktop(); gui_save_cursor_bg(); gui_draw_cursor(); vga_print("GUI!\n"); }
                tmnt_bot_svc_learn_output("Switched to GUI desktop mode");
            } else if(input[0]=='c'&&input[1]=='l'&&input[2]=='e'&&input[3]=='a'&&input[4]=='r'&&input[5]=='\0') {
                vga_clear_screen();
                tmnt_bot_svc_learn_output("Cleared screen");
            } else if(input[0]=='c'&&input[1]=='o'&&input[2]=='w'&&input[3]=='a'&&input[4]=='b'&&input[5]=='u'&&input[6]=='n'&&input[7]=='g'&&input[8]=='a'&&input[9]=='\0') {
                vga_print("🐢 COWABUNGA! 🍕\n");
                tmnt_bot_svc_learn_output("COWABUNGA! Turtle power activated!");
            } else if(input[0]=='l'&&input[1]=='s'&&input[2]=='\0') {
                file_list_t* files = fs_list_directory("/");
                if(files && files->count>0) for(int i=0;i<files->count;i++){vga_print("  ");vga_print(files->names[i]);vga_print("\n");}
                else vga_print("(empty)\n");
                tmnt_bot_svc_learn_output("Listed directory contents");
            } else if(input[0]=='c'&&input[1]=='a'&&input[2]=='t'&&input[3]==' '){
                char* path=input+4;
                if(fs_file_exists(path)){uint32_t sz=fs_get_file_size(path);if(sz>0&&sz<4096){uint8_t* buf=(uint8_t*)malloc(sz+1);fs_read_file(path,buf,sz);buf[sz]='\0';vga_print((char*)buf);vga_print("\n");free(buf);}}
                else vga_print("File not found\n");
                tmnt_bot_svc_learn_output("Displayed file contents");
            } else if(input[0]=='e'&&input[1]=='d'&&input[2]=='i'&&input[3]=='t'&&input[4]==' '){
                char* path=input+5; vga_print("Editing: ");vga_print(path);vga_print("\n");
                char* content=keyboard_read_multiline();
                fs_write_file(path,(uint8_t*)content,strlen(content)); vga_print("Saved\n");
                tmnt_bot_svc_learn_output("File edited and saved");
            } else if(input[0]=='m'&&input[1]=='k'&&input[2]=='d'&&input[3]=='i'&&input[4]=='r'&&input[5]==' '){
                fs_create_dir(input+6); vga_print("Directory created\n");
                tmnt_bot_svc_learn_output("Created new directory");
            } else if(input[0]=='r'&&input[1]=='m'&&input[2]==' '){
                fs_delete_file(input+3); vga_print("File removed\n");
                tmnt_bot_svc_learn_output("Deleted file");
            } else if(input[0]=='r'&&input[1]=='u'&&input[2]=='n'&&input[3]==' '){
                char* path=input+4;
                if(fs_file_exists(path)){uint32_t sz=fs_get_file_size(path);if(sz>0&&sz<65536){uint8_t* code=(uint8_t*)malloc(sz);fs_read_file(path,code,sz);typedef void(*f)(void);f func=(f)code;func();free(code);}}
                else vga_print("File not found\n");
                tmnt_bot_svc_learn_output("Executed program");
            } else if(strlen(input)>0) {
                vga_print("Unknown command\n");
                tmnt_bot_svc_learn_output("Unknown command - not recognized");
            }
        }
    }
}