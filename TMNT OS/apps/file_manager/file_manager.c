#include "file_manager.h"
#include "../../fs/tmnt_fs.h"
#include "../../system/memory.h"
#include "../../system/string.h"

extern char keyboard_getchar_poll(void);
extern char* keyboard_readline_poll(void);
extern int keyboard_getint_poll(void);
extern void term_fb_print(const char* str);
extern void term_fb_putchar(char c);
extern void term_fb_clear(void);
extern uint32_t term_fg, term_bg;

static char current_dir[128];

void file_manager_init(void) {
    strcpy(current_dir, "/");
}

void file_manager_open(void) {
    term_fb_clear();
    term_fg = 0x00FF00; term_bg = 0x000000;
    
    while(1) {
        term_fb_clear();
        term_fb_print("🐢 TMNT FILE MANAGER 🐢\n");
        term_fb_print("=======================\n");
        term_fb_print("📂 ");
        term_fb_print(current_dir);
        term_fb_print("\n\n");
        
        file_list_t* files = fs_list_directory(current_dir);
        if(files && files->count > 0) {
            for(int i = 0; i < files->count; i++) {
                term_fb_print("  [");
                char num[4];
                int_to_str(i + 1, num);
                term_fb_print(num);
                term_fb_print("] ");
                
                char full_path[256];
                strcpy(full_path, current_dir);
                if(strcmp(current_dir, "/") != 0) strcat(full_path, "/");
                strcat(full_path, files->names[i]);
                
                file_list_t* sub = fs_list_directory(full_path);
                if(sub && sub->count >= 0) {
                    term_fb_print("📁 ");
                } else {
                    term_fb_print("📄 ");
                }
                term_fb_print(files->names[i]);
                term_fb_print(" (");
                char size_str[16];
                int_to_str(files->sizes[i], size_str);
                term_fb_print(size_str);
                term_fb_print(" bytes)\n");
            }
        } else {
            term_fb_print("  (empty directory)\n");
        }
        
        term_fb_print("\n[O]pen [B]ack [Q]uit\n");
        term_fb_print("> ");
        
        char choice = keyboard_getchar_poll();
        term_fb_putchar(choice);
        term_fb_putchar('\n');
        
        switch(choice) {
            case 'O': case 'o': {
                term_fb_print("Select number: ");
                int num = keyboard_getint_poll();
                if(num > 0 && files && num <= files->count) {
                    char full_path[256];
                    strcpy(full_path, current_dir);
                    if(strcmp(current_dir, "/") != 0) strcat(full_path, "/");
                    strcat(full_path, files->names[num - 1]);
                    
                    file_list_t* sub = fs_list_directory(full_path);
                    if(sub && sub->count >= 0) {
                        strcpy(current_dir, full_path);
                    }
                }
                break;
            }
            case 'B': case 'b': {
                if(strcmp(current_dir, "/") != 0) {
                    char* last = strrchr(current_dir, '/');
                    if(last == current_dir) {
                        strcpy(current_dir, "/");
                    } else if(last) {
                        *last = '\0';
                    }
                }
                break;
            }
            case 'Q': case 'q':
                return;
        }
    }
}
