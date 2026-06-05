#include "text_editor.h"
#include "../../fs/tmnt_fs.h"
#include "../../system/memory.h"
#include "../../system/string.h"

extern char keyboard_getchar_poll(void);
extern char* keyboard_readline_poll(void);
extern char* keyboard_read_multiline(void);
extern void term_fb_print(const char* str);
extern void term_fb_putchar(char c);
extern void term_fb_clear(void);
extern uint32_t term_fg, term_bg;

static char editor_buffer[4096];
static char current_file[64];
static int editor_lines = 0;

void text_editor_init(void) {
    fs_create_dir("/user");
    fs_create_dir("/user/documents");
    fs_create_dir("/user/projects");
    memset(editor_buffer, 0, 4096);
    memset(current_file, 0, 64);
    editor_lines = 0;
}

void text_editor_open(void) {
    term_fb_clear();
    term_fg = 0x00FF00; term_bg = 0x000000;
    
    while(1) {
        term_fb_clear();
        term_fg = 0x00FF00; term_fb_print("🐢 TMNT TEXT EDITOR 🐢\n");
        term_fg = 0x00FF00; term_fb_print("=======================\n");
        
        if(current_file[0] != '\0') {
            term_fg = 0xFFFFFF; term_fb_print("📝 Editing: ");
            term_fg = 0xFFFF00; term_fb_print(current_file);
            term_fb_print("\n");
            term_fg = 0x00AA00; term_fb_print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
            
            // Show line numbers and content
            if(editor_buffer[0] != '\0') {
                int line_num = 1;
                int char_count = 0;
                term_fg = 0x888888; 
                char ln[8];
                int_to_str(line_num, ln);
                term_fb_print("  ");
                term_fb_print(ln);
                term_fb_print(" │ ");
                term_fg = 0xCCCCCC;
                
                for(int i = 0; editor_buffer[i] && char_count < 1000; i++) {
                    term_fb_putchar(editor_buffer[i]);
                    char_count++;
                    if(editor_buffer[i] == '\n' && editor_buffer[i+1] != '\0') {
                        line_num++;
                        term_fg = 0x888888;
                        char ln2[8];
                        int_to_str(line_num, ln2);
                        term_fb_print("  ");
                        term_fb_print(ln2);
                        term_fb_print(" │ ");
                        term_fg = 0xCCCCCC;
                    }
                }
                term_fb_print("\n");
                term_fg = 0x00AA00; term_fb_print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
                term_fg = 0x888888; term_fb_print("  Lines: ");
                char lc[8];
                int_to_str(editor_lines + 1, lc);
                term_fb_print(lc);
                term_fb_print(" │ Size: ");
                char sz[8];
                int_to_str(strlen(editor_buffer), sz);
                term_fb_print(sz);
                term_fb_print(" bytes\n\n");
            } else {
                term_fg = 0x666666; term_fb_print("  (empty file - press E to edit)\n\n");
            }
        } else {
            term_fg = 0x666666; term_fb_print("  No file open. Create or open a file to begin.\n\n");
        }
        
        // Menu with colors
        term_fg = 0xFFFF00; term_fb_print("╔════════════════════════════════════════╗\n");
        term_fb_print("║  ");
        term_fg = 0x00FF00; term_fb_print("[N]ew");
        term_fg = 0xFFFF00; term_fb_print("  │  ");
        term_fg = 0x00FF00; term_fb_print("[O]pen");
        term_fg = 0xFFFF00; term_fb_print("  │  ");
        term_fg = 0x00FF00; term_fb_print("[S]ave");
        term_fg = 0xFFFF00; term_fb_print("  │  ");
        term_fg = 0x00FF00; term_fb_print("[E]dit");
        term_fg = 0xFFFF00; term_fb_print("  │  ");
        term_fg = 0x00FF00; term_fb_print("[L]ist");
        term_fg = 0xFFFF00; term_fb_print("  │  ");
        term_fg = 0xFF4444; term_fb_print("[Q]uit");
        term_fg = 0xFFFF00; term_fb_print("  ║\n");
        term_fb_print("╚════════════════════════════════════════╝\n");
        
        term_fg = 0x00FF00; term_fb_print("\n🐢 > ");
        term_fg = 0xFFFFFF;
        
        char choice = keyboard_getchar_poll();
        term_fb_putchar(choice);
        term_fb_putchar('\n');
        
        switch(choice) {
            case 'N': case 'n': {
                term_fg = 0xFFFF00; term_fb_print("📄 Filename: ");
                term_fg = 0xFFFFFF;
                char* name = keyboard_readline_poll();
                if(name[0] != '\0') {
                    text_editor_new_file(name);
                    term_fg = 0x00FF00; term_fb_print("✅ Created new file: ");
                    term_fb_print(name);
                    term_fb_print("\n");
                }
                break;
            }
            case 'O': case 'o': {
                term_fg = 0xFFFF00; term_fb_print("📂 Open file: ");
                term_fg = 0xFFFFFF;
                char* name = keyboard_readline_poll();
                if(name[0] != '\0') {
                    text_editor_open_file(name);
                    if(current_file[0] != '\0') {
                        term_fg = 0x00FF00; term_fb_print("✅ Opened: ");
                        term_fb_print(name);
                        term_fb_print("\n");
                    }
                }
                break;
            }
            case 'S': case 's': {
                if(current_file[0] != '\0') {
                    text_editor_save_file(current_file, editor_buffer);
                    term_fg = 0x00FF00; term_fb_print("💾 Saved!\n");
                    term_fb_print("Press any key...");
                    keyboard_getchar_poll();
                } else {
                    term_fg = 0xFF4444; term_fb_print("❌ No file open!\n");
                    term_fb_print("Press any key...");
                    keyboard_getchar_poll();
                }
                break;
            }
            case 'E': case 'e': {
                if(current_file[0] == '\0') {
                    term_fg = 0xFF4444; term_fb_print("❌ No file open! Use [N]ew first.\n");
                    term_fb_print("Press any key...");
                    keyboard_getchar_poll();
                    break;
                }
                term_fg = 0xFFFF00; term_fb_print("📝 Enter text ('.' on empty line to finish):\n");
                term_fg = 0xFFFFFF;
                char* content = keyboard_read_multiline();
                strcpy(editor_buffer, content);
                editor_lines = 0;
                for(int i = 0; content[i]; i++) {
                    if(content[i] == '\n') editor_lines++;
                }
                term_fg = 0x00FF00; term_fb_print("✅ Text updated!\n");
                break;
            }
            case 'L': case 'l': {
                int count = 0;
                char** files = text_editor_list_files(&count);
                term_fg = 0xFFFF00; term_fb_print("\n📁 /user/documents/\n");
                term_fg = 0x00AA00; term_fb_print("━━━━━━━━━━━━━━━━━━━━━\n");
                if(count > 0) {
                    for(int i = 0; i < count; i++) {
                        term_fg = 0xCCCCCC; term_fb_print("  📄 ");
                        term_fb_print(files[i]);
                        term_fb_print("\n");
                    }
                } else {
                    term_fg = 0x666666; term_fb_print("  (empty)\n");
                }
                
                term_fg = 0xFFFF00; term_fb_print("\n📁 /user/projects/\n");
                term_fg = 0x00AA00; term_fb_print("━━━━━━━━━━━━━━━━━━━━━\n");
                file_list_t* proj = fs_list_directory("/user/projects");
                if(proj && proj->count > 0) {
                    for(int i = 0; i < proj->count; i++) {
                        term_fg = 0xCCCCCC; term_fb_print("  📦 ");
                        term_fb_print(proj->names[i]);
                        term_fb_print(" (");
                        char sz[8];
                        int_to_str(proj->sizes[i], sz);
                        term_fb_print(sz);
                        term_fb_print(" bytes)\n");
                    }
                } else {
                    term_fg = 0x666666; term_fb_print("  (empty)\n");
                }
                term_fb_print("\nPress any key...");
                keyboard_getchar_poll();
                break;
            }
            case 'Q': case 'q':
                return;
        }
    }
}

