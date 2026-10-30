#ifndef TRACEGPU_UI_WORKFLOW_H
#define TRACEGPU_UI_WORKFLOW_H
#include "ui_shell.h"
enum {TG_FLOW_NONE,TG_FLOW_DATA,TG_FLOW_AI};
enum {TG_FLOW_OK,TG_FLOW_LEGACY,TG_FLOW_CONFIG,TG_FLOW_PROTOCOL,TG_FLOW_INVERT,
 TG_FLOW_HARDWARE,TG_FLOW_FROZEN,TG_FLOW_RUNNING,TG_FLOW_DRAINING,TG_FLOW_INCOMPLETE,TG_FLOW_EMPTY};
void tg_workflow_enable(tracegpu_ui_shell *);
unsigned tg_workflow_gate(const tracegpu_ui_shell *,const tracegpu_ui_state *,int hardware);
const char *tg_workflow_reason(unsigned);
int tg_workflow_config_valid(const tracegpu_ui_shell *);
void tg_workflow_invalidate(tracegpu_ui_shell *);
void tg_workflow_queue(tracegpu_ui_shell *,unsigned,uint32_t);
int tg_workflow_poll(tracegpu_ui_shell *,const tracegpu_ui_state *,uint32_t,int);
int tg_workflow_action(tracegpu_ui_shell *,int,const tracegpu_ui_state *,int *handled);
int tg_workflow_controls(const tracegpu_ui_shell *,tracegpu_ui_control *);
void tg_workflow_dock(const tracegpu_ui_shell *,tracegpu_ui_control *,unsigned *);
const char *tg_workflow_label(int,const tracegpu_ui_shell *,const tracegpu_ui_state *,char *);
void tg_workflow_draw(uintptr_t,const tracegpu_ui_shell *,const tracegpu_ui_state *,int);
const char *tg_workflow_title(unsigned);
#endif
