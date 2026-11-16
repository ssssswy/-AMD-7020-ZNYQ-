#ifndef TG_UI_ERROR_HISTORY_H
#define TG_UI_ERROR_HISTORY_H
#include "ui_shell.h"
void tg_ui_error_draw(uintptr_t,const tracegpu_ui_shell *,const tracegpu_ui_state *,int x);
const char *tg_ui_error_label(int,const tracegpu_ui_shell *,const tracegpu_ui_state *,char *);
#endif
