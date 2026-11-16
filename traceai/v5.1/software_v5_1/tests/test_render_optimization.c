#include "render_cpu.h"
#include <stdint.h>
static void cpu_partial_fixture(tracegpu_ui_state *,tracegpu_ui_shell *,const tracegpu_uart_event_ring *);
#define TG_CPU_PARTIAL_TEST 1
#define main optimization_fixture_main
#include "preview_logic.c"
#undef main

/* Independent one-shot compositor versus the actual asynchronous, alternating
 * back-buffer engine. The test does not replace any production paint writer. */
static struct {
 uint32_t before;
 uint32_t pixels[800*480] __attribute__((aligned(32)));
 uint32_t after;
} surface[2];
static uint32_t reference[800*480],held_front[800*480];
static tracegpu_ui_state initial_ui;
static tracegpu_ui_shell initial_shell;
static tracegpu_wave_plan initial_plan,saved_plan;
static tracegpu_touch_frame touch;
static unsigned front,back,mode_index,frames,expect_partial_count,expect_full_count,expect_zero_count;
static unsigned live_frames,viewport_frames,menu_frames,page_motion_frames,plan_frames,touch_frames,max_polls;
static unsigned profile_frames;
static uint64_t total_tiles,total_pixels,total_flush;
static const char *scenario;
enum {EXPECT_ANY,EXPECT_PARTIAL,EXPECT_FULL,EXPECT_ZERO};

