#include "ui_freeze_snapshot.h"
static struct {tracegpu_ui_state state;tracegpu_wave_plan plan;} frozen_data;
unsigned tracegpu_freeze_bytes(void){return sizeof frozen_data;}
void tracegpu_freeze_store_prepared(tracegpu_ui_state *state,const tracegpu_wave_plan *plan,const tracegpu_uart_event_ring *ring){
 frozen_data.state=*state;frozen_data.state.frozen=1;frozen_data.plan=*plan;
 for(unsigned i=0;i<TRACEGPU_UART_CHANNELS;i++)frozen_data.state.channel_ring[i]=0;
 tracegpu_dashboard_freeze_capture(state,ring);*state=frozen_data.state;
}
void tracegpu_freeze_resolve(tracegpu_ui_state *state,tracegpu_wave_plan *plan,const tracegpu_uart_event_ring *ring,int frozen,int take){
 if(take){unsigned i;if(state->data_page)plan->count=plan->pixels=0;else tracegpu_wave_build(plan,state,ring);frozen_data.state=*state;frozen_data.state.frozen=1;
 for(i=0;i<TRACEGPU_UART_CHANNELS;i++)frozen_data.state.channel_ring[i]=0;
 frozen_data.plan=*plan;tracegpu_dashboard_freeze_capture(state,ring);}
 if(frozen||take){*state=frozen_data.state;*plan=frozen_data.plan;}
 else if(state->data_page)plan->count=plan->pixels=0;
 else tracegpu_wave_build(plan,state,ring);
}
