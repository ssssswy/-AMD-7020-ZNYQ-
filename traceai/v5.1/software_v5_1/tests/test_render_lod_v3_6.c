/* Reuse the maintained real compositor + intentionally non-coherent cache/DDR
 * model. The runner changes ONLY the legacy suite's final entry-point name. */
#include "wave_render.h"
static void checked_wave_build(tracegpu_wave_plan *,const tracegpu_ui_state *,const tracegpu_uart_event_ring *);
#define tracegpu_wave_build checked_wave_build
#include "test_render_gpu_partial_embedded.c"
#undef tracegpu_wave_build

void tracegpu_v35_reference_build(tracegpu_wave_plan *,const tracegpu_ui_state *,const tracegpu_uart_event_ring *);
void tracegpu_v35_reference_cache_enable(int,uint32_t,uint32_t);
static tracegpu_wave_plan independent_plan;
static unsigned oracle_plans,lod_active,lod_cached,lod_sparse,lod_cold,lod_off;
static tracegpu_vector_record large_records[262144U];
static tracegpu_vector_stream large_stream;
static tracegpu_uart_event_ring large_lanes[8];
static tracegpu_ui_state large_ui;
static tracegpu_ui_shell large_shell;

static void checked_wave_build(tracegpu_wave_plan *p,const tracegpu_ui_state *u,const tracegpu_uart_event_ring *r){
 tracegpu_wave_build(p,u,r);tracegpu_v35_reference_cache_enable(0,0,0);
 tracegpu_v35_reference_build(&independent_plan,u,r);
 if(memcmp(p,&independent_plan,sizeof *p)){
  fprintf(stderr,"RENDER_LOD_PLAN_DIFFERENCE plan=%u count=%u original_count=%u gate=%u\n",oracle_plans,p->count,independent_plan.count,tracegpu_wave_lod_get_metrics().last_gate);abort();
 }
 unsigned gate=tracegpu_wave_lod_get_metrics().last_gate;
 lod_active+=gate==TG_WAVE_LOD_ACTIVE;lod_cached+=gate==TG_WAVE_LOD_CACHE;
 lod_sparse+=gate==TG_WAVE_LOD_SPARSE;lod_cold+=gate==TG_WAVE_LOD_COLD;lod_off+=gate==TG_WAVE_LOD_OFF;
 oracle_plans++;
}
static void large_put(unsigned type,uint64_t tick,unsigned levels){
 tracegpu_vector_event e={tick,large_stream.expected_sequence,(uint8_t)type,(uint8_t)(type==TG_VEC_GAP?1:255),(uint8_t)(type==TG_VEC_GAP?0:levels)};
 tracegpu_vector_record record=tracegpu_vector_pack(e);assert(tracegpu_vector_feed(&large_stream,&record,sizeof record)==1);
}
static void large_views(void){
 for(unsigned ch=0;ch<8;ch++){tracegpu_uart_ring_vector_view(&large_lanes[ch],&large_stream,ch);large_ui.channel_ring[ch]=&large_lanes[ch];}
 large_ui.logic_records=large_stream.history.count;large_ui.logic_begin=1000;large_ui.logic_end=large_stream.last_tick;
 large_ui.latest_event_ticks=large_stream.last_tick;
}
static void large_prepare(uint32_t id){
 for(unsigned n=0;n<30000;n++){
  assert(tracegpu_wave_lod_service(&large_stream.history,id,3,4)>=0);
  if(tracegpu_wave_lod_get_metrics().ready)return;
 }
 assert(!"large LOD did not become ready");
}
static void large_fixture(unsigned count,uint32_t id){
 tracegpu_wave_lod_reset();tracegpu_wave_lod_enable(1);tracegpu_wave_cache_enable(0,id,3);
 memset(&large_ui,0,sizeof large_ui);memset(large_lanes,0,sizeof large_lanes);
 tracegpu_vector_stream_init(&large_stream,large_records,262144,0,0);assert(!tracegpu_vector_stream_configure(&large_stream,255));
 large_put(TG_VEC_INIT,1000,255);unsigned levels=255;
 for(unsigned n=1;n<count;n++){
  unsigned type=n%2003U==0U?TG_VEC_GAP:large_stream.needs_resync?TG_VEC_RESYNC:n%337U==0U?TG_VEC_WATERMARK:TG_VEC_CHANGE;
  if(type==TG_VEC_CHANGE||type==TG_VEC_RESYNC)levels^=1U<<(n%8U);
  large_put(type,1000U+(uint64_t)n*11U,levels);
 }
 large_ui.logic_mode=large_ui.dual_channel=large_ui.quad_channel=1;large_ui.logic_capture_mask=255;large_ui.logic_channel_count=8;
 large_ui.channel_enabled_mask=large_ui.logic_visible=255;large_ui.selected_channel=7;large_ui.drain_state=2;
 large_ui.time_window_ticks=(uint32_t)(large_stream.last_tick+11U);large_ui.view_anchor_ticks=large_stream.last_tick+11U;
 large_views();tracegpu_ui_shell_init(&large_shell,&large_ui);large_shell.protocol_mode=3;large_shell.motion=0;
 large_prepare(id);tracegpu_wave_cache_enable(1,id,3);reset_surfaces();
}
int main(int argc,char **argv){
 assert(argc==2);assert(!gpu_suite_main(argc,argv));unsigned prior_frames=frames,large_frames=0;
 for(unsigned dataset=0;dataset<2;dataset++){
  uint32_t id=301U+dataset;large_fixture(dataset?200000U:64000U,id);
  uint32_t full_window=large_ui.time_window_ticks;
  for(unsigned step=0;step<24;step++){
   if(step==2){large_ui.byte_count+=73;large_ui.cpu_percent_valid=1;large_ui.cpu_percent=59;}
   if(step>=3&&step<=5){large_shell.drawer=1;large_shell.position=(int)((step-2U)*64U);large_shell.target=256;}
   if(step==6){large_shell.drawer=0;large_shell.position=0;}
   if(step>=6&&step<=11){large_ui.time_window_ticks=(step&1U)?full_window:full_window/2U;large_ui.view_offset_ticks=-(int64_t)(step*131U);}
   if(step>=12&&step<=15){
    touch.online=1;touch.count=5;touch.sequence++;
    for(unsigned q=0;q<5;q++){touch.point[q].id=(uint8_t)q;touch.point[q].x=(uint16_t)(100+q*110U+step);touch.point[q].y=(uint16_t)(125+q*47U);}
   }
   if(step==16)touch.count=0;
   if(step==17)large_ui.view_mode=1;
   if(step==18)large_ui.view_mode=2;
   if(step==19)large_ui.logic_visible=0xa5;
   if(step==20){
    tracegpu_wave_cache_enable(0,id,3);large_put(TG_VEC_CHANGE,large_stream.last_tick+11U,large_stream.levels^128U);
    large_views();large_ui.view_anchor_ticks=large_stream.last_tick;large_ui.time_window_ticks=full_window;large_ui.view_mode=0;large_ui.logic_visible=255;
   }
   if(step==21)tracegpu_wave_lod_enable(0);
   if(step==22){tracegpu_wave_lod_enable(1);large_prepare(id);tracegpu_wave_cache_enable(1,id,3);}
   if(step==23)large_ui.data_page=large_shell.data_page=4;
   uint32_t old_hits=tracegpu_wave_cache_hits();checked_wave_build(&plan,&large_ui,&large_lanes[7]);
   if(step==2)assert(tracegpu_wave_cache_hits()==old_hits+1U);
   render_frame(&large_ui,&large_shell,&large_lanes[7],2000U+large_frames,(step%3U)?((step&1U)?2:1):0,step%7U==0U);
   large_frames++;
  }
 }
 assert(large_frames==48U&&frames==prior_frames+large_frames&&lod_active&&lod_cached&&lod_cold&&lod_off);
 printf("RENDER_LOD_V36_PASS legacy_frames=%u large_frames=%u total_frames=%u independent_plan_checks=%u active=%u cached=%u sparse=%u cold=%u off=%u pixel_differences=0 cpu_ddr_separate=1 front_untouched=1 gpu_cpu_full_fallback=1 guards=PASS board_tested=0\n",
  prior_frames,large_frames,frames,oracle_plans,lod_active,lod_cached,lod_sparse,lod_cold,lod_off);return 0;
}