static void flush_check(uintptr_t address,uint32_t bytes){
 assert(!(address&31U)&&!(bytes&31U)&&bytes&&bytes<=3200U);
 assert(address>=(uintptr_t)surface[back].pixels&&address+bytes<=(uintptr_t)(surface[back].pixels+800*480));
 assert(address<(uintptr_t)surface[front].pixels||address>=(uintptr_t)(surface[front].pixels+800*480));
}
static void reference_draw(const tracegpu_ui_state *u,const tracegpu_ui_shell *s,const tracegpu_uart_event_ring *ring){
 tracegpu_clip_reset();tracegpu_dashboard_set_wave_plan(&plan);
 tracegpu_dashboard_draw_uart((uintptr_t)reference,u,ring);
 tracegpu_dashboard_set_wave_plan(0);tracegpu_dashboard_draw_analysis((uintptr_t)reference,u);
 tracegpu_ui_shell_draw((uintptr_t)reference,s,u);
 tracegpu_dashboard_draw_touch_points((uintptr_t)reference,&touch);
}
static void present(tracegpu_ui_state *u,tracegpu_ui_shell *s,const tracegpu_uart_event_ring *ring,int force_full,unsigned expectation){
 reference_draw(u,s,ring);back=front^1U;
 memcpy(held_front,surface[front].pixels,sizeof held_front);
 tracegpu_dashboard_cpu_latch(u,ring);
 assert(!tracegpu_cpu_begin(back,front,(uintptr_t)surface[back].pixels,u,u,s,&touch,&plan,force_full));
 assert(tracegpu_cpu_confirm(back)<0);
 int rc=0;unsigned polls=0;
 while(!rc){rc=tracegpu_cpu_step(front,flush_check);assert(rc>=0&&++polls<100000U);}
 assert(tracegpu_cpu_confirm(front)<0);
 assert(!memcmp(held_front,surface[front].pixels,sizeof held_front));
 for(unsigned pixel=0;pixel<800*480;pixel++)if(surface[back].pixels[pixel]!=reference[pixel]){
  fprintf(stderr,"OPT_PIXEL_DIFFERENCE mode=%u scenario=%s frame=%u x=%u y=%u got=%08x expected=%08x\n",
   mode_index,scenario,frames,pixel%800,pixel/800,surface[back].pixels[pixel],reference[pixel]);abort();
 }
 const tracegpu_cpu_metrics *m=tracegpu_cpu_stats();
 if(expectation==EXPECT_PARTIAL){
  if(m->tiles>=TG_DAMAGE_TILES){fprintf(stderr,"OPT_EXPECT_PARTIAL mode=%u scenario=%s tiles=%u\n",mode_index,scenario,m->tiles);abort();}
  expect_partial_count++;
 }
 if(expectation==EXPECT_FULL){assert(m->tiles==TG_DAMAGE_TILES);expect_full_count++;}
 if(expectation==EXPECT_ZERO){assert(!m->tiles&&!m->painted_pixels&&!m->flush_bytes);expect_zero_count++;}
 total_tiles+=m->tiles;total_pixels+=m->painted_pixels;total_flush+=m->flush_bytes;
 if(polls>max_polls)max_polls=polls;
 front=back;assert(!tracegpu_cpu_confirm(front));tracegpu_dashboard_cpu_release();
 for(unsigned b=0;b<2;b++)assert(surface[b].before==0x12345678U&&surface[b].after==0x98765432U);
 frames++;
}
static void restore_scene(tracegpu_ui_state *u,tracegpu_ui_shell *s){
 *u=initial_ui;*s=initial_shell;plan=initial_plan;memset(&touch,0,sizeof touch);
}
static void settle(tracegpu_ui_state *u,tracegpu_ui_shell *s,const tracegpu_uart_event_ring *ring){
 present(u,s,ring,0,EXPECT_ANY);present(u,s,ring,0,EXPECT_ANY);
}
static void cpu_partial_fixture(tracegpu_ui_state *u,tracegpu_ui_shell *s,const tracegpu_uart_event_ring *ring){
 initial_ui=*u;initial_shell=*s;initial_plan=plan;front=0;
 tracegpu_cpu_reset();memset(&touch,0,sizeof touch);
 memset(reference,0xa5,sizeof reference);
 for(unsigned b=0;b<2;b++){
  surface[b].before=0x12345678U;surface[b].after=0x98765432U;
  memset(surface[b].pixels,0xa5,sizeof surface[b].pixels);
 }
 scenario="warmup";settle(u,s,ring);

 /* Model the multiple fields that main publishes together, not just one
  * synthetic viewport field. Tables intentionally keep conservative fallback. */
 scenario="main_multi_field_live";
 for(unsigned step=0;step<8;step++){
  u->byte_count+=19;u->frame_errors+=step&1U;u->parity_errors+=step%3U;
  u->latest_byte=(uint8_t)(0x80U+step);u->rx_level=(uint8_t)(step&1U);
  u->retained_events+=17;u->covered_events+=3;u->retained_first+=3;u->retained_last+=20;
  u->logic_records+=7;u->logic_end+=10000;u->latest_event_ticks+=10000;
  u->logic_rows[step%8].tick+=10000;u->logic_rows[step%8].sequence+=7;
  u->logic_density[(step*7)%64]=(uint8_t)(30+step);
  for(unsigned channel=0;channel<4;channel++)u->channel_bytes[channel]+=channel+step+1;
  u->quality.counter[TG_QUALITY_HISTORY_EVICTED].value+=3;
  u->spi_lag_us+=17;
  u->spi_total++;u->spi_rows[0].count++;u->spi_selected.total++;u->spi_count=1;
  u->spi_sclk=100000U+step;u->spi_words[0].mosi=(uint16_t)(0x50U+step);
  u->i2c_rows[0].count++;u->i2c_selected.total++;u->i2c_count=1;u->i2c_omitted++;
  u->decode_total++;u->decode_progress=step*7U;u->decode_rows[0].value++;
  u->decode_detail.row.value++;u->export_bytes+=16;
  s->profile_window_us=1000000U+step;s->profile_valid=1;s->profile_us[step%8]+=777;
  tracegpu_wave_build(&plan,u,ring);
  present(u,s,ring,0,mode_index==0?EXPECT_PARTIAL:EXPECT_ANY);live_frames++;
 }

 /* A periodic profile publication has no pixels with its menu closed, and
  * may repair only the overlay when open. Toggling header percentages also
  * updates the diagnostics button label, even if no pressed frame was drawn. */
 restore_scene(u,s);s->drawer=0;s->position=0;
 scenario="profile_publish_closed";settle(u,s,ring);
 for(unsigned step=0;step<4;step++){
  s->profile_window_us=1000000U+step;s->profile_valid=1;s->profile_us[step]+=100;
  present(u,s,ring,0,EXPECT_ZERO);profile_frames++;
 }
 s->drawer=3;s->position=s->target=800;s->menu_page=TG_MENU_PERF;
 scenario="profile_publish_open";settle(u,s,ring);
 for(unsigned step=0;step<4;step++){
  s->profile_window_us++;s->profile_us[step]+=100;
  present(u,s,ring,0,EXPECT_PARTIAL);profile_frames++;
 }
 s->menu_page=TG_MENU_DIAGNOSTICS;scenario="header_percent_toggle_menu_label";settle(u,s,ring);
 s->show_cpu=!s->show_cpu;s->show_gpu=!s->show_gpu;present(u,s,ring,0,EXPECT_ANY);
 s->menu_page=TG_MENU_APPEARANCE;scenario="automatic_motion_mode_change";settle(u,s,ring);
 s->motion_mode=2;s->motion_reduced=1;s->motion=0;present(u,s,ring,0,EXPECT_ANY);

 restore_scene(u,s);scenario="continuous_zoom_and_pan";settle(u,s,ring);
 for(unsigned step=0;step<12;step++){
  u->time_window_ticks=(step&1U)?400000U+step*777U:1500000U-step*2222U;
  u->view_offset_ticks=(step&1U)?-(int64_t)(step*5000U):(int64_t)(step*2500U);
  u->view_anchor_ticks=2000000U+step*2500U;u->follow=0;
  tracegpu_wave_build(&plan,u,ring);
  present(u,s,ring,0,mode_index==0?EXPECT_PARTIAL:EXPECT_ANY);viewport_frames++;
 }

 restore_scene(u,s);scenario="plan_only_unchanged_ui";settle(u,s,ring);saved_plan=plan;
 assert(saved_plan.count+1U<TRACEGPU_WAVE_MAX_RECTS);
 const tracegpu_render_rect locations[]={{0,0,32,16},{31,95,65,21},{80,100,53,38},
  {5,350,73,13},{760,400,40,16},{13,450,65,26},{799,479,1,1},{96,112,49,42}};
 for(unsigned step=0;step<sizeof locations/sizeof locations[0];step++){
  plan=saved_plan;plan.rects[plan.count]=locations[step];plan.colors[plan.count]=0xff1577a9U+step*0x00110000U;
  plan.count++;present(u,s,ring,0,EXPECT_ANY);plan_frames++;
  /* The same UI/count/geometry with a new color must repair both canvases. */
  plan.colors[plan.count-1]^=0x00f0a050U;
  present(u,s,ring,0,EXPECT_ANY);plan_frames++;
 }
 plan=saved_plan;plan.rects[plan.count]=(tracegpu_render_rect){799,479,2,2};
 plan.colors[plan.count++]=0xff123456U;present(u,s,ring,0,EXPECT_ANY);plan_frames++;
 plan.rects[plan.count-1]=(tracegpu_render_rect){96,112,0,40};
 present(u,s,ring,0,EXPECT_ANY);plan_frames++;
 plan=saved_plan;present(u,s,ring,0,EXPECT_ANY);plan_frames++;

 /* Position sequences include opening, reversal, closing, reopening, and
  * the final drawer=0 transition that restores tabs and dock buttons. */
 for(unsigned drawer=1;drawer<=4;drawer++){
  restore_scene(u,s);s->drawer=0;s->position=0;s->motion=1;
  scenario="menu_open_close_reverse";settle(u,s,ring);
  int extent=drawer==1?256:drawer==3?800:266;
  int positions[]={0,24,extent/2,extent,extent-16,32,extent/3,extent,0};
  for(unsigned step=0;step<sizeof positions/sizeof positions[0];step++){
   s->drawer=(int)drawer;s->position=positions[step];s->target=step>=4&&step<=5?0:extent;
   s->anim_from=step?positions[step-1]:0;s->anim_start=1000U+step*37U;
   s->animating=positions[step]!=s->target;s->now=1010U+step*37U;
   s->menu_page=TG_MENU_HOME;s->decode_detail_page=0;
   present(u,s,ring,0,EXPECT_ANY);menu_frames++;
  }
  s->drawer=s->position=s->target=s->animating=0;
  present(u,s,ring,0,EXPECT_ANY);menu_frames++;
 }

 restore_scene(u,s);s->drawer=0;s->position=0;s->motion=1;
 scenario="page_motion_begin_end";settle(u,s,ring);
 const uint32_t animation_time[]={0,1,30,70,140,179,180,220};
 for(unsigned step=0;step<sizeof animation_time/sizeof animation_time[0];step++){
  s->page_motion=animation_time[step]<180;s->page_motion_start=2000;
  s->now=2000+animation_time[step];
  present(u,s,ring,0,EXPECT_ANY);page_motion_frames++;
 }

 restore_scene(u,s);scenario="five_points_removal_and_short_text";settle(u,s,ring);
 for(unsigned step=0;step<7;step++){
  touch.online=1;touch.count=(uint8_t)(step<5?step+1:step==5?1:0);touch.sequence++;
  for(unsigned point=0;point<touch.count;point++){
   touch.point[point].x=(uint16_t)((step*79U+point*137U)%800U);
   touch.point[point].y=(uint16_t)((step*43U+point*71U)%480U);touch.point[point].id=(uint8_t)point;
  }
  u->cpu_percent_valid=1;u->cpu_percent=(uint8_t)(step<3?100:9);s->show_cpu=1;
  present(u,s,ring,0,EXPECT_ANY);touch_frames++;
 }

 restore_scene(u,s);scenario="unknown_field_full_fallback";settle(u,s,ring);
 u->trigger_source_mode=(tracegpu_uart_trigger)((unsigned)u->trigger_source_mode^1U);
 present(u,s,ring,0,EXPECT_FULL);
 scenario="explicit_full_reference";present(u,s,ring,1,EXPECT_FULL);
 scenario="both_buffers_repaired_then_idle";settle(u,s,ring);
 present(u,s,ring,0,EXPECT_ZERO);present(u,s,ring,0,EXPECT_ZERO);
 memcpy(image.pixels,surface[front].pixels,sizeof image.pixels);
}
int main(int argc,char **argv){
 assert(argc==2);
 for(mode_index=0;mode_index<25;mode_index++){
  char value[16],file[1024];snprintf(value,sizeof value,"%u",mode_index);
  snprintf(file,sizeof file,"%s/mode_%u.bmp",argv[1],mode_index);
  char *args[]={argv[0],file,value,0};assert(!optimization_fixture_main(3,args));
 }
 char path[1024];snprintf(path,sizeof path,"%s/engine_result.json",argv[1]);
 FILE *f=fopen(path,"wb");assert(f);
 assert(fprintf(f,"{\n  \"Status\": \"HOST_PASS_NOT_BOARD_ACCEPTED\",\n  \"ActualRendererModes\": %u,\n  \"Frames\": %u,\n  \"PixelDifferences\": 0,\n  \"PartialAssertions\": %u,\n  \"FullFallbackAssertions\": %u,\n  \"IdleAssertions\": %u,\n  \"LiveFrames\": %u,\n  \"ViewportFrames\": %u,\n  \"MenuFrames\": %u,\n  \"PageMotionFrames\": %u,\n  \"PlanOnlyFrames\": %u,\n  \"TouchFrames\": %u,\n  \"ProfileFrames\": %u,\n  \"TotalTiles\": %llu,\n  \"PaintedPixels\": %llu,\n  \"FlushBytes\": %llu,\n  \"MaxPolls\": %u,\n  \"FrontBufferUntouched\": true,\n  \"Programmed\": false,\n  \"BoardAccepted\": false\n}\n",
  mode_index,frames,expect_partial_count,expect_full_count,expect_zero_count,live_frames,viewport_frames,menu_frames,page_motion_frames,plan_frames,touch_frames,profile_frames,
  (unsigned long long)total_tiles,(unsigned long long)total_pixels,(unsigned long long)total_flush,max_polls)>0);
 assert(!fclose(f));
 printf("RENDER_OPTIMIZATION_PASS modes=%u frames=%u differences=0 partial_assertions=%u full_fallbacks=%u idle_assertions=%u front_untouched=1 board_tested=0\n",
  mode_index,frames,expect_partial_count,expect_full_count,expect_zero_count);
 return 0;
}
