#ifndef TRACEGPU_UI_TRIGGER_H
#define TRACEGPU_UI_TRIGGER_H
#include "ui_shell.h"
void tracegpu_ui_trigger_init(tracegpu_ui_shell *s);
int tracegpu_ui_trigger_action(tracegpu_ui_shell *s,int action);
unsigned tracegpu_ui_trigger_controls(const tracegpu_ui_shell *s,tracegpu_ui_control *out,unsigned n,int x);
const char *tracegpu_ui_trigger_label(int id,const tracegpu_ui_shell *s,char *buf);
const char *tracegpu_ui_trigger_title(unsigned page);
const char *tracegpu_ui_trigger_state(unsigned state);
void tracegpu_ui_trigger_draw(uintptr_t fb,const tracegpu_ui_shell *s,const tracegpu_ui_state *u,int x);
int tracegpu_ui_trigger_signed_time(char *buf,unsigned size,uint64_t tick,uint64_t origin);
#endif
