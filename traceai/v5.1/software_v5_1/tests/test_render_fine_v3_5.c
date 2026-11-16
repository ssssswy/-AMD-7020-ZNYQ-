#include "render_cpu.h"
#include <stdint.h>
static void cpu_partial_fixture(tracegpu_ui_state *,tracegpu_ui_shell *,const tracegpu_uart_event_ring *);
#define TG_CPU_PARTIAL_TEST 1
#define main fine_fixture_main
#include "preview_logic.c"
#undef main

/* The reference uses the real full compositor; the candidate uses both actual
 * alternating canvases. No production painter or tile expectation is mocked. */
static struct {uint32_t before;uint32_t pixels[800*480] __attribute__((aligned(32)));uint32_t after;} surface[2];
static uint32_t reference[800*480],held_front[800*480];
static tracegpu_ui_state saved_ui,live_ui;
static tracegpu_ui_shell saved_shell;
static tracegpu_wave_plan saved_plan;
static tracegpu_touch_frame touch;
static unsigned front,back,mode_index,frames,fine_assertions,zero_assertions,full_assertions,frozen_frames;
static unsigned max_single_lane_tiles,max_rx_tiles,max_frozen_tiles;
static uint64_t total_tiles,total_flush;
static const char *scenario;
enum {ANY=0,PARTIAL=1,ZERO=2,FULL=3,SINGLE=4,RX=5,FROZEN=6};

