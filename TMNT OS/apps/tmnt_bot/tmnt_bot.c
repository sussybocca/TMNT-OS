#include "tmnt_bot.h"
#include "../../external/gui.h"
#include "../../drivers/AI/Background/Services/tmnt_bot_svc.h"
#include "../../fs/tmnt_fs.h"
#include "../../system/string.h"
#include "../../system/memory.h"

extern uint32_t* fb;
extern uint32_t fb_width, fb_height, fb_pitch;

static knowledge_entry_t* knowledge = 0;
static int knowledge_count = 0;
static int knowledge_capacity = 0;

static char chat_history[32][256];
static int chat_lines = 0;
static char input_buffer[256];
static int input_pos = 0;
static char status_text[256];

static int str_contains(const char* str, const char* substr) {
    int i = 0, j = 0;
    while(str[i]) {
        if(str[i] == substr[j]) { j++; if(!substr[j]) return 1; }
        else j = 0;
        i++;
    }
    return 0;
}

static void str_cpy(char* dst, const char* src, int max) {
    int i = 0;
    while(src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void str_cat(char* dst, const char* src, int max) {
    int len = strlen(dst);
    int i = 0;
    while(src[i] && (len + i) < max - 1) { dst[len + i] = src[i]; i++; }
    dst[len + i] = 0;
}

static const char* knowledge_get(const char* keyword) {
    for(int i = 0; i < knowledge_count; i++) {
        if(strcmp(knowledge[i].keyword, keyword) == 0) return knowledge[i].response;
    }
    return 0;
}

static void expand_template(char* out, int out_max, const char* templ, const char* name, const char* description) {
    out[0] = 0;
    int i = 0;
    while(templ[i] && strlen(out) < (unsigned int)(out_max - 1)) {
        if(templ[i] == '$' && templ[i+1]) {
            i++;
            if(templ[i] == 'N') { str_cat(out, name, out_max); i++; }
            else if(templ[i] == 'U') {
                for(int j = 0; name[j]; j++) {
                    char c = name[j];
                    if(c >= 'a' && c <= 'z') c -= 32;
                    int l = strlen(out);
                    if(l < out_max - 1) out[l] = c;
                    out[l+1] = 0;
                }
                i++;
            }
            else if(templ[i] == 'D') { if(description) str_cat(out, description, out_max); i++; }
            else { int l = strlen(out); if(l < out_max-1) out[l]=templ[i-1]; if(l+1<out_max-1) out[l+1]=templ[i]; out[l+2]=0; i++; }
        } else { int l = strlen(out); if(l < out_max-1) out[l]=templ[i]; out[l+1]=0; i++; }
    }
}

static void parse_knowledge_line(const char* line) {
    if(!line || !line[0] || line[0] == '#') return;
    char keyword[64], response[512], action[64];
    int type = 0, pos = 0;
    
    int k = 0;
    while(line[pos] && line[pos] != '|' && k < 63) keyword[k++] = line[pos++];
    keyword[k] = 0;
    if(line[pos] == '|') pos++;
    if(line[pos] >= '0' && line[pos] <= '9') { type = line[pos] - '0'; pos++; if(line[pos] == '|') pos++; }
    int r = 0;
    while(line[pos] && line[pos] != '|' && r < 511) response[r++] = line[pos++];
    response[r] = 0;
    if(line[pos] == '|') pos++;
    int a = 0;
    while(line[pos] && a < 63) action[a++] = line[pos++];
    action[a] = 0;
    if(!keyword[0] || !response[0]) return;
    
    if(knowledge_count >= knowledge_capacity) {
        int new_cap = knowledge_capacity ? knowledge_capacity * 2 : 64;
        knowledge_entry_t* new_k = (knowledge_entry_t*)malloc(sizeof(knowledge_entry_t) * new_cap);
        if(!new_k) return;
        if(knowledge) { for(int i = 0; i < knowledge_count; i++) new_k[i] = knowledge[i]; free(knowledge); }
        knowledge = new_k;
        knowledge_capacity = new_cap;
    }
    str_cpy(knowledge[knowledge_count].keyword, keyword, 64);
    str_cpy(knowledge[knowledge_count].response, response, 512);
    str_cpy(knowledge[knowledge_count].action, action, 64);
    knowledge[knowledge_count].type = type;
    knowledge_count++;
}

void tmnt_bot_load_knowledge(void) {
    if(knowledge) { free(knowledge); knowledge = 0; }
    knowledge_count = 0;
    knowledge_capacity = 0;
    
    file_list_t* files = fs_list_directory("/system/ai/knowledge");
    if(files) {
        for(int i = 0; i < files->count; i++) {
            char path[128] = "/system/ai/knowledge/";
            strcat(path, files->names[i]);
            uint32_t sz = fs_get_file_size(path);
            if(sz > 0 && sz < 65536) {
                char* data = (char*)malloc(sz + 1);
                if(data && fs_read_file(path, (uint8_t*)data, sz) > 0) {
                    data[sz] = 0;
                    char* line = data;
                    while(*line) {
                        char* end = line;
                        while(*end && *end != '\n') end++;
                        char saved = *end;
                        *end = 0;
                        parse_knowledge_line(line);
                        if(saved == 0) break;
                        line = end + 1;
                    }
                }
                if(data) free(data);
            }
        }
    }
}

static void add_chat(const char* prefix, const char* msg) {
    if(chat_lines >= 31) {
        for(int i = 0; i < 31; i++) str_cpy(chat_history[i], chat_history[i+1], 256);
        chat_lines = 31;
    }
    str_cpy(chat_history[chat_lines], prefix, 256);
    str_cat(chat_history[chat_lines], msg, 256);
    chat_lines++;
}

static void process_input(const char* input) {
    if(!input || !input[0]) return;
    
    add_chat("🧑 ", input);
    
    // Handle reload knowledge command
    if(str_contains(input, "reload knowledge") || str_contains(input, "refresh knowledge")) {
        tmnt_bot_load_knowledge();
        tmnt_bot_svc_load_brain();
        add_chat("🐢 ", "Knowledge base reloaded from /system/ai/knowledge/ and learned patterns restored!");
        str_cpy(status_text, "Knowledge reloaded", 256);
        return;
    }
    
    // Handle list knowledge command
    if(str_contains(input, "list knowledge") || str_contains(input, "what do you know")) {
        char topics[512];
        topics[0] = 0;
        str_cpy(topics, "Knowledge files: ", 256);
        int remaining = 256 - strlen(topics);
        for(int i = 0; i < knowledge_count && remaining > 3; i++) {
            if(i > 0) { str_cat(topics, ", ", 256); remaining -= 2; }
            int kw_len = strlen(knowledge[i].keyword);
            if(kw_len < remaining) {
                str_cat(topics, knowledge[i].keyword, 256);
                remaining -= kw_len;
            } else break;
        }
        add_chat("🐢 ", topics);
        
        char learned[256];
        strcpy(learned, "Learned patterns: ");
        char num[8];
        int_to_str(tmnt_bot_svc_get_total_learned(), num);
        strcat(learned, num);
        add_chat("🐢 ", learned);
        str_cpy(status_text, "Listed all knowledge", 256);
        return;
    }
    
    // Handle app generation
    if(str_contains(input, "generate app") || str_contains(input, "create app") || str_contains(input, "make app")) {
        char appname[32] = "new_app";
        const char* markers[] = {"app ", "app named ", "called "};
        for(int m = 0; m < 3; m++) {
            const char* pos = strstr(input, markers[m]);
            if(pos) {
                pos += strlen(markers[m]);
                while(*pos == ' ') pos++;
                int n = 0;
                while(*pos && *pos != ' ' && *pos != '.' && n < 30) appname[n++] = *pos++;
                appname[n] = 0;
                break;
            }
        }
        
        char appdir[128] = "/apps/";
        strcat(appdir, appname);
        fs_create_dir(appdir);
        
        const char* h_template = knowledge_get("header_template");
        const char* draw_template = knowledge_get("draw_template");
        const char* open_template = knowledge_get("open_template");
        const char* includes_list = knowledge_get("app_includes");
        
        char hpath[128], hcontent[2048];
        strcpy(hpath, appdir); strcat(hpath, "/"); strcat(hpath, appname); strcat(hpath, ".h");
        
        if(h_template) {
            expand_template(hcontent, 2048, h_template, appname, input);
        } else {
            strcpy(hcontent, "// "); strcat(hcontent, appname); strcat(hcontent, " - Generated by TMNT Bot\n#ifndef ");
            for(int i = 0; appname[i]; i++) { char c = appname[i]; if(c >= 'a' && c <= 'z') c -= 32; int l = strlen(hcontent); hcontent[l]=c; hcontent[l+1]=0; }
            strcat(hcontent, "_H\n#define ");
            for(int i = 0; appname[i]; i++) { char c = appname[i]; if(c >= 'a' && c <= 'z') c -= 32; int l = strlen(hcontent); hcontent[l]=c; hcontent[l+1]=0; }
            strcat(hcontent, "_H\n\nvoid "); strcat(hcontent, appname); strcat(hcontent, "_open(void);\n\n#endif\n");
        }
        fs_write_file(hpath, (uint8_t*)hcontent, strlen(hcontent));
        
        char cpath[128], ccontent[8192];
        ccontent[0] = 0;
        strcpy(cpath, appdir); strcat(cpath, "/"); strcat(cpath, appname); strcat(cpath, ".c");
        
        if(includes_list) {
            char expanded[1024];
            expand_template(expanded, 1024, includes_list, appname, input);
            strcat(ccontent, expanded);
        } else {
            strcat(ccontent, "#include \""); strcat(ccontent, appname);
            strcat(ccontent, ".h\"\n#include \"../../external/gui.h\"\n");
        }
        
        if(draw_template) {
            char expanded[4096];
            expand_template(expanded, 4096, draw_template, appname, input);
            strcat(ccontent, expanded);
        }
        
        if(open_template) {
            char expanded[2048];
            expand_template(expanded, 2048, open_template, appname, input);
            strcat(ccontent, expanded);
        } else {
            strcat(ccontent, "void "); strcat(ccontent, appname);
            strcat(ccontent, "_open(void) {\n    gui_run_auto_app(\"");
            strcat(ccontent, appname); strcat(ccontent, "\", 100, 50, 600, 400, ");
            strcat(ccontent, draw_template ? "draw_content" : "0");
            strcat(ccontent, ", 0, 0);\n}\n");
        }
        fs_write_file(cpath, (uint8_t*)ccontent, strlen(ccontent));
        
        char msg[256];
        strcpy(msg, "App '"); strcat(msg, appname); strcat(msg, "' created in /apps/"); strcat(msg, appname); strcat(msg, "/ - Rebuild ISO to use!");
        add_chat("🐢 ", msg);
        str_cpy(status_text, "App generated", 256);
        return;
    }
    
    // Handle edit file command
    if(str_contains(input, "edit file") || str_contains(input, "debug file") || str_contains(input, "fix file")) {
        char filepath[128];
        filepath[0] = 0;
        const char* markers2[] = {"file ", "file named ", "path "};
        for(int m = 0; m < 3; m++) {
            const char* pos = strstr(input, markers2[m]);
            if(pos) {
                pos += strlen(markers2[m]);
                while(*pos == ' ') pos++;
                int n = 0;
                while(*pos && *pos != ' ' && n < 127) filepath[n++] = *pos++;
                filepath[n] = 0;
                break;
            }
        }
        if(filepath[0] && fs_file_exists(filepath)) {
            uint32_t sz = fs_get_file_size(filepath);
            char sizestr[16];
            int_to_str(sz, sizestr);
            char msg[256];
            strcpy(msg, "File '"); strcat(msg, filepath); strcat(msg, "' exists ("); strcat(msg, sizestr); strcat(msg, " bytes). Tell me what changes to make!");
            add_chat("🐢 ", msg);
        } else if(filepath[0]) {
            char msg[256];
            strcpy(msg, "File not found: "); strcat(msg, filepath);
            add_chat("🐢 ", msg);
        } else {
            add_chat("🐢 ", "Specify a file path. Example: 'edit file /apps/myapp/myapp.c'");
        }
        str_cpy(status_text, "File operation", 256);
        return;
    }
    
    // Check knowledge files
    for(int i = 0; i < knowledge_count; i++) {
        if(str_contains(input, knowledge[i].keyword)) {
            add_chat("🐢 ", knowledge[i].response);
            str_cpy(status_text, "Knowledge match", 256);
            return;
        }
    }
    
    // Try learned patterns from background service
    char* learned = tmnt_bot_svc_generate_response(input);
    if(learned && learned[0] && strcmp(learned, "Ask me anything! I learn from everything you do!") != 0 && strcmp(learned, "I'm learning! Use more commands and I'll get smarter!") != 0) {
        add_chat("🐢 ", learned);
        str_cpy(status_text, "Learned response", 256);
        return;
    }
    
    // Fallback
    if(brain.pattern_count > 0 || knowledge_count > 0) {
        add_chat("🐢 ", "I don't know that yet. Keep using the OS and I'll learn! Try 'list knowledge' to see what I know.");
    } else {
        add_chat("🐢 ", "I'm a fresh brain! Use terminal commands and I'll learn from everything you do. Type 'list knowledge' to see what I know so far.");
    }
    str_cpy(status_text, "No match found", 256);
}

static void bot_draw(int wx, int wy, int ww, int wh) {
    for(int dy = 0; dy < 28; dy++) {
        uint8_t g = 20 + dy * 4;
        uint32_t color = (0x00 << 16) | (g << 8) | 0x00;
        for(int dx = 4; dx < ww - 4; dx++) gui_fb_putpixel(wx + dx, wy + 30 + dy, color);
    }
    gui_fb_draw_rect(wx + 4, wy + 30, ww - 8, 28, 0x00FF00);
    gui_fb_draw_text(wx + (ww - 200) / 2, wy + 36, "🐢 TMNT BOT AI 🐢", 0x00FF00, 0x0A2A0A);
    
    int chat_x = wx + 10, chat_y = wy + 65, chat_w = ww - 20, chat_h = wh - 160;
    gui_fb_fill_rect(chat_x - 1, chat_y - 1, chat_w + 2, chat_h + 2, 0x003300);
    gui_fb_fill_rect(chat_x, chat_y, chat_w, chat_h, 0x0A0A0A);
    
    int visible_lines = (chat_h - 10) / 16;
    int start_line = chat_lines - visible_lines;
    if(start_line < 0) start_line = 0;
    for(int i = 0; i < visible_lines && (start_line + i) < chat_lines; i++) {
        gui_fb_draw_text(chat_x + 5, chat_y + 5 + i * 16, chat_history[start_line + i], 0x00CC00, 0x0A0A0A);
    }
    
    int input_y = wy + wh - 85;
    gui_fb_fill_rect(wx + 10, input_y, ww - 20, 24, 0x1A1A2E);
    gui_fb_draw_rect(wx + 10, input_y, ww - 20, 24, 0x00AA00);
    gui_fb_draw_text(wx + 15, input_y + 4, input_buffer, 0x00FF00, 0x1A1A2E);
    
    int status_y = wy + wh - 55;
    gui_fb_fill_rect(wx + 10, status_y, ww - 20, 20, 0x0A1A0A);
    gui_fb_draw_rect(wx + 10, status_y, ww - 20, 20, 0x00CC00);
    
    char brain_info[256];
    strcpy(brain_info, status_text);
    strcat(brain_info, " | Knowledge: ");
    char num[8];
    int_to_str(knowledge_count, num);
    strcat(brain_info, num);
    strcat(brain_info, " | Learned: ");
    int_to_str(tmnt_bot_svc_get_total_learned(), num);
    strcat(brain_info, num);
    gui_fb_draw_text(wx + 15, status_y + 2, brain_info, 0xFFFF00, 0x0A1A0A);
    
    const char* help = knowledge_get("bot_help_text");
    gui_fb_draw_text(wx + 15, wy + wh - 30, help ? help : "Type and press Enter | create app NAME | list knowledge | reload knowledge | edit file PATH", 0x666666, 0x0A1A0A);
}

static void bot_click(int mx, int my) { (void)mx; (void)my; }

static void bot_key(char key) {
    if(key == '\b') { if(input_pos > 0) { input_pos--; input_buffer[input_pos] = 0; } }
    else if(key == '\n' || key == '\r') {
        if(input_pos > 0) { input_buffer[input_pos] = 0; process_input(input_buffer); input_pos = 0; input_buffer[0] = 0; }
    }
    else if(key >= ' ' && key <= '~' && input_pos < 250) { input_buffer[input_pos++] = key; input_buffer[input_pos] = 0; }
}

void tmnt_bot_open(void) {
    chat_lines = 0;
    input_pos = 0;
    input_buffer[0] = 0;
    str_cpy(status_text, "AI Ready", 256);
    
    tmnt_bot_load_knowledge();
    
    int total = tmnt_bot_svc_get_total_learned() + knowledge_count;
    char welcome[256];
    strcpy(welcome, "COWABUNGA! I'm TMNT Bot with ");
    char num[8];
    int_to_str(total, num);
    strcat(welcome, num);
    strcat(welcome, " total patterns! Commands: 'create app NAME' | 'list knowledge' | 'reload knowledge' | 'edit file PATH' | or just chat!");
    add_chat("🐢 ", welcome);
    
    gui_run_auto_app("🐢 TMNT Bot AI", 80, 40, 640, 480, bot_draw, bot_click, bot_key);
}