#ifndef TMNT_BOT_H
#define TMNT_BOT_H

#include "../../system/types.h"

#define KNOWLEDGE_GENERAL 0
#define KNOWLEDGE_TERMINAL 1
#define KNOWLEDGE_DEBUG 2
#define KNOWLEDGE_CODING 3
#define KNOWLEDGE_SYSTEM 4

typedef struct {
    char keyword[64];
    char response[512];
    char action[64];
    int type;
} knowledge_entry_t;

void tmnt_bot_open(void);
void tmnt_bot_load_knowledge(void);
void tmnt_bot_svc_init(void);
void tmnt_bot_svc_learn_input(const char* input);
void tmnt_bot_svc_learn_output(const char* output);
char* tmnt_bot_svc_generate_response(const char* input);

#endif