#define _POSIX_C_SOURCE 200809L
#include "wave_render.h"
#include "wave_lod.h"
#include "capture_vector_stream.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void tracegpu_v35_reference_build(tracegpu_wave_plan *,const tracegpu_ui_state *,const tracegpu_uart_event_ring *);
void tracegpu_v35_reference_cache_enable(int,uint32_t,uint32_t);
static tracegpu_vector_record storage[262145U];
static tracegpu_vector_stream capture;
static tracegpu_uart_event_ring lanes[8];
static tracegpu_ui_state ui;
static struct {uint32_t first[8];tracegpu_wave_plan plan;uint32_t last[8];} candidate;
static tracegpu_wave_plan original;
static uint32_t capture_id=71,generation=3;
static unsigned plans,cache_checks,gate_checks,service_checks,special_checks,mutation_checks;
static const char *scenario;

static void put(unsigned type,uint64_t tick,unsigned levels){
 tracegpu_vector_event e={tick,capture.expected_sequence,(uint8_t)type,
   (uint8_t)(type==TG_VEC_GAP?1U:capture.history.capture_mask),(uint8_t)(type==TG_VEC_GAP?0U:levels)};
 tracegpu_vector_record r=tracegpu_vector_pack(e);
 assert(tracegpu_vector_feed(&capture,&r,sizeof r)==1);
}
static void views(void){
 for(unsigned ch=0;ch<8;ch++){tracegpu_uart_ring_vector_view(&lanes[ch],&capture,ch);ui.channel_ring[ch]=&lanes[ch];}
}
static void fixture(unsigned capacity,unsigned count,unsigned mask,int special,int ended){
 memset(&ui,0,sizeof ui);memset(lanes,0,sizeof lanes);
 tracegpu_wave_cache_enable(0,++capture_id,++generation);tracegpu_wave_lod_reset();tracegpu_wave_lod_enable(1);
 tracegpu_vector_stream_init(&capture,storage,capacity,0,0);assert(!tracegpu_vector_stream_configure(&capture,mask));
 unsigned levels=mask,seed=0x5319U;
 put(TG_VEC_INIT,1000,levels);
 for(unsigned n=1;n<count;n++){
  uint64_t tick=1000U+(uint64_t)n*11U;unsigned type=TG_VEC_CHANGE;
  if(ended&&n==count-1U)type=TG_VEC_END;
  else if(capture.needs_resync)type=TG_VEC_RESYNC;
  else if(special&&n%2003U==0U&&n+2U<count)type=TG_VEC_GAP;
  else if(special&&n%337U==0U)type=TG_VEC_WATERMARK;
  if(type==TG_VEC_CHANGE||type==TG_VEC_RESYNC){
   seed=seed*1664525U+1013904223U;unsigned change=(1U+(seed%255U))&mask;
   if(!change)change=mask&(~mask+1U);levels^=change;
  }
  put(type,tick,levels);
 }
 ui.logic_mode=ui.dual_channel=ui.quad_channel=1;ui.logic_channel_count=8;ui.logic_capture_mask=(uint8_t)mask;
 ui.channel_enabled_mask=255;ui.logic_visible=255;ui.selected_channel=7;
 ui.time_window_ticks=(uint32_t)(capture.last_tick+11U);ui.view_anchor_ticks=capture.last_tick+11U;
 views();
}
static void check_plan(void){
 for(unsigned n=0;n<8;n++)candidate.first[n]=candidate.last[n]=0xcafe69a5U;
 memset(&candidate.plan,0x5a,sizeof candidate.plan);memset(&original,0xa5,sizeof original);
 tracegpu_wave_build(&candidate.plan,&ui,&lanes[ui.selected_channel]);
 tracegpu_v35_reference_cache_enable(0,capture_id,generation);
 tracegpu_v35_reference_build(&original,&ui,&lanes[ui.selected_channel]);
 if(memcmp(&candidate.plan,&original,sizeof original)){
  tracegpu_wave_lod_metrics m=tracegpu_wave_lod_get_metrics();
  fprintf(stderr,"LOD_PLAN_DIFFERENCE scenario=%s plans=%u count=%u capacity=%u gate=%u ready=%u got=%u/%u/%u expected=%u/%u/%u\n",
    scenario,plans,capture.history.count,capture.history.capacity,m.last_gate,m.ready,
    candidate.plan.count,candidate.plan.edges,candidate.plan.pixels,original.count,original.edges,original.pixels);abort();
 }
 for(unsigned n=0;n<8;n++)assert(candidate.first[n]==0xcafe69a5U&&candidate.last[n]==0xcafe69a5U);
 assert(candidate.plan.count<=TRACEGPU_WAVE_MAX_RECTS&&!candidate.plan.overflow);
 plans++;
}
static void prepare(void){
 for(unsigned step=0;step<20000U;step++){
  tracegpu_wave_lod_metrics before=tracegpu_wave_lod_get_metrics();
  int rc=tracegpu_wave_lod_service(&capture.history,capture_id,generation,4);
  tracegpu_wave_lod_metrics after=tracegpu_wave_lod_get_metrics();
  assert(rc>=0&&after.service_records-before.service_records<=4U*TRACEGPU_LOD_LEAF_RECORDS);
  assert(after.service_leaves-before.service_leaves<=4U&&after.init_nodes-before.init_nodes<=8U);service_checks++;
  if(after.ready)return;
 }
 assert(!"budgeted index failed to become ready");
}
static void cache_suite(void){
 scenario="immutable_cache_and_diagnostic_only";fixture(65536,64000,255,1,1);prepare();
 tracegpu_wave_cache_enable(1,capture_id,generation);check_plan();
 uint32_t hits=tracegpu_wave_cache_hits();
 for(unsigned step=0;step<12;step++){
  ui.byte_count+=59;ui.frame_errors++;ui.parity_errors+=2;ui.logic_rows[0].tick++;
  ui.cpu_percent_valid=ui.gpu_percent_valid=ui.render_metrics_valid=1;
  ui.cpu_percent=(uint8_t)(step*7U);ui.gpu_percent=(uint8_t)(step*3U);
  ui.render_prepare_us+=57;ui.render_frame_us+=91;ui.gpu_render_us++;
  check_plan();assert(tracegpu_wave_cache_hits()==++hits);cache_checks++;
 }
 scenario="viewport_and_layout_invalidate";
 for(unsigned step=0;step<13;step++){
  uint32_t misses=tracegpu_wave_cache_misses();
  if(step==0)ui.time_window_ticks/=2;
  else if(step==1)ui.view_offset_ticks=-1337;
  else if(step==2)ui.view_anchor_ticks-=177;
  else if(step==3)ui.logic_visible=0xa5;
  else if(step==4)ui.view_mode=2;
  else if(step==5)ui.selected_channel=5;
  else if(step==6)ui.channel_enabled_mask=0x0f;
  else if(step==7)ui.dual_channel=0;
  else if(step==8)ui.quad_channel=0;
  else if(step==9)ui.logic_mode=0,ui.spi_mode=1;
  else if(step==10)capture.history.capacity--;
  else if(step==11)capture.history.evicted++;
  else ui.logic_channel_count=4;
  check_plan();assert(tracegpu_wave_cache_misses()==misses+1U);cache_checks++;
 }
 scenario="immutable_authorization_revoked";
 tracegpu_wave_cache_enable(0,capture_id,generation);hits=tracegpu_wave_cache_hits();check_plan();check_plan();
 assert(tracegpu_wave_cache_hits()==hits);cache_checks++;
 tracegpu_wave_cache_enable(1,capture_id,generation);check_plan();
 uint32_t misses=tracegpu_wave_cache_misses();tracegpu_wave_cache_enable(1,++capture_id,generation);check_plan();
 assert(tracegpu_wave_cache_misses()==misses+1U);cache_checks++;
 misses=tracegpu_wave_cache_misses();tracegpu_wave_cache_enable(1,capture_id,++generation);check_plan();
 assert(tracegpu_wave_cache_misses()==misses+1U);cache_checks++;
 tracegpu_wave_cache_enable(0,capture_id,generation);
}
static void gate_suite(void){
 scenario="cold_and_disabled";fixture(65536,64000,255,0,1);check_plan();
 assert(tracegpu_wave_lod_get_metrics().last_gate==TG_WAVE_LOD_COLD);gate_checks++;
 prepare();check_plan();assert(tracegpu_wave_lod_get_metrics().last_gate==TG_WAVE_LOD_ACTIVE);gate_checks++;
 tracegpu_wave_lod_enable(0);check_plan();assert(tracegpu_wave_lod_get_metrics().last_gate==TG_WAVE_LOD_OFF);gate_checks++;
 tracegpu_wave_lod_enable(1);prepare();
 scenario="zoom_density_gate";ui.time_window_ticks=2200;check_plan();
 assert(tracegpu_wave_lod_get_metrics().last_gate==TG_WAVE_LOD_SPARSE);gate_checks++;
 scenario="medium_history_gate";fixture(16384,16000,255,0,1);prepare();check_plan();
 assert(tracegpu_wave_lod_get_metrics().last_gate==TG_WAVE_LOD_SPARSE);gate_checks++;
 scenario="capacity_guard";fixture(262145U,64000U,255,0,1);
 (void)tracegpu_wave_lod_service(&capture.history,capture_id,generation,4);check_plan();
 assert(tracegpu_wave_lod_get_metrics().last_gate==TG_WAVE_LOD_UNSUPPORTED);gate_checks++;
 scenario="bus_mode_passthrough";fixture(65536,64000,255,0,1);prepare();ui.bus_mode=1;check_plan();gate_checks++;
}
static void equivalence_suite(void){
 const unsigned masks[]={255,128,0xa5,1};
 for(unsigned mi=0;mi<4;mi++)for(unsigned special=0;special<2;special++){
  scenario="dense_eight_lane_exact";fixture(262144,200000,masks[mi],(int)special,1);prepare();
  for(unsigned layout=0;layout<4;layout++)for(int offset=-3;offset<=3;offset++){
   ui.view_mode=(uint8_t)layout;ui.view_offset_ticks=offset;
   for(unsigned zoom=0;zoom<3;zoom++){
    ui.time_window_ticks=(uint32_t)((capture.last_tick+11U)/(zoom==0?1U:zoom==1?8U:200U));
    check_plan();special_checks++;
   }
  }
  ui.logic_visible=0xa5;ui.view_mode=0;ui.time_window_ticks=1;ui.view_offset_ticks=0;check_plan();special_checks++;
 }
 scenario="live_append_ready_revocation";fixture(65536,64000,255,1,0);prepare();check_plan();
 for(unsigned step=0;step<16;step++){
  put(TG_VEC_CHANGE,capture.last_tick+11U,capture.levels^(1U<<(step%8U)));views();ui.view_anchor_ticks=capture.last_tick;
  check_plan();assert(tracegpu_wave_lod_get_metrics().last_gate==TG_WAVE_LOD_COLD);mutation_checks++;
  prepare();check_plan();mutation_checks++;
 }
 scenario="physical_wrap_eviction_and_boundary";fixture(4096,8000,255,1,0);prepare();check_plan();
 for(unsigned step=0;step<100;step++){
  unsigned type=step==23?TG_VEC_GAP:step==24?TG_VEC_RESYNC:step%13U==0U?TG_VEC_WATERMARK:TG_VEC_CHANGE;
  unsigned levels=capture.levels;
  if(type==TG_VEC_CHANGE||type==TG_VEC_RESYNC)levels^=1U<<(step%8U);
  put(type,capture.last_tick+11U,levels);views();ui.view_anchor_ticks=capture.last_tick;check_plan();prepare();check_plan();mutation_checks+=2;
 }
 put(TG_VEC_END,capture.last_tick+11U,capture.levels);views();ui.view_anchor_ticks=capture.last_tick+1000U;prepare();check_plan();mutation_checks++;
 scenario="same_tick_watermark_upper_boundary";fixture(65536,2,255,0,0);
 for(unsigned n=0;n<60000U;n++)put(TG_VEC_WATERMARK,capture.last_tick,capture.levels);
 put(TG_VEC_END,capture.last_tick,capture.levels);views();prepare();
 ui.time_window_ticks=100;ui.view_anchor_ticks=capture.last_tick;
 for(int offset=-2;offset<=2;offset++){ui.view_offset_ticks=offset;check_plan();special_checks++;}
 scenario="same_backing_new_capture";fixture(65536,64000,255,0,1);prepare();check_plan();mutation_checks++;
}
static uint64_t ns(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000000000U+(uint64_t)t.tv_nsec;}
static void performance(const char *directory){
 char path[2048];assert(snprintf(path,sizeof path,"%s/raw.csv",directory)>0);FILE *f=fopen(path,"wb");assert(f);
 fputs("records,scenario,block,variant,step,plan_ns,gate,cache_hits,plan_oracle_checked\n",f);
 const unsigned sizes[]={16000,64000,200000};
 for(unsigned si=0;si<3;si++)for(unsigned menu=0;menu<2;menu++){
  scenario="timed_changed_view_or_ready_cache";fixture(262144,sizes[si],255,1,1);
  uint64_t cold_start=ns();prepare();uint64_t cold=ns()-cold_start;
  printf("LOD_COLD_PC_US records=%u scenario=%s service_loop_us=%.3f\n",sizes[si],menu?"menu_cached":"pan_zoom",cold/1000.0);
  tracegpu_wave_cache_enable(1,capture_id,generation);tracegpu_v35_reference_cache_enable(1,capture_id,generation);
  uint32_t full=(uint32_t)(capture.last_tick+11U);
  for(unsigned block=0;block<4;block++)for(unsigned step=0;step<48;step++){
   int lod=block==1||block==2;ui.time_window_ticks=menu?full:(step&1U)?full:full/2U;ui.view_offset_ticks=menu?0:(int64_t)(step%7U);
   ui.byte_count+=31;ui.render_prepare_us+=11;
   uint64_t start=ns();
   if(lod)tracegpu_wave_build(&candidate.plan,&ui,&lanes[7]);else tracegpu_v35_reference_build(&candidate.plan,&ui,&lanes[7]);
   uint64_t elapsed=ns()-start;
   if(lod)tracegpu_v35_reference_build(&original,&ui,&lanes[7]);else tracegpu_wave_build(&original,&ui,&lanes[7]);
   assert(!memcmp(&candidate.plan,&original,sizeof original));
   if(step>=8U)fprintf(f,"%u,%s,%u,%s,%u,%llu,%u,%u,1\n",sizes[si],menu?"menu_cached":"pan_zoom",block,lod?"lod":"baseline",step,
    (unsigned long long)elapsed,tracegpu_wave_lod_get_metrics().last_gate,tracegpu_wave_cache_hits());
  }
 }
 assert(!fclose(f));puts("WAVE_LOD_V36_PERF_PASS same_binary_ABBA=1 cpu_plan_only=1 board_fps=0");
}
int main(int argc,char **argv){
 if(argc==3&&!strcmp(argv[1],"--perf")){performance(argv[2]);return 0;}
 assert(argc==1);cache_suite();gate_suite();equivalence_suite();
 assert(plans&&cache_checks&&gate_checks&&service_checks&&special_checks&&mutation_checks);
 printf("WAVE_LOD_V36_PASS plans=%u cache_checks=%u gate_checks=%u bounded_service_checks=%u special_layout_cases=%u append_eviction_cases=%u oracle=v3.5_byte_exact guards=PASS board_tested=0\n",
  plans,cache_checks,gate_checks,service_checks,special_checks,mutation_checks);return 0;
}
