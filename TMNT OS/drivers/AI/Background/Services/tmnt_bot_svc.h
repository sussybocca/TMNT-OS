#ifndef TMNT_BOT_SVC_H
#define TMNT_BOT_SVC_H
#include "system/types.h"

#define MAX_LEARNED 512
#define MAX_WORDS 2048

typedef struct {
    char word[32];
    int frequency;
    char contexts[16][64];
    int context_count;
} learned_word_t;

typedef struct {
    char pattern[128];
    char response[256];
    int confidence;
    int used_count;
} learned_pattern_t;

typedef struct {
    learned_word_t words[MAX_WORDS];
    int word_count;
    learned_pattern_t patterns[MAX_LEARNED];
    int pattern_count;
    int total_inputs;
    char last_input[256];
    char last_output[256];
} bot_brain_t;

extern bot_brain_t brain;

void tmnt_bot_svc_init(void);
void tmnt_bot_svc_learn_input(const char* input);
void tmnt_bot_svc_learn_output(const char* output);
void tmnt_bot_svc_learn_pair(const char* input, const char* output);
char* tmnt_bot_svc_generate_response(const char* input);
void tmnt_bot_svc_save_brain(void);
void tmnt_bot_svc_load_brain(void);
int tmnt_bot_svc_get_total_learned(void);

#endif