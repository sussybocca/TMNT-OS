#ifndef TM_LANG_H
#define TM_LANG_H

void tm_execute(const char* source);
void tm_run_file(const char* filepath);

// New TMNT app packaging
int tm_compile_to_app(const char* source, const char* app_name, const char* icon_name);
int tm_install_app(const char* tme_path);

#endif