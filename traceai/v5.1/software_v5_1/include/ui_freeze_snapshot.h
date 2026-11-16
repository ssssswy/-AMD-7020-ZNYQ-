#ifndef TRACEGPU_UI_FREEZE_H
#define TRACEGPU_UI_FREEZE_H
#include "wave_render.h"
void tracegpu_freeze_resolve(tracegpu_ui_state *state,tracegpu_wave_plan *plan,const tracegpu_uart_event_ring *ring,int frozen,int take);
void tracegpu_freeze_store_prepared(tracegpu_ui_state *,const tracegpu_wave_plan *,const tracegpu_uart_event_ring *);
unsigned tracegpu_freeze_bytes(void);
#endif