void text_editor_new_file(const char* filename) {
    strcpy(current_file, filename);
    memset(editor_buffer, 0, 4096);
    editor_lines = 0;
    
    char full_path[128];
    strcpy(full_path, "/user/documents/");
    strcat(full_path, filename);
    fs_write_file(full_path, (uint8_t*)"", 0);
}

void text_editor_open_file(const char* filename) {
    char full_path[128];
    strcpy(full_path, "/user/documents/");
    strcat(full_path, filename);
    
    if(fs_file_exists(full_path)) {
        strcpy(current_file, filename);
        uint32_t sz = fs_get_file_size(full_path);
        if(sz > 0 && sz < 4096) {
            fs_read_file(full_path, (uint8_t*)editor_buffer, sz);
            editor_buffer[sz] = '\0';
            editor_lines = 0;
            for(int i = 0; editor_buffer[i]; i++) {
                if(editor_buffer[i] == '\n') editor_lines++;
            }
        } else {
            memset(editor_buffer, 0, 4096);
            editor_lines = 0;
        }
    } else {
        term_fg = 0xFF4444; term_fb_print("❌ File not found!\n");
    }
}

void text_editor_save_file(const char* filename, const char* content) {
    char full_path[128];
    strcpy(full_path, "/user/documents/");
    strcat(full_path, filename);
    fs_write_file(full_path, (uint8_t*)content, strlen(content));
}

char** text_editor_list_files(int* count) {
    static char* file_list[64];
    static char file_names[64][64];
    *count = 0;
    
    file_list_t* files = fs_list_directory("/user/documents");
    if(files && files->count > 0) {
        for(int i = 0; i < files->count && *count < 64; i++) {
            strcpy(file_names[*count], files->names[i]);
            file_list[*count] = file_names[*count];
            (*count)++;
        }
    }
    return file_list;
}

void text_editor_delete_file(const char* filename) {
    char full_path[128];
    strcpy(full_path, "/user/documents/");
    strcat(full_path, filename);
    fs_delete_file(full_path);
}
