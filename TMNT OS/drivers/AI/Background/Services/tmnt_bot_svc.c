#include "tmnt_bot_svc.h"
#include "fs/tmnt_fs.h"
#include "system/string.h"
#include "system/memory.h"

bot_brain_t brain;

static int split_words(const char* str, char words[][32], int max) {
    int count = 0;
    const char* p = str;
    while(*p && count < max) {
        while(*p == ' ' || *p == '\n' || *p == '\t') p++;
        if(!*p) break;
        int w = 0;
        while(*p && *p != ' ' && *p != '\n' && *p != '\t' && w < 31) {
            words[count][w++] = *p++;
        }
        words[count][w] = 0;
        if(w > 1) count++;
    }
    return count;
}

static void learn_words(const char* input) {
    char words[64][32];
    int count = split_words(input, words, 64);
    
    for(int i = 0; i < count; i++) {
        int found = -1;
        for(int j = 0; j < brain.word_count; j++) {
            if(strcmp(brain.words[j].word, words[i]) == 0) {
                found = j;
                break;
            }
        }
        
        if(found >= 0) {
            brain.words[found].frequency++;
        } else if(brain.word_count < MAX_WORDS) {
            strcpy(brain.words[brain.word_count].word, words[i]);
            brain.words[brain.word_count].frequency = 1;
            brain.words[brain.word_count].context_count = 0;
            brain.word_count++;
        }
        
        if(found >= 0 && brain.words[found].context_count < 16) {
            if(i > 0) {
                strcpy(brain.words[found].contexts[brain.words[found].context_count], words[i-1]);
                brain.words[found].context_count++;
            }
            if(i < count - 1 && brain.words[found].context_count < 16) {
                strcpy(brain.words[found].contexts[brain.words[found].context_count], words[i+1]);
                brain.words[found].context_count++;
            }
        }
    }
}

static void learn_pattern(const char* input, const char* output) {
    for(int i = 0; i < brain.pattern_count; i++) {
        if(strcmp(brain.patterns[i].pattern, input) == 0) {
            brain.patterns[i].used_count++;
            brain.patterns[i].confidence += 10;
            strcpy(brain.patterns[i].response, output);
            tmnt_bot_svc_save_brain();
            return;
        }
    }
    
    if(brain.pattern_count < MAX_LEARNED && strlen(input) > 1) {
        strcpy(brain.patterns[brain.pattern_count].pattern, input);
        strcpy(brain.patterns[brain.pattern_count].response, output);
        brain.patterns[brain.pattern_count].confidence = 50;
        brain.patterns[brain.pattern_count].used_count = 1;
        brain.pattern_count++;
        tmnt_bot_svc_save_brain();
    }
}

void tmnt_bot_svc_init(void) {
    memset(&brain, 0, sizeof(bot_brain_t));
    tmnt_bot_svc_load_brain();
}

void tmnt_bot_svc_learn_input(const char* input) {
    if(!input || !input[0]) return;
    strcpy(brain.last_input, input);
    learn_words(input);
    brain.total_inputs++;
}

void tmnt_bot_svc_learn_output(const char* output) {
    if(!output || !output[0]) return;
    strcpy(brain.last_output, output);
    
    if(brain.last_input[0] && brain.last_output[0]) {
        learn_pattern(brain.last_input, brain.last_output);
        brain.last_input[0] = 0;
    }
}

void tmnt_bot_svc_learn_pair(const char* input, const char* output) {
    if(input && output) {
        learn_words(input);
        learn_words(output);
        learn_pattern(input, output);
    }
}

char* tmnt_bot_svc_generate_response(const char* input) {
    static char response[512];
    response[0] = 0;
    
    if(!input || !input[0]) {
        strcpy(response, "Ask me anything! I learn from everything you do!");
        return response;
    }
    
    for(int i = 0; i < brain.pattern_count; i++) {
        if(strcmp(brain.patterns[i].pattern, input) == 0) {
            strcpy(response, brain.patterns[i].response);
            brain.patterns[i].used_count++;
            return response;
        }
    }
    
    int best_match = -1;
    int best_score = 0;
    char words[32][32];
    int word_count = split_words(input, words, 32);
    
    for(int i = 0; i < brain.pattern_count; i++) {
        int score = 0;
        for(int j = 0; j < word_count; j++) {
            if(strstr(brain.patterns[i].pattern, words[j])) {
                score += brain.patterns[i].confidence;
            }
        }
        if(score > best_score) {
            best_score = score;
            best_match = i;
        }
    }
    
    if(best_match >= 0 && best_score > 15) {
        strcpy(response, brain.patterns[best_match].response);
        brain.patterns[best_match].used_count++;
        return response;
    }
    
    if(brain.word_count > 5) {
        strcpy(response, "I've learned about: ");
        int added = 0;
        for(int i = 0; i < brain.word_count && added < 8; i++) {
            if(brain.words[i].frequency > 2) {
                if(added > 0) strcat(response, ", ");
                strcat(response, brain.words[i].word);
                added++;
            }
        }
        if(added == 0) strcpy(response, "Still learning... keep using the OS!");
        return response;
    }
    
    strcpy(response, "I'm learning! Use more commands and I'll get smarter!");
    return response;
}

void tmnt_bot_svc_save_brain(void) {
    fs_create_dir("/system");
    fs_create_dir("/system/ai");
    
    char data[65536];
    data[0] = 0;
    
    for(int i = 0; i < brain.pattern_count; i++) {
        if(strlen(data) + strlen(brain.patterns[i].pattern) + strlen(brain.patterns[i].response) + 20 < 65500) {
            strcat(data, brain.patterns[i].pattern);
            strcat(data, "|0|");
            strcat(data, brain.patterns[i].response);
            strcat(data, "|\n");
        }
    }
    
    if(strlen(data) > 0) {
        fs_write_file("/system/ai/learned.kn", (uint8_t*)data, strlen(data));
    }
}

void tmnt_bot_svc_load_brain(void) {
    char path[128] = "/system/ai/learned.kn";
    if(!fs_file_exists(path)) return;
    
    uint32_t sz = fs_get_file_size(path);
    if(sz == 0 || sz > 65535) return;
    
    char* data = (char*)malloc(sz + 1);
    if(!data) return;
    
    if(fs_read_file(path, (uint8_t*)data, sz) > 0) {
        data[sz] = 0;
        char* line = data;
        while(*line && brain.pattern_count < MAX_LEARNED) {
            char* end = line;
            while(*end && *end != '\n') end++;
            char saved = *end;
            *end = 0;
            
            char* sep1 = strchr(line, '|');
            if(sep1) {
                *sep1 = 0;
                char* pattern = line;
                char* response = sep1 + 3;
                char* sep2 = strchr(response, '|');
                if(sep2) *sep2 = 0;
                
                strcpy(brain.patterns[brain.pattern_count].pattern, pattern);
                strcpy(brain.patterns[brain.pattern_count].response, response);
                brain.patterns[brain.pattern_count].confidence = 40;
                brain.patterns[brain.pattern_count].used_count = 1;
                brain.pattern_count++;
            }
            
            if(saved == 0) break;
            line = end + 1;
        }
    }
    free(data);
}

int tmnt_bot_svc_get_total_learned(void) {
    return brain.pattern_count;
}