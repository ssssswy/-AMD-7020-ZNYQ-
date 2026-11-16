#include "capture_memory.h"
#include "ui_channel_panel.h"
#include "wave_render.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_uart_dual_stream stream;
static tracegpu_wave_plan plan;
static tracegpu_uart_event event(unsigned ch,unsigned type,unsigned seq,unsigned t,unsigned data){
 tracegpu_uart_event e={{t,0,(seq<<8)|ch,(type<<24)|(data<<8)|(seq>>24)}};return e;
}
int main(void){
 unsigned ch,i;tracegpu_ui_state ui={0};
 tracegpu_uart_dual_clear(&stream);assert(tracegpu_capture_memory_bind(&stream)==0);
 assert(sizeof(stream)<65536U);
 for(ch=0;ch<TRACEGPU_UART_CHANNELS;ch++){
  assert(stream.channel[ch].history.records);
  if(ch)assert(stream.channel[ch].history.records!=stream.channel[ch-1].history.records);
 }
 for(i=0;i<256;i++)for(ch=0;ch<TRACEGPU_UART_CHANNELS;ch++){
  tracegpu_uart_event e=event(ch,2,i,1000+i*100,(i+ch)&255);
  assert(tracegpu_uart_dual_feed(&stream,&e,sizeof e)==1);
 }
 for(ch=0;ch<TRACEGPU_UART_CHANNELS;ch++){
  tracegpu_uart_event e=event(ch,4,256,30000,0);
  assert(tracegpu_uart_dual_feed(&stream,&e,sizeof e)==1);
  assert(stream.channel[ch].bytes==256&&stream.channel[ch].history.lost_events==0);
  assert(tracegpu_uart_burst_latest(&stream.channel[ch].bursts)->byte_count==256);
 }
 {tracegpu_uart_event e=event(TRACEGPU_UART_CHANNELS,2,0,0,0);assert(tracegpu_uart_dual_feed(&stream,&e,sizeof e)==0);assert(stream.bad_channel==1);}
 tracegpu_uart_dual_clear(&stream);
 for(i=0;i<TRACEGPU_UART_EVENT_RING_RECORDS+7U;i++){
  tracegpu_uart_event e=event(0,2,i,i,0);assert(tracegpu_uart_dual_feed(&stream,&e,sizeof e)==1);
 }
 assert(stream.channel[0].history.count==TRACEGPU_UART_EVENT_RING_RECORDS);
 assert(tracegpu_uart_event_sequence(tracegpu_uart_ring_oldest(&stream.channel[0].history,0))==7U);
 assert(stream.channel[0].history.lost_events==0);
 tracegpu_uart_dual_clear(&stream);
 for(ch=0;ch<TRACEGPU_UART_CHANNELS;ch++)assert(stream.channel[ch].history.records&&stream.channel[ch].history.count==0);
 if(TRACEGPU_UART_CHANNELS==4U){
  ui.dual_channel=ui.quad_channel=1;ui.channel_enabled_mask=15;ui.time_window_ticks=100;
  ui.view_anchor_ticks=100;
  for(ch=0;ch<4;ch++)ui.channel_ring[ch]=&stream.channel[ch].history;
  tracegpu_wave_build(&plan,&ui,0);assert(plan.count==4);
  for(ch=0;ch<4;ch++){assert(plan.colors[ch]==tracegpu_ui_channel_color(ch));assert(plan.rects[ch].y==tracegpu_ui_lane_high(&ui,ch));}
  ui.view_mode=2;tracegpu_wave_build(&plan,&ui,0);assert(plan.count==2);assert(plan.colors[0]==tracegpu_ui_channel_color(2));
  assert(tracegpu_ui_lane_at(&ui,TG_PLOT_TOP+4)==2&&tracegpu_ui_lane_at(&ui,TG_PLOT_TOP+(TG_PLOT_BOTTOM-TG_PLOT_TOP)/2+4)==3);
  ui.channel_enabled_mask=7;tracegpu_wave_build(&plan,&ui,0);assert(plan.count==1);
  ui.channel_enabled_mask=15;
  for(i=1;i<=6;i++){
   ui.view_mode=(uint8_t)i;tracegpu_wave_build(&plan,&ui,0);assert(plan.count==2);
   assert(plan.colors[0]==tracegpu_ui_channel_color(tracegpu_ui_lane_channel(&ui,0)));
   assert(plan.colors[1]==tracegpu_ui_channel_color(tracegpu_ui_lane_channel(&ui,1)));
   assert(tracegpu_ui_lane_channel(&ui,0)!=tracegpu_ui_lane_channel(&ui,1));
  }
 }
 printf("PASS external capture memory and %u-channel stream/UI\n",TRACEGPU_UART_CHANNELS);return 0;
}
