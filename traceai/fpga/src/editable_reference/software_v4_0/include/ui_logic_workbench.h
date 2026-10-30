#ifndef TRACEGPU_UI_LOGIC_WORKBENCH_H
#define TRACEGPU_UI_LOGIC_WORKBENCH_H
#include "ui_dashboard.h"
#include "ui_shell.h"
void tracegpu_ui_logic_data(uintptr_t,const tracegpu_ui_state *);
void tracegpu_ui_logic_overview(uintptr_t,const tracegpu_ui_state *);
const char *tracegpu_ui_bus_label(int,const tracegpu_ui_shell *,char *,unsigned);
void tracegpu_ui_bus_menu_draw(uintptr_t,const tracegpu_ui_shell *,const tracegpu_ui_state *,int x);
#endif
