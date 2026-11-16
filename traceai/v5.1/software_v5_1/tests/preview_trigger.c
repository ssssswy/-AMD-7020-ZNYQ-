#include "ui_trigger.h"
#include "ui_paint.h"
#include "ui_layout.h"
#include "wave_cursor.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t pixels[800*480];
static tracegpu_uart_event_ring rings[4];
void XTime_GetTime(uint64_t *t){*t=0;}
static void bmp(const char *path){
 unsigned char header[54]={0x42,0x4d};FILE *f;uint32_t size=54+800*480*4,offset=54,dib=40,w=800,h=480;uint16_t planes=1,bits=32;
 memcpy(header+2,&size,4);memcpy(header+10,&offset,4);memcpy(header+14,&dib,4);memcpy(header+18,&w,4);memcpy(header+22,&h,4);memcpy(header+26,&planes,2);memcpy(header+28,&bits,2);
 f=fopen(path,"wb");assert(f);assert(fwrite(header,1,54,f)==54);for(unsigned i=480;i;i--)assert(fwrite(pixels+(i-1)*800,4,800,f)==800);assert(!fclose(f));
}
int main(int argc,char **argv){
 tracegpu_ui_state u={0};tracegpu_ui_shell s;unsigned mode=argc>2?(unsigned)atoi(argv[2]):0;assert(argc>1);assert(!tracegpu_dashboard_init());
 u.dual_channel=u.quad_channel=1;u.channel_enabled_mask=15;u.baud=115200;u.data_bits=8;u.stop_bits=1;u.drain_state=2;
 u.time_window_ticks=1000000;u.view_anchor_ticks=u.latest_event_ticks=1000000;u.gpu_active=1;
 for(unsigned ch=0;ch<4;ch++){u.channel_ring[ch]=&rings[ch];u.channel_baud[ch]=115200;for(unsigned i=0;i<60;i++){tracegpu_uart_event e={{i*16000+ch*1000,0,(i<<8)|ch,((i?TRACEGPU_RECORD_RAW_EDGE:TRACEGPU_RECORD_SESSION_INIT)<<24)|((i&1)<<16)}};tracegpu_uart_ring_push(&rings[ch],&e);}}
 u.advanced_trigger=(tracegpu_trigger_ui_status){.state=TG_TRIG_READY,.capture_id=27,.reason=2,.source_mask=2,.predicate_mask=1,.stop_reason=TG_TRIG_STOP_TIME,.trigger_tick=600000,.condition_start_tick=400000,.valid=1,.has_trigger=1};
 for(unsigned ch=0;ch<4;ch++){u.advanced_trigger.actual_pre_ticks[ch]=500000;u.advanced_trigger.actual_post_ticks[ch]=400000;u.advanced_trigger.records[ch]=60;}
 tracegpu_ui_shell_init(&s,&u);s.motion=0;s.drawer=3;s.position=s.target=800;s.menu_page=TG_MENU_TRIGGER+(mode<8?mode:0);
 s.trigger_draft.slots[0].type=TG_TRIG_SEQUENCE;s.trigger_draft.slots[0].length=8;
 for(unsigned i=0;i<8;i++){s.trigger_draft.slots[0].bytes[i]=(uint8_t)(0xa0+i);s.trigger_draft.slots[0].mask[i]=i==3?0:255;}
 s.trigger_draft.slots[1].type=TG_TRIG_PULSE;s.trigger_draft.slots[1].channel=1;
 s.trigger_number=100000;s.trigger_number_target=UI_TRIGGER_MIN;
 if(mode==8||mode==9||mode==10){s.drawer=0;s.position=0;if(mode==9)u.advanced_trigger.state=TG_TRIG_POST;if(mode==10){u.advanced_trigger.state=TG_TRIG_ARMED;u.advanced_trigger.has_trigger=u.advanced_trigger.valid=0;}}
 if(mode==11)s.confirm=UI_TRIGGER_ARM;
 if(mode==12){s.menu_page=TG_MENU_TRIGGER_RESULT;u.advanced_trigger.state=TG_TRIG_INCOMPLETE;u.advanced_trigger.quality=TG_TRIG_Q_PRE_SHORT|TG_TRIG_Q_LOSS;}
 if(mode==13){s.menu_page=TG_MENU_TRIGGER_RESULT;u.advanced_trigger.valid=u.advanced_trigger.has_trigger=0;u.advanced_trigger.state=TG_TRIG_IDLE;}
 if(mode==14){s.menu_page=TG_MENU_TRIGGER_CONDITION;s.trigger_draft.slots[0].type=TG_TRIG_PULSE;s.trigger_draft.slots[0].mode=TG_TRIG_OUTSIDE;s.trigger_draft.slots[0].min_ticks=500000000;s.trigger_draft.slots[0].max_ticks=1000000000;}
 tracegpu_dashboard_draw_uart((uintptr_t)pixels,&u,&rings[0]);tracegpu_dashboard_draw_analysis((uintptr_t)pixels,&u);
 if(mode==8){int x=tracegpu_cursor_pixel(u.advanced_trigger.trigger_tick,1000000,1000000);assert(x>=0&&pixels[170*800+x]==0xffffd54dU);x=tracegpu_cursor_pixel(u.advanced_trigger.condition_start_tick,1000000,1000000);assert(x>=0&&pixels[(TG_PLOT_TOP+2)*800+x]==0xffabc5e2U);}
 tracegpu_ui_shell_draw((uintptr_t)pixels,&s,&u);tg_text((uintptr_t)pixels,242,30,"HOST SAMPLE",0x9bb3ce,0);bmp(argv[1]);
 puts("PASS v2.9 actual C trigger UI preview / sample data only");return 0;
}
