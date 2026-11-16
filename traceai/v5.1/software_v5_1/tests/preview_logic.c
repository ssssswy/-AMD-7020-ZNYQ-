#include "ui_shell.h"
#include "ui_paint.h"
#include "wave_render.h"
#include "capture_session.h"
#include "render_diagnostics.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct{uint32_t before,pixels[800*480],after;} image;
static tracegpu_vector_record records[128];
static tracegpu_vector_stream stream;
static tracegpu_uart_event_ring rings[8];
static tracegpu_wave_plan plan;
void XTime_GetTime(uint64_t *t){*t=0;}
static void put(uint64_t tick,unsigned type,unsigned levels){
 tracegpu_vector_event e={tick,stream.expected_sequence,(uint8_t)type,(uint8_t)(type==TG_VEC_GAP?1:255),(uint8_t)levels};
 tracegpu_vector_record r=tracegpu_vector_pack(e);assert(tracegpu_vector_feed(&stream,&r,sizeof r)==1);
}
static void bmp(const char *path){
 unsigned char h[54]={0x42,0x4d};uint32_t size=54+800*480*4,offset=54,dib=40,w=800,height=480;uint16_t planes=1,bits=32;
 memcpy(h+2,&size,4);memcpy(h+10,&offset,4);memcpy(h+14,&dib,4);memcpy(h+18,&w,4);memcpy(h+22,&height,4);memcpy(h+26,&planes,2);memcpy(h+28,&bits,2);
 FILE *f=fopen(path,"wb");assert(f);assert(fwrite(h,1,54,f)==54);
 for(unsigned y=480;y;y--)assert(fwrite(image.pixels+(y-1)*800,4,800,f)==800);
 assert(!fclose(f));
}
int main(int argc,char **argv){
 tracegpu_ui_state u={0};tracegpu_ui_shell shell;assert(argc==3);unsigned mode=(unsigned)atoi(argv[2]);
 image.before=0x12345678;image.after=0x98765432;assert(!tracegpu_dashboard_init());
 tracegpu_vector_stream_init(&stream,records,128,0,0);
 put(1000000,TG_VEC_INIT,255);unsigned levels=255;
 for(unsigned i=1;i<50;i++){levels^=1U<<(i%8);put(1000000+i*10000,TG_VEC_CHANGE,levels);}
 if(mode==4||mode==17){put(1500000,TG_VEC_GAP,0);put(1650000,TG_VEC_RESYNC,levels);}
 put(2000000,TG_VEC_END,levels);
 u.logic_mode=u.dual_channel=u.quad_channel=1;u.channel_enabled_mask=15;u.logic_capture_mask=255;u.logic_channel_count=8;u.time_window_ticks=1000000;
 u.view_anchor_ticks=u.latest_event_ticks=2000000;u.logic_begin=1000000;u.logic_end=2000000;u.logic_records=stream.history.count;
 u.logic_gaps=(uint32_t)stream.gaps;u.lost_events=u.logic_gaps;u.drain_state=2;
 for(unsigned i=0;i<8;i++){tracegpu_uart_ring_vector_view(&rings[i],&stream,i);u.channel_ring[i]=&rings[i];}
 for(unsigned i=0;i<8;i++)u.logic_rows[u.logic_count++]=tracegpu_vector_unpack(tracegpu_vector_oldest(&stream.history,stream.history.count-8+i));
 tracegpu_ui_shell_init(&shell,&u);shell.protocol_mode=3;shell.motion=0;
 if(mode==1)u.data_page=shell.data_page=4;
 if(mode==2){shell.drawer=3;shell.position=shell.target=800;shell.menu_page=TG_MENU_PROTOCOL;}
 if(mode==3){shell.drawer=1;shell.position=shell.target=256;}
 if(mode==5||mode==6){shell.drawer=3;shell.position=shell.target=800;shell.menu_page=TG_MENU_LOGIC_DECODE;shell.decode_config[0].protocol=mode==5?0:2;}
 if(mode==7){shell.drawer=3;shell.position=shell.target=800;shell.menu_page=TG_MENU_LOGIC_EXPORT;u.export_state=1;u.export_size=22704;}
 if(mode==8){u.data_page=shell.data_page=4;u.logic_results=1;u.decode_valid=1;u.decode_progress=100;u.decode_total=u.decode_count=8;for(unsigned j=0;j<8;j++)u.decode_rows[j]=(tracegpu_decode_row){1000000+10000*j,1005000+10000*j,0x30+j,0,0,0};}
 if(mode==9){u.logic_visible=5;u.logic_alias[0]=2;u.logic_alias[2]=1;}
 if(mode==10){u.decode_annotation_count[0]=u.decode_annotation_count[1]=2;for(unsigned j=0;j<2;j++){u.decode_annotations[0][j]=(tracegpu_decode_row){1200000+150000*j,1210000+150000*j,0x50+j,0,1,0};u.decode_annotations[1][j]=(tracegpu_decode_row){1400000+150000*j,1410000+150000*j,0x55+j,0,0,2};}}
 if(mode==11)u.view_mode=1;
 if(mode==12)u.view_mode=2;
 if(mode==13){u.view_mode=3;u.selected_channel=shell.selected=7;u.logic_alias[7]=shell.logic_alias[7]=1;}
 if(mode==14){shell.drawer=3;shell.position=shell.target=800;shell.menu_page=TG_MENU_TRIGGER_LEVELS;shell.trigger_draft.level_mask=0xff;shell.trigger_draft.level_value=0xaa;}
 if(mode==15){shell.drawer=3;shell.position=shell.target=800;shell.menu_page=TG_MENU_TRIGGER_RESULT;u.advanced_trigger.valid=u.advanced_trigger.has_trigger=1;u.advanced_trigger.state=TG_TRIG_READY;u.advanced_trigger.source_mask=0x80;u.advanced_trigger.predicate_mask=1;u.advanced_trigger.records[0]=stream.history.count;u.advanced_trigger.actual_pre_ticks[0]=500000;u.advanced_trigger.actual_post_ticks[0]=500000;}
 if(mode>=16&&mode<=19){
  static tracegpu_bus_task bus;tracegpu_logic_trace_view view;
  for(unsigned group=0;group<2;group++){tracegpu_bus_config_default(&shell.bus_config[group],group);shell.bus_config[group].enabled=1;shell.bus_config[group].width=mode==17?8:4;shell.bus_config[group].binary=mode==17;}
  if(mode==17){shell.bus_config[1].map[0]=7;shell.bus_config[1].map[3]=4;}
  assert(!tracegpu_logic_view_bind(&view,&stream.history,100,2,1));assert(!tracegpu_bus_begin(&bus,&view,shell.bus_config,1,1000000,2000000));
  while(!tracegpu_bus_step(&bus,7));
  u.bus_mode=shell.bus_mode=1;u.bus_state=bus.state;u.bus_progress=100;memcpy(u.bus_config,shell.bus_config,sizeof u.bus_config);memcpy(u.bus_cells,bus.cells,sizeof u.bus_cells);
  u.bus_point_valid=1;for(unsigned g=0;g<2;g++){tracegpu_bus_point(&view,&u.bus_config[g],1250000,&u.bus_point[g][0]);tracegpu_bus_point(&view,&u.bus_config[g],1750000,&u.bus_point[g][1]);}
  if(mode==18){shell.drawer=3;shell.position=shell.target=800;shell.menu_page=TG_MENU_LOGIC_BUS;}
  if(mode==19){u.bus_state=TG_BUS_RUNNING;u.bus_progress=37;}
 }
 if(mode>=20&&mode<=24){
  tracegpu_quality_input quality={0};quality.capture_id=201;quality.generation=3;quality.identity_valid=quality.capture_started=quality.stopped=1;
  quality.scope=TG_QUALITY_SCOPE_SESSION;quality.required_mask=TG_QUALITY_ACQUISITION_MASK;
  for(unsigned k=0;k<TG_QUALITY_METRICS;k++)quality.counter[k]=(tracegpu_quality_counter){0,1,1,TG_QUALITY_SCOPE_SESSION,0};
  quality.counter[TG_QUALITY_HISTORY_EVICTED].value=47000;quality.counter[TG_QUALITY_DECODE_OMITTED].value=123;
  quality.counter[TG_QUALITY_VIEW_HIDDEN]=(tracegpu_quality_counter){0,0,0,TG_QUALITY_SCOPE_VIEW,0};
  if(mode==21){quality.counter[TG_QUALITY_PL_DROP].value=17;quality.counter[TG_QUALITY_RAW_GAP].value=3;u.overflows=17;u.logic_gaps=u.lost_events=3;}
  if(mode==23){quality.source_issue_flags=TG_QUALITY_SOURCE_CAPTURE_ISSUE;u.drain_state=CAPTURE_INCOMPLETE;u.drain_reason=TG_DRAIN_TIMEOUT;}
  if(mode==24){quality.stopped=0;u.drain_state=CAPTURE_DRAINING;shell.draining=1;}
  tracegpu_quality_project(&quality,&u.quality);
  shell.drawer=3;shell.position=shell.target=800;shell.menu_page=TG_MENU_QUALITY;
  if(mode==22){shell.menu_page=TG_MENU_DIAGNOSTICS;shell.show_cpu=shell.show_gpu=1;u.cpu_percent_valid=u.gpu_percent_valid=u.render_metrics_valid=1;u.cpu_percent=53;u.gpu_percent=7;
   u.submit_fps=34;u.stream_fps=45;u.render_prepare_us=13100;u.gpu_render_us=730;u.gpu_convert_us=1100;u.render_wait_us=9100;u.render_frame_us=23200;
   u.render_reason=tracegpu_render_explain(2,1,1,325,0,0,0);
  }
 }
 if(mode>=25&&mode<=32){
  unsigned protocol=(mode==27||mode==28)?TG_DECODE_MODBUS:mode==30?TG_DECODE_RS485:TG_DECODE_CAN;
  tracegpu_ui_decode_protocol(&shell,protocol);u.data_page=shell.data_page=4;u.logic_results=shell.logic_results=1;
  u.decode_valid=1;u.decode_progress=100;u.decode_total=128;u.decode_count=8;
  for(unsigned i=0;i<8;i++){
   tracegpu_decode_row row={1200000+i*50000,1210000+i*50000,protocol==TG_DECODE_CAN?0x1abc000+i:protocol==TG_DECODE_MODBUS?0x141:0x30+i,0,(uint8_t)protocol,7};
   u.decode_rows[i]=row;u.decode_bus_valid[i]=1;
   if(protocol==TG_DECODE_CAN){tracegpu_can_decoded *f=&u.decode_bus[i].can;f->start=row.start;f->end=row.end;f->id=row.value;f->extended=1;f->dlc=f->length=8;f->ack=f->complete=1;f->crc_received=f->crc_calculated=0x1234;for(unsigned j=0;j<8;j++)f->data[j]=(uint8_t)(i+j);}
   if(protocol==TG_DECODE_MODBUS){tracegpu_modbus_frame *f=&u.decode_bus[i].modbus;f->start=row.start;f->end=row.end;f->address=1;f->function=0x41;f->length=256;f->observed=256;for(unsigned j=0;j<254;j++)f->adu[j]=(uint8_t)j;f->adu[0]=1;f->adu[1]=0x41;f->crc_received=f->crc_computed=tracegpu_modbus_crc16(f->adu,254);f->adu[254]=(uint8_t)f->crc_received;f->adu[255]=(uint8_t)(f->crc_received>>8);f->valid=f->complete=f->crc_checked=f->crc_ok=1;}
  }
  u.decode_detail.config=shell.decode_config[0];u.decode_detail.row=u.decode_rows[0];u.decode_detail.bus=u.decode_bus[0];u.decode_detail_state=TG_REVIEW_DONE;
  if(mode==26||mode==28){shell.drawer=4;shell.position=shell.target=266;if(mode==28)shell.decode_detail_page=31;}
  if(mode==29){shell.drawer=3;shell.position=shell.target=800;shell.menu_page=TG_MENU_LOGIC_DECODE;}
  if(mode==31){shell.drawer=3;shell.position=shell.target=800;shell.menu_page=TG_MENU_PROTOCOL;}
  if(mode==32){u.data_page=shell.data_page=0;u.decode_annotation_count[0]=2;u.decode_annotations[0][0]=u.decode_rows[0];u.decode_annotations[0][1]=u.decode_rows[1];u.marker_valid=1;u.marker_channel=7;u.marker_ticks=u.decode_rows[0].start;}
 }
 tracegpu_wave_build(&plan,&u,&rings[0]);assert(plan.dual&&(u.bus_mode?!plan.count:plan.count));
 for(unsigned i=0;i<plan.count;i++)assert(tracegpu_render_rect_valid(&plan.rects[i]));
 tracegpu_dashboard_draw_uart((uintptr_t)image.pixels,&u,&rings[0]);
 tracegpu_dashboard_draw_analysis((uintptr_t)image.pixels,&u);tracegpu_ui_shell_draw((uintptr_t)image.pixels,&shell,&u);
 #ifdef TG_CPU_PARTIAL_TEST
 cpu_partial_fixture(&u,&shell,&rings[0]);
 #endif
 /* Keep the host-only watermark above header metrics, never over CPU/GPU. */
 tg_text((uintptr_t)image.pixels,668,14,"HOST SAMPLE",0x9bb3ce,0);
 assert(image.before==0x12345678&&image.after==0x98765432);bmp(argv[1]);
 puts("LOGIC_C_RENDER_PASS geometry/framebuffer bounds; synthetic host trace, not board FPS");return 0;
}
