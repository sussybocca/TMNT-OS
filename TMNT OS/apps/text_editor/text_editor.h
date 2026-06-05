#ifndef TEXT_EDITOR_H
#define TEXT_EDITOR_H

void text_editor_init(void);
void text_editor_open(void);
void text_editor_new_file(const char* filename);
void text_editor_open_file(const char* filename);
void text_editor_save_file(const char* filename, const char* content);
char** text_editor_list_files(int* count);
void text_editor_delete_file(const char* filename);
void text_editor_compile(const char* source_file, const char* output_file);

#endif