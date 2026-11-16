#include "ui_freeze_snapshot.h"
#include <assert.h>
#include <stdio.h>
static int generation,captured;
void tracegpu_wave_build(tracegpu_wave_plan *p,const tracegpu_ui_state *u,const tracegpu_uart_event_ring *r){(void)u;(void)r;p->count=1;p->rects[0].x=generation;p->colors[0]=generation;}
void tracegpu_dashboard_freeze_capture(const tracegpu_ui_state *u,const tracegpu_uart_event_ring *r){(void)u;(void)r;captured++;}
int main(void){tracegpu_ui_state u={0};static tracegpu_wave_plan p;
 u.byte_count=256;u.selected_channel=2;generation=7;tracegpu_freeze_resolve(&u,&p,0,0,1);assert(u.frozen&&p.rects[0].x==7&&captured==1);
 for(generation=8;generation<100;generation++){u.byte_count=900;u.selected_channel=0;tracegpu_freeze_resolve(&u,&p,0,1,0);assert(u.byte_count==256&&u.selected_channel==2&&p.rects[0].x==7&&!u.channel_ring[0]);}
 assert(captured==1&&tracegpu_freeze_bytes()<256*1024);tracegpu_freeze_resolve(&u,&p,0,0,0);assert(p.rects[0].x==100);
 puts("PASS freeze: immutable geometry/metadata, no history pointers, storage below 256KiB and live restore");return 0;}