static void flush_check(uintptr_t address,uint32_t bytes){
 assert(!(address&31U)&&!(bytes&31U)&&bytes&&bytes<=3200U);
 assert(address>=(uintptr_t)surface[back].pixels&&address+bytes<=(uintptr_t)(surface[back].pixels+800*480));
 assert(address<(uintptr_t)surface[front].pixels||address>=(uintptr_t)(surface[front].pixels+800*480));
}
static void present(tracegpu_ui_state *u,tracegpu_ui_state *live,tracegpu_ui_shell *s,const tracegpu_uart_event_ring *r,int forced,unsigned expect){
 tracegpu_clip_reset();tracegpu_dashboard_set_wave_plan(&plan);
 tracegpu_dashboard_draw_uart((uintptr_t)reference,u,r);tracegpu_dashboard_set_wave_plan(0);
 tracegpu_dashboard_draw_analysis((uintptr_t)reference,u);tracegpu_ui_shell_draw((uintptr_t)reference,s,live);
 tracegpu_dashboard_draw_touch_points((uintptr_t)reference,&touch);
 back=front^1U;memcpy(held_front,surface[front].pixels,sizeof held_front);
 tracegpu_dashboard_cpu_latch(u,r);
 assert(!tracegpu_cpu_begin(back,front,(uintptr_t)surface[back].pixels,u,live,s,&touch,&plan,forced));
 assert(tracegpu_cpu_confirm(back)<0);
 int rc=0;unsigned polls=0;while(!rc){rc=tracegpu_cpu_step(front,flush_check);assert(rc>=0&&++polls<100000U);}
 assert(tracegpu_cpu_confirm(front)<0);assert(!memcmp(held_front,surface[front].pixels,sizeof held_front));
 for(unsigned p=0;p<800*480;p++)if(surface[back].pixels[p]!=reference[p]){
  fprintf(stderr,"FINE_PIXEL_DIFFERENCE mode=%u scenario=%s frame=%u x=%u y=%u got=%08x expected=%08x\n",mode_index,scenario,frames,p%800U,p/800U,surface[back].pixels[p],reference[p]);abort();
 }
 const tracegpu_cpu_metrics *m=tracegpu_cpu_stats();
 if(expect==PARTIAL||expect==SINGLE||expect==RX||expect==FROZEN){assert(m->tiles<TG_DAMAGE_TILES);fine_assertions++;}
 if(expect==ZERO){assert(!m->tiles&&!m->flush_bytes&&!m->painted_pixels);zero_assertions++;}
 if(expect==FULL){assert(m->tiles==TG_DAMAGE_TILES);full_assertions++;}
 if(expect==SINGLE){if(m->tiles>max_single_lane_tiles)max_single_lane_tiles=m->tiles;assert(m->tiles<=12U);}
 if(expect==RX){if(m->tiles>max_rx_tiles)max_rx_tiles=m->tiles;assert(m->tiles<=125U);}
 if(expect==FROZEN){if(m->tiles>max_frozen_tiles)max_frozen_tiles=m->tiles;assert(m->tiles<=150U);frozen_frames++;}
 total_tiles+=m->tiles;total_flush+=m->flush_bytes;
 front=back;assert(!tracegpu_cpu_confirm(front));tracegpu_dashboard_cpu_release();
 for(unsigned b=0;b<2;b++)assert(surface[b].before==0x12345678U&&surface[b].after==0x98765432U);
 frames++;
}
static void settle(tracegpu_ui_state *u,tracegpu_ui_state *live,tracegpu_ui_shell *s,const tracegpu_uart_event_ring *r){present(u,live,s,r,0,ANY);present(u,live,s,r,0,ANY);}
static void restore(tracegpu_ui_state *u,tracegpu_ui_shell *s){*u=saved_ui;*s=saved_shell;plan=saved_plan;memset(&touch,0,sizeof touch);}
static void cpu_partial_fixture(tracegpu_ui_state *u,tracegpu_ui_shell *s,const tracegpu_uart_event_ring *r){
 saved_ui=*u;saved_shell=*s;saved_plan=plan;front=0;tracegpu_cpu_reset();memset(&touch,0,sizeof touch);
 memset(reference,0xa5,sizeof reference);
 for(unsigned b=0;b<2;b++){surface[b].before=0x12345678U;surface[b].after=0x98765432U;memset(surface[b].pixels,0xa5,sizeof surface[b].pixels);}
 scenario="warmup";settle(u,u,s,r);

 /* Fix the viewport and all labels. Only one primitive of one lane changes,
  * then changes color and disappears. The untouched seven lanes stay clean. */
 if(!u->data_page&&!u->bus_mode){
  s->drawer=0;s->position=s->target=0;u->running=0;u->follow=0;
  memset(&plan,0,sizeof plan);plan.count=64;plan.dual=1;
  for(unsigned lane=0;lane<8;lane++)for(unsigned item=0;item<8;item++){
   unsigned q=lane*8+item;plan.rects[q]=(tracegpu_render_rect){(uint16_t)(112U+item*75U),(uint16_t)(120U+lane*26U),35,3};plan.colors[q]=0xff307090U+lane*0x00151207U;
  }
  scenario="eight_lane_settle";settle(u,u,s,r);
  scenario="single_lane_geometry_color_and_old_shape_erase";
  for(unsigned q=0;q<12;q++){
   plan.rects[27].x=(uint16_t)(337U+(q&1U)*8U);plan.rects[27].w=(uint16_t)(q%3U?35U:0U);plan.colors[27]^=0x00643311U;
   present(u,u,s,r,0,SINGLE);
  }
  scenario="insert_then_remove_prefix_without_invalidating_suffix";
  for(unsigned q=plan.count;q>0;q--){plan.rects[q]=plan.rects[q-1];plan.colors[q]=plan.colors[q-1];}
  plan.rects[0]=(tracegpu_render_rect){150,122,7,5};plan.colors[0]=0xffed8042;plan.count++;
  present(u,u,s,r,0,SINGLE);present(u,u,s,r,0,SINGLE);
  for(unsigned q=1;q<plan.count;q++){plan.rects[q-1]=plan.rects[q];plan.colors[q-1]=plan.colors[q];}plan.count--;
  present(u,u,s,r,0,SINGLE);present(u,u,s,r,0,SINGLE);
  scenario="rx_counters_are_not_waveform_geometry";settle(u,u,s,r);
  for(unsigned q=0;q<6;q++){u->byte_count+=17;u->channel_bytes[3]+=17;u->channel_overflows[3]++;u->frame_errors++;u->latest_byte=(uint8_t)q;present(u,u,s,r,0,RX);}
  scenario="raw_rows_not_visible_on_wave_page";settle(u,u,s,r);
  for(unsigned q=0;q<4;q++){u->logic_rows[q].sequence++;u->logic_rows[q].tick+=1234;u->logic_evicted++;present(u,u,s,r,0,ZERO);}
  scenario="order_sensitive_overlapping_colors";
  plan.rects[plan.count]=(tracegpu_render_rect){200,174,18,21};plan.colors[plan.count++]=0xffea354f;
  plan.rects[plan.count]=(tracegpu_render_rect){207,178,19,18};plan.colors[plan.count++]=0xff37ce9a;
  settle(u,u,s,r);
  tracegpu_render_rect temp=plan.rects[plan.count-1];uint32_t c=plan.colors[plan.count-1];
  plan.rects[plan.count-1]=plan.rects[plan.count-2];plan.colors[plan.count-1]=plan.colors[plan.count-2];
  plan.rects[plan.count-2]=temp;plan.colors[plan.count-2]=c;
  present(u,u,s,r,0,SINGLE);present(u,u,s,r,0,SINGLE);
 }

 restore(u,s);s->drawer=0;s->position=s->target=0;s->show_cpu=s->show_gpu=1;
 scenario="frozen_body_and_independent_live_header";
 tracegpu_dashboard_freeze_capture(u,r);u->frozen=1;s->frozen=1;live_ui=*u;
 settle(u,&live_ui,s,r);
 for(unsigned q=0;q<8;q++){
  live_ui.running=(uint8_t)(q&1U);live_ui.cpu_percent_valid=live_ui.gpu_percent_valid=1;
  live_ui.cpu_percent=(uint8_t)(q<4?100:9);live_ui.gpu_percent=(uint8_t)(q*7U);
  live_ui.byte_count+=513;live_ui.latest_event_ticks+=10000;live_ui.logic_records+=3;
  present(u,&live_ui,s,r,0,FROZEN);
 }
 scenario="frozen_live_header_tab_indicator";
 live_ui.data_page=(uint8_t)(live_ui.data_page?0:4);present(u,&live_ui,s,r,0,FROZEN);
 live_ui.data_page=u->data_page;present(u,&live_ui,s,r,0,FROZEN);
 scenario="frozen_live_diagnostics_overlay";s->drawer=3;s->position=s->target=800;s->menu_page=TG_MENU_DIAGNOSTICS;settle(u,&live_ui,s,r);
 for(unsigned q=0;q<4;q++){live_ui.submit_fps++;live_ui.render_metrics_valid=1;live_ui.gpu_render_us+=25;present(u,&live_ui,s,r,0,PARTIAL);}
 restore(u,s);scenario="unknown_field_and_explicit_full_fallback";settle(u,u,s,r);
 u->trigger_source_mode=(tracegpu_uart_trigger)((unsigned)u->trigger_source_mode^1U);present(u,u,s,r,0,FULL);
 present(u,u,s,r,1,FULL);
 scenario="both_canvases_settled_idle";settle(u,u,s,r);present(u,u,s,r,0,ZERO);present(u,u,s,r,0,ZERO);
 memcpy(image.pixels,surface[front].pixels,sizeof image.pixels);
}
int main(int argc,char **argv){
 assert(argc==2);
 {
  static tracegpu_damage d;uint32_t mask[TG_DAMAGE_WORDS]={0};tracegpu_damage_init(&d);
  uint32_t epoch=d.epoch;tracegpu_damage_mark_tiles(&d,mask);assert(d.epoch==epoch);
  mask[0]=1U;mask[(TG_DAMAGE_TILES-1U)/32U]|=1U<<((TG_DAMAGE_TILES-1U)%32U);
  tracegpu_damage_mark_tiles(&d,mask);assert(d.epoch==epoch+1U&&d.desired[0]==d.epoch&&d.desired[TG_DAMAGE_TILES-1U]==d.epoch);
  assert(d.desired[1]==epoch);
  /* Many primitives collapse to the same tile mask; the API consumes exactly
   * one epoch per call, and never treats integer wrap as fresh canvas data. */
  for(unsigned i=0;i<TG_DAMAGE_WORDS;i++)mask[i]=UINT32_MAX;
  epoch=d.epoch;tracegpu_damage_mark_tiles(&d,mask);assert(d.epoch==epoch+1U);
  for(unsigned i=0;i<TG_DAMAGE_TILES;i++)assert(d.desired[i]==d.epoch);
  memset(mask,0,sizeof mask);mask[TG_DAMAGE_WORDS-1U]=UINT32_MAX<<(TG_DAMAGE_TILES%32U);
  epoch=d.epoch;tracegpu_damage_mark_tiles(&d,mask);assert(d.epoch==epoch); /* unused bits */
  d.epoch=UINT32_MAX;mask[0]=1;tracegpu_damage_mark_tiles(&d,mask);assert(d.fault&&!d.active&&!d.ready);
  assert(tracegpu_damage_begin(&d,1,0,0)<0);puts("FINE_TILE_MASK_EPOCH_PASS single_increment=1 empty_noop=1 ignored_unused_bits=1 wrap_quarantine=1");
 }
 for(mode_index=0;mode_index<25;mode_index++){
  char value[16],path[1024];snprintf(value,sizeof value,"%u",mode_index);snprintf(path,sizeof path,"%s/mode_%u.bmp",argv[1],mode_index);
  char *a[]={argv[0],path,value,0};assert(!fine_fixture_main(3,a));
 }
 assert(fine_assertions&&full_assertions&&zero_assertions&&frozen_frames&&max_single_lane_tiles);
 char path[1024];snprintf(path,sizeof path,"%s/engine_result.json",argv[1]);FILE *f=fopen(path,"wb");assert(f);
 assert(fprintf(f,"{\n  \"Status\": \"HOST_PASS_NOT_BOARD_ACCEPTED\",\n  \"ActualRendererModes\": %u,\n  \"Frames\": %u,\n  \"PixelDifferences\": 0,\n  \"FineAssertions\": %u,\n  \"IdleAssertions\": %u,\n  \"FullFallbackAssertions\": %u,\n  \"FrozenHeaderFrames\": %u,\n  \"MaxSingleLaneTiles\": %u,\n  \"MaxRXInfoTiles\": %u,\n  \"MaxFrozenHeaderTiles\": %u,\n  \"TotalTiles\": %llu,\n  \"FlushBytes\": %llu,\n  \"FrontBufferUntouched\": true,\n  \"Programmed\": false,\n  \"BoardAccepted\": false\n}\n",mode_index,frames,fine_assertions,zero_assertions,full_assertions,frozen_frames,max_single_lane_tiles,max_rx_tiles,max_frozen_tiles,(unsigned long long)total_tiles,(unsigned long long)total_flush)>0);
 assert(!fclose(f));printf("RENDER_FINE_V35_PASS modes=%u frames=%u differences=0 single_lane_max_tiles=%u rx_max_tiles=%u frozen_header_max_tiles=%u full_fallbacks=%u front_untouched=1 board_tested=0\n",mode_index,frames,max_single_lane_tiles,max_rx_tiles,max_frozen_tiles,full_assertions);return 0;
}
