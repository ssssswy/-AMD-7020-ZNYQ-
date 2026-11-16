#ifndef TG_UI_AI_NOTICE_H
#define TG_UI_AI_NOTICE_H
#include "ui_shell.h"
#include "wave_render.h"
void tg_ai_notice_draw(uintptr_t,const tracegpu_ui_shell *,const tracegpu_ui_state *);
void tg_ai_highlight_plan(tracegpu_wave_plan *,const tracegpu_ui_state *);
/* Cooperative postprocess: one foreground plan, at most budget rectangles/call. */
int tg_ai_highlight_slice_begin(tracegpu_wave_plan *,const tracegpu_ui_state *);
int tg_ai_highlight_slice_step(tracegpu_wave_plan *,unsigned budget);
void tg_ai_highlight_draw(uintptr_t,const tracegpu_ui_state *);
int tg_ai_notice_locate(tracegpu_ui_state *,tracegpu_ui_shell *,const tg_ai_event *);
#endif
