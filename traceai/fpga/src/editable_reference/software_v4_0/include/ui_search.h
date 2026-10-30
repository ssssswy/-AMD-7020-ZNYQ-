#ifndef TRACEGPU_UI_SEARCH_H
#define TRACEGPU_UI_SEARCH_H
#include "ui_shell.h"
void tracegpu_ui_search_init(tracegpu_ui_shell *s);
int tracegpu_ui_search_action(tracegpu_ui_shell *s,int action);
void tracegpu_ui_search_draw(uintptr_t fb,const tracegpu_ui_shell *s,int x);
const char *tracegpu_ui_search_label(int id,const tracegpu_ui_shell *s,char *buf);
#endif
