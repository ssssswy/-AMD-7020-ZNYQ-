#include "render_cpu.h"
#include "ui_freeze_snapshot.h"
#include "tracegpu_time.h"
#include "ui_layout.h"
#include <string.h>
static tracegpu_damage damage;
static tracegpu_ui_state previous;
static tracegpu_ui_state previous_live;
static tracegpu_ui_shell previous_shell;
static tracegpu_touch_frame previous_touch;
/* Exact geometry identity, never a collision-prone damage hash. Only active
 * entries are compared/copied; no framebuffer or live history is duplicated. */
static tracegpu_wave_plan previous_plan;
static uint8_t initialized,have_previous;
enum {BAND_NEXT,BAND_SCAN_FIRST,BAND_HANDOFF,BAND_START,BAND_GPU,BAND_SCAN_MORE,BAND_ACQUIRE,BAND_OVERLAY,BAND_FLUSH,BAND_INDEX};
static struct {
 uintptr_t canvas;const tracegpu_ui_state *u,*live;const tracegpu_ui_shell *shell;
 const tracegpu_touch_frame *touch;const tracegpu_wave_plan *plan;
 tracegpu_damage_rect rect;unsigned row;uint8_t phase,started,covered,colored;
 const tracegpu_partial_gpu_ops *gpu;void *context;uint8_t base_on_gpu;
 tracegpu_render_rect commands[TRACEGPU_PARTIAL_BATCH_RECTS];
 uint32_t command_colors[TRACEGPU_PARTIAL_BATCH_RECTS];
 unsigned scan,count,submitted;uint32_t color,command_pixels,min_area;
 unsigned first[TG_DAMAGE_ROWS],end[TG_DAMAGE_ROWS],index_cursor,limit;
 uint32_t wave_tiles[TG_DAMAGE_WORDS];
 XTime gpu_begin;
} job;
static tracegpu_cpu_metrics metrics,completed;
static uint32_t batch_sequence;
void tracegpu_cpu_reset(void){tracegpu_damage_init(&damage);memset(&metrics,0,sizeof metrics);memset(&job,0,sizeof job);initialized=1;have_previous=0;tracegpu_clip_reset();tracegpu_wave_cpu_range(0,0,0);}
const tracegpu_cpu_metrics *tracegpu_cpu_stats(void){return &metrics;}
const tracegpu_cpu_metrics *tracegpu_cpu_completed(void){return &completed;}
static void touch_damage(const tracegpu_touch_frame *t){
 for(unsigned i=0;i<t->count&&i<5;i++)tracegpu_damage_mark(&damage,(int)t->point[i].x-20,(int)t->point[i].y-20,72,48);
}
static void button_damage(const tracegpu_ui_shell *s,int id){
 tracegpu_ui_control c[TRACEGPU_UI_MAX_CONTROLS];unsigned n=tracegpu_ui_shell_controls(s,c);
 for(unsigned i=0;i<n;i++)if(c[i].id==id)tracegpu_damage_mark(&damage,c[i].x-4,c[i].y-4,c[i].w+8,c[i].h+8);
}
static void body_damage(void){tracegpu_damage_mark(&damage,0,96,800,320);}
static void controls_damage(const tracegpu_ui_shell *s){
 tracegpu_ui_control c[TRACEGPU_UI_MAX_CONTROLS];unsigned n=tracegpu_ui_shell_controls(s,c);
 for(unsigned i=0;i<n;i++)tracegpu_damage_mark(&damage,c[i].x-4,c[i].y-4,c[i].w+8,c[i].h+8);
}
static void overlay_damage(const tracegpu_ui_shell *s){
 if(s->drawer==1&&s->position>0)tracegpu_damage_mark(&damage,0,480-s->position,800,s->position);
 else if(s->drawer&&s->position>0)tracegpu_damage_mark(&damage,800-s->position,48,s->position,368);
}
static void page_damage(const tracegpu_ui_shell *s){
 uint32_t age=s->now-s->page_motion_start;
 if(s->page_motion&&s->motion&&age<180U){int w=800-(int)age*800/180;tracegpu_damage_mark(&damage,800-w,96,w,268);}
}
static uint32_t primitive_color(const tracegpu_wave_plan *p,unsigned i){
 return p->colors[i]?p->colors[i]:((p->dual&&p->rects[i].y>=166U)?0xFFFFB448U:TRACEGPU_WAVE_COLOR);
}
static int primitive_equal(const tracegpu_wave_plan *a,unsigned ai,const tracegpu_wave_plan *b,unsigned bi){
 int av=tracegpu_render_rect_valid(&a->rects[ai]),bv=tracegpu_render_rect_valid(&b->rects[bi]);
 if(!av||!bv)return av==bv; /* Invalid primitives have no visible pixels. */
 return !memcmp(&a->rects[ai],&b->rects[bi],sizeof a->rects[ai])&&primitive_color(a,ai)==primitive_color(b,bi);
}
static void primitive_damage(const tracegpu_wave_plan *p,unsigned i,uint32_t *mask){
 const tracegpu_render_rect *r=&p->rects[i];
 if(!tracegpu_render_rect_valid(r))return;
 for(unsigned row=r->y/16U;row<=(r->y+r->h-1U)/16U;row++){
  unsigned first=row*TG_DAMAGE_COLS+r->x/32U;
  unsigned last=row*TG_DAMAGE_COLS+(r->x+r->w-1U)/32U;
  for(unsigned word=first/32U;word<=last/32U;word++){
   unsigned lo=word==first/32U?first%32U:0U,hi=word==last/32U?last%32U:31U;
   mask[word]|=(hi==31U?UINT32_MAX:(1U<<(hi+1U))-1U)&(UINT32_MAX<<lo);
  }
 }
}
static void plan_damage(const tracegpu_wave_plan *a,const tracegpu_wave_plan *b){
 unsigned first=0,ae=a->count,be=b->count;
 uint32_t mask[TG_DAMAGE_WORDS]={0}; /* 96 bytes, not a plan/paint copy. */
 /* Preserve order: only identical prefix/suffix primitives are excluded.
  * Every changed old shape is erased and every new shape reconstructed, then
  * the compositor replays ALL overlapping commands in their original order. */
 while(first<ae&&first<be&&primitive_equal(a,first,b,first))first++;
 while(ae>first&&be>first&&primitive_equal(a,ae-1U,b,be-1U)){ae--;be--;}
 for(unsigned i=first;i<ae;i++)primitive_damage(a,i,mask);
 for(unsigned i=first;i<be;i++)primitive_damage(b,i,mask);
 /* A busy 8K-primitive frame must not consume 16K epochs. */
 tracegpu_damage_mark_tiles(&damage,mask);
}
static void overview_damage(void){tracegpu_damage_mark(&damage,0,354,800,62);}
static void line_label_damage(int x,int width,int label_x,int label_width){
 if(x<0)return;
 tracegpu_damage_mark(&damage,x,TG_PLOT_TOP,width,TG_PLOT_BOTTOM-TG_PLOT_TOP);
 tracegpu_damage_mark(&damage,label_x,TG_CONTENT_TOP,label_width,16);
}
static void plot_overlay_damage(const tracegpu_ui_state *u){
 if(u->ai_alert.selected)tracegpu_damage_mark(&damage,TG_PLOT_LEFT,TG_CONTENT_TOP,TG_PLOT_RIGHT-TG_PLOT_LEFT+1,TG_PLOT_BOTTOM-TG_CONTENT_TOP+4);
 uint64_t end=tracegpu_cursor_view_end(u->follow?u->latest_event_ticks:u->view_anchor_ticks,u->view_offset_ticks);
 uint64_t start=end>u->time_window_ticks?end-u->time_window_ticks:0;
 if(u->triggered)tracegpu_damage_mark(&damage,396,TG_PLOT_TOP,1,TG_PLOT_BOTTOM-TG_PLOT_TOP+1);
 if(u->running&&u->latest_event_ticks&&u->time_window_ticks&&u->latest_event_ticks>=start&&u->latest_event_ticks<=end){
  int x=TG_PLOT_LEFT+(int)((u->latest_event_ticks-start)*(TG_PLOT_RIGHT-TG_PLOT_LEFT)/u->time_window_ticks);
  if(x>TG_PLOT_RIGHT)x=TG_PLOT_RIGHT;
  int label=x>20?x-12:x;if(label>740)label=740;
  line_label_damage(x,1,label,40);
 }
 if(u->advanced_trigger.has_trigger){
  int x=tracegpu_cursor_pixel(u->advanced_trigger.trigger_tick,end,u->time_window_ticks);
  int a=tracegpu_cursor_pixel(u->advanced_trigger.condition_start_tick,end,u->time_window_ticks);
  line_label_damage(x,2,x>730?730:x,64);
  if(u->advanced_trigger.condition_start_tick!=u->advanced_trigger.trigger_tick)line_label_damage(a,1,a>710?710:a,64);
 }
 if(u->marker_valid){int x=tracegpu_cursor_pixel(u->marker_ticks,end,u->time_window_ticks);line_label_damage(x,1,x>720?720:x,80);}
 if(u->cursors.enabled)for(unsigned i=0;i<2;i++){
  int x=tracegpu_cursor_pixel(u->cursors.point[i].ticks,end,u->time_window_ticks);line_label_damage(x,1,x,24);
 }
 if(u->logic_mode)for(unsigned slot=0;slot<2;slot++)for(unsigned row=0;row<u->decode_annotation_count[slot]&&row<4;row++){
  int x=tracegpu_cursor_pixel(u->decode_annotations[slot][row].start,end,u->time_window_ticks);
  if(x<0)continue;
  if(x>704)x=704;
  tracegpu_damage_mark(&damage,x,304+(int)slot*16,72,16);
 }
}
static void viewport_damage(const tracegpu_ui_state *u){
 if(u->logic_mode&&u->bus_mode){body_damage();return;}
 tracegpu_damage_mark(&damage,0,336,800,20);overview_damage();plot_overlay_damage(u);
}
static void live_header_damage(const tracegpu_ui_state *u,const tracegpu_ui_shell *s){
 if(!have_previous)return;
 const tracegpu_ui_state *a=&previous_live;
 int changed=0;
 #define LIVE_HEADER(f) changed|=memcmp(&a->f,&u->f,sizeof a->f)!=0
 LIVE_HEADER(running);LIVE_HEADER(advanced_trigger.state);LIVE_HEADER(cpu_percent);LIVE_HEADER(gpu_percent);
 LIVE_HEADER(cpu_percent_valid);LIVE_HEADER(gpu_percent_valid);LIVE_HEADER(quality.level);
 LIVE_HEADER(logic_mode);LIVE_HEADER(spi_mode);LIVE_HEADER(i2c_mode);LIVE_HEADER(export_state);LIVE_HEADER(drain_state);
 LIVE_HEADER(selected_channel);LIVE_HEADER(baud);LIVE_HEADER(data_bits);LIVE_HEADER(parity);LIVE_HEADER(stop_bits);LIVE_HEADER(data_page);
 LIVE_HEADER(spi_config);LIVE_HEADER(logic_capture_mask);LIVE_HEADER(logic_channel_count);LIVE_HEADER(view_mode);
 LIVE_HEADER(submit_fps);LIVE_HEADER(fps_valid);LIVE_HEADER(update_idle);
 #undef LIVE_HEADER
 if(changed)tracegpu_damage_mark(&damage,0,0,800,96);
 /* Frozen-body snapshots are immutable, but an open live diagnostics/menu
  * layer still reads the live state. Repair that actual overlay, not the body. */
 if(memcmp(a,u,sizeof *u))overlay_damage(s);
 if(a->logic_mode!=u->logic_mode){button_damage(s,UI_HEAD);button_damage(s,UI_TAIL);}
}
static void classify(const tracegpu_ui_state *u,const tracegpu_ui_shell *s,const tracegpu_touch_frame *t){
 /* Mutate the previous comparison copy: no large UI structures on the 8KiB ARM stack. */
 tracegpu_ui_state *a=&previous;const tracegpu_ui_state *b=u;
 tracegpu_ui_shell *x=&previous_shell;const tracegpu_ui_shell *y=s;
 if(!have_previous){tracegpu_damage_all(&damage);return;}
 if(a->running!=b->running||a->drain_state!=b->drain_state)body_damage();
 /* Known dependencies repair only their composited regions. Unknown state and
  * layout/page changes still fall back to a complete, safe rebuild. */
 #define HEADER(f) do{if(a->f!=b->f){tracegpu_damage_mark(&damage,0,0,800,96);overlay_damage(x);overlay_damage(y);}a->f=b->f;}while(0)
 HEADER(cpu_percent);HEADER(gpu_percent);HEADER(cpu_percent_valid);HEADER(gpu_percent_valid);
 HEADER(touch_points);HEADER(touch_online);HEADER(touch_i2c_errors);HEADER(pinch_active);
 HEADER(gpu_wave_active);
 HEADER(submit_fps);HEADER(fps_valid);HEADER(update_idle);
 HEADER(running);HEADER(drain_state);HEADER(drain_reason);HEADER(export_state);
 #undef HEADER
 /* These measurements are only rendered inside the diagnostics body. */
 #define METRIC(f) do{if(a->f!=b->f){overlay_damage(x);overlay_damage(y);}a->f=b->f;}while(0)
 METRIC(submit_fps);METRIC(stream_fps);METRIC(gpu_render_us);METRIC(gpu_convert_us);
 METRIC(render_prepare_us);METRIC(render_wait_us);METRIC(render_frame_us);METRIC(render_reason);
 METRIC(render_metrics_valid);METRIC(update_idle);
 #undef METRIC
 if(memcmp(&a->ai,&b->ai,sizeof a->ai)){
  if(x->drawer==3&&x->menu_page==TG_MENU_AI)overlay_damage(x);
  if(y->drawer==3&&y->menu_page==TG_MENU_AI)overlay_damage(y);
  a->ai=b->ai;
 }
 if(memcmp(&a->ai_alert,&b->ai_alert,sizeof a->ai_alert)){
  tracegpu_damage_mark(&damage,0,48,800,72);overlay_damage(x);overlay_damage(y);
  if(a->ai_alert.selected!=b->ai_alert.selected||memcmp(&a->ai_alert.highlight,&b->ai_alert.highlight,sizeof a->ai_alert.highlight)||
   a->ai_alert.candidate_frame_visible!=b->ai_alert.candidate_frame_visible||a->ai_alert.candidate_frame_begin!=b->ai_alert.candidate_frame_begin||a->ai_alert.candidate_frame_end!=b->ai_alert.candidate_frame_end)body_damage();
  a->ai_alert=b->ai_alert;
 }
 if(b->ai_alert.visible&&y->motion&&y->now-b->ai_alert.appeared<180)tracegpu_damage_mark(&damage,200,48,496,48);
 if(x->ai_prompts!=y->ai_prompts||x->ai_banner_visible!=y->ai_banner_visible){tracegpu_damage_mark(&damage,0,48,800,72);overlay_damage(x);overlay_damage(y);}
 if(!a->data_page&&!b->data_page){
  #define BODY(f) do{if(memcmp(&a->f,&b->f,sizeof a->f)){body_damage();overlay_damage(x);overlay_damage(y);}memcpy(&a->f,&b->f,sizeof a->f);}while(0)
  #define INFO(f) do{if(memcmp(&a->f,&b->f,sizeof a->f)){overview_damage();overlay_damage(x);overlay_damage(y);}memcpy(&a->f,&b->f,sizeof a->f);}while(0)
  #define HIDDEN(f) do{if(memcmp(&a->f,&b->f,sizeof a->f)){overlay_damage(x);overlay_damage(y);}memcpy(&a->f,&b->f,sizeof a->f);}while(0)
  if(a->time_window_ticks!=b->time_window_ticks||a->view_offset_ticks!=b->view_offset_ticks||
     a->view_anchor_ticks!=b->view_anchor_ticks||a->latest_event_ticks!=b->latest_event_ticks||a->follow!=b->follow){
   viewport_damage(a);viewport_damage(b);overlay_damage(x);overlay_damage(y);
  }
  a->time_window_ticks=b->time_window_ticks;a->view_offset_ticks=b->view_offset_ticks;
  a->view_anchor_ticks=b->view_anchor_ticks;a->latest_event_ticks=b->latest_event_ticks;a->follow=b->follow;
  /* Live RX and retained-history counters are not waveform geometry. The
   * immutable plan comparison below owns exact lane repaint dependencies. */
  INFO(channel_bytes);INFO(channel_overflows);INFO(byte_count);INFO(frame_errors);INFO(parity_errors);
  INFO(overflows);INFO(lost_events);INFO(latest_byte);INFO(rx_level);BODY(triggered);
  INFO(burst_ordinal);INFO(burst_byte_count);INFO(burst_start_ticks);INFO(burst_end_ticks);INFO(has_completed_burst);INFO(rx_view_mode);
  HIDDEN(retained_events);HIDDEN(covered_events);HIDDEN(invalid_events);HIDDEN(retained_first);HIDDEN(retained_last);
  BODY(cursors);BODY(marker_ticks);BODY(marker_valid);BODY(marker_channel);BODY(marker_ai);
  HIDDEN(logic_count);INFO(logic_records);HIDDEN(logic_evicted);INFO(logic_gaps);INFO(logic_begin);INFO(logic_end);HIDDEN(logic_rows);INFO(logic_density);
  INFO(logic_measurement);INFO(logic_measure_state);INFO(logic_measure_progress);INFO(logic_period_min);INFO(logic_period_max);
  BODY(decode_annotations);BODY(decode_annotation_count);BODY(decode_annotation_omitted);
  HIDDEN(spi_lag_us);HIDDEN(spi_evicted);INFO(spi_words_total);INFO(spi_open);HIDDEN(spi_open_mask);
  /* Background transaction/review snapshots can advance while viewing waves.
   * Data pages retain full fallback; waveform pages repair body + all overlays. */
  HIDDEN(spi_busy);HIDDEN(spi_total);HIDDEN(spi_page);HIDDEN(spi_errors);HIDDEN(spi_omitted);HIDDEN(spi_sclk);
  HIDDEN(spi_count);HIDDEN(spi_rows);HIDDEN(spi_selected_valid);HIDDEN(spi_selected);
  HIDDEN(spi_word_count);HIDDEN(spi_word_page);HIDDEN(spi_word_valid);HIDDEN(spi_words);HIDDEN(spi_word_selected);HIDDEN(spi_word_index);
  INFO(spi_preview_count);INFO(spi_preview);HIDDEN(spi_row_preview_count);HIDDEN(spi_row_preview);
  INFO(cursor_readout);INFO(cursor_measure_valid);INFO(spi_direction);BODY(i2c_ascii);BODY(i2c_show_ack);
  HIDDEN(i2c_busy);HIDDEN(i2c_page);HIDDEN(i2c_omitted);HIDDEN(i2c_count);HIDDEN(i2c_rows);HIDDEN(i2c_selected_valid);HIDDEN(i2c_selected);
  HIDDEN(decode_count);HIDDEN(decode_total);HIDDEN(decode_omitted);HIDDEN(decode_busy);HIDDEN(decode_valid);HIDDEN(decode_progress);
  HIDDEN(decode_page);HIDDEN(decode_selected);HIDDEN(decode_rows);HIDDEN(decode_bus);HIDDEN(decode_bus_valid);HIDDEN(decode_detail);HIDDEN(decode_detail_state);
  HIDDEN(export_bytes);HIDDEN(export_size);HIDDEN(export_retries);HIDDEN(export_rate);HIDDEN(export_eta);HIDDEN(export_rate_valid);HIDDEN(export_eta_valid);
  INFO(i2c_total);HIDDEN(i2c_errors);HIDDEN(i2c_edges);BODY(bus_cells);BODY(bus_point);BODY(bus_point_valid);BODY(bus_progress);BODY(bus_state);
  if(a->advanced_trigger.state!=b->advanced_trigger.state)tracegpu_damage_mark(&damage,0,0,800,96);
  BODY(advanced_trigger);
  if(a->quality.level!=b->quality.level)tracegpu_damage_mark(&damage,0,0,800,96);
  if(memcmp(&a->quality,&b->quality,sizeof a->quality)){overlay_damage(x);overlay_damage(y);}
  a->quality=b->quality;
  #undef HIDDEN
  #undef INFO
  #undef BODY
 }
 a->gpu_wave_defer=b->gpu_wave_defer; /* backend control, not a pixel dependency */
 if(memcmp(a,b,sizeof *a))tracegpu_damage_all(&damage);
 if(memcmp(&previous_touch,t,sizeof *t)){touch_damage(&previous_touch);touch_damage(t);}
 if(x->pressed!=y->pressed){button_damage(x,x->pressed);button_damage(y,y->pressed);}
 if(x->feedback_id!=y->feedback_id||x->feedback_until!=y->feedback_until||x->now!=y->now){
  if(x->motion&&(int32_t)(x->feedback_until-x->now)>0)button_damage(x,x->feedback_id);
  if(y->motion&&(int32_t)(y->feedback_until-y->now)>0)button_damage(y,y->feedback_id);
 }
 if((x->notice&&(int32_t)(x->notice_until-x->now)>0)!=(y->notice&&(int32_t)(y->notice_until-y->now)>0))tracegpu_damage_mark(&damage,0,TG_NOTICE_TOP,800,TG_NOTICE_HEIGHT);
 if(x->drawer!=y->drawer||x->position!=y->position||x->scroll!=y->scroll||x->menu_page!=y->menu_page){
  overlay_damage(x);overlay_damage(y);controls_damage(x);controls_damage(y);
 }
 if(x->page_motion||y->page_motion){page_damage(x);page_damage(y);}
 if(x->show_cpu!=y->show_cpu||x->show_gpu!=y->show_gpu||x->show_fps!=y->show_fps){tracegpu_damage_mark(&damage,0,0,800,96);overlay_damage(x);overlay_damage(y);}
 if(x->ai_selected!=y->ai_selected){overlay_damage(x);overlay_damage(y);}
 if(x->running!=y->running||x->draining!=y->draining||x->motion!=y->motion||x->motion_mode!=y->motion_mode||x->motion_reduced!=y->motion_reduced){
  controls_damage(x);controls_damage(y);overlay_damage(x);overlay_damage(y);
 }
 /* main publishes these every second, even when the performance menu is shut. */
 if(x->profile_window_us!=y->profile_window_us||x->profile_valid!=y->profile_valid||memcmp(x->profile_us,y->profile_us,sizeof x->profile_us)){
  overlay_damage(x);overlay_damage(y);
 }
 if(x->notice!=y->notice||x->notice_until!=y->notice_until)tracegpu_damage_mark(&damage,0,TG_NOTICE_TOP,800,TG_NOTICE_HEIGHT);
 /* Input bookkeeping is not a paint dependency. Other shell changes remain conservative. */
 #define IGNORE(f) x->f=y->f
 IGNORE(now);IGNORE(pressed);IGNORE(feedback_id);IGNORE(feedback_until);
 IGNORE(owner);IGNORE(contact);IGNORE(id);IGNORE(x0);IGNORE(y0);IGNORE(x);IGNORE(y);
 IGNORE(moved);IGNORE(press_start);IGNORE(scroll_start);
 IGNORE(drawer);IGNORE(position);IGNORE(target);IGNORE(anim_from);IGNORE(anim_start);IGNORE(animating);IGNORE(scroll);IGNORE(menu_page);
 IGNORE(page_motion);IGNORE(page_motion_start);IGNORE(motion);IGNORE(show_cpu);IGNORE(show_gpu);IGNORE(running);IGNORE(draining);
 IGNORE(show_fps);
 IGNORE(ai_selected);
 IGNORE(ai_prompts);IGNORE(ai_banner_visible);IGNORE(ai_banner_id);IGNORE(ai_pressed_event);
 IGNORE(motion_mode);IGNORE(motion_reduced);IGNORE(profile_window_us);IGNORE(profile_valid);memcpy(x->profile_us,y->profile_us,sizeof x->profile_us);
 IGNORE(notice);IGNORE(notice_until);
 #undef IGNORE
 if(memcmp(x,y,sizeof *x))tracegpu_damage_all(&damage);
}
int tracegpu_cpu_begin(unsigned back,unsigned front,uintptr_t canvas,const tracegpu_ui_state *u,
 const tracegpu_ui_state *live,const tracegpu_ui_shell *s,const tracegpu_touch_frame *t,const tracegpu_wave_plan *p,int full){
 if(!initialized)tracegpu_cpu_reset();
 if(!u||!live||!s||!t||!p||p->count>TRACEGPU_WAVE_MAX_RECTS||damage.active||damage.ready||damage.fault||back==front||back>1||front>1)return -1;
 classify(u,s,t);
 if(have_previous)plan_damage(&previous_plan,p);
 live_header_damage(live,s);
 if(tracegpu_damage_begin(&damage,back,front,full))return -1;
 previous=*u;previous_live=*live;previous_shell=*s;previous_touch=*t;have_previous=1;
 previous_plan.count=p->count;previous_plan.dual=p->dual;
 memcpy(previous_plan.rects,p->rects,p->count*sizeof p->rects[0]);
 memcpy(previous_plan.colors,p->colors,p->count*sizeof p->colors[0]);
 memset(&job,0,sizeof job);job.canvas=canvas;job.u=u;job.live=live;job.shell=s;job.touch=t;job.plan=p;
 for(unsigned i=0;i<TG_DAMAGE_ROWS;i++)job.first[i]=p->count;
 /* Bounded 256-command indexing steps precede drawing, rather than N scans
    for every one of the thirty bands. Empty/idle jobs skip the index pass. */
 if(damage.tiles&&p->count)job.phase=BAND_INDEX;
 metrics.tiles=damage.tiles;metrics.painted_pixels=metrics.flush_bytes=metrics.work_ticks=metrics.wave_ticks=0;
 metrics.gpu_batches=metrics.gpu_rects=metrics.gpu_pixels=metrics.gpu_bands=metrics.cpu_bands=0;
 metrics.cache_ticks=metrics.gpu_wait_ticks=metrics.pl_cycles=0;
 metrics.wave_dirty_bands=0;
 if(damage.tiles==750)metrics.full_frames++;else metrics.partial_frames++;
 return 0;
}
int tracegpu_cpu_set_gpu(const tracegpu_partial_gpu_ops *ops,void *context){
 if(!damage.active||damage.fault||job.started||!ops||!ops->begin||!ops->submit||!ops->poll||!ops->invalidate)return -1;
 job.gpu=ops;job.context=context;job.colored=ops->begin_colored&&ops->submit_colored;return 0;
}
int tracegpu_cpu_gpu_min_area(uint32_t pixels){
 if(!damage.active||damage.fault||job.started||!job.gpu)return -1;
 job.min_area=pixels;return 0;
}
static int contains(const tracegpu_damage_rect *r,int x,int y,int w,int h){
 return w>0&&h>0&&r->x>=x&&r->y>=y&&r->x+r->w<=x+w&&r->y+r->h<=y+h;
}
static int opaque_band(void){
 const tracegpu_ui_shell *s=job.shell;
 if(s->drawer==1&&contains(&job.rect,0,480-s->position,800,s->position))return 1;
 if(s->drawer>=2&&s->drawer<=4&&contains(&job.rect,800-s->position,48,s->position,368))return 1;
 uint32_t age=s->now-s->page_motion_start;
 if(s->page_motion&&s->motion&&age<180U){int w=800-(int)age*800/180;if(contains(&job.rect,800-w,96,w,268))return 1;}
 return 0;
}
static void draw_tail(int gpu){
 tracegpu_clip_set(job.canvas,&job.rect);
 if(!job.covered){
 if(gpu){
  tracegpu_dashboard_wave_labels(job.canvas,job.u);
  tracegpu_dashboard_after_wave(job.canvas,job.u,0);
 }else{
  unsigned row=(unsigned)job.rect.y/16U;
  tracegpu_wave_cpu_range(job.plan,job.first[row],job.end[row]);
  tracegpu_dashboard_set_wave_plan(job.plan);
  tracegpu_dashboard_draw_uart(job.canvas,job.u,0);
  metrics.wave_ticks+=tracegpu_dashboard_cpu_wave_ticks();
  tracegpu_dashboard_set_wave_plan(0);
  tracegpu_wave_cpu_range(0,0,0);
 }
 tracegpu_dashboard_draw_analysis(job.canvas,job.u);
 }
 tracegpu_ui_shell_draw(job.canvas,job.shell,job.live);
 tracegpu_dashboard_draw_touch_points(job.canvas,job.touch);
 tracegpu_clip_reset();metrics.painted_pixels+=(uint32_t)job.rect.w*job.rect.h;
 job.row=0;job.phase=BAND_FLUSH;
}
/* The CPU reference skips invalid rectangles; preserve its exact clipping,
 * order and fallback colors (including dual-lane legacy plans). */
static int scan_commands(void){
 unsigned budget=256;
 while(job.scan<job.limit&&budget--){
  unsigned i=job.scan;tracegpu_render_rect r=job.plan->rects[i];
  if(!tracegpu_render_rect_valid(&r)){job.scan++;continue;}
  int x=r.x,y=r.y,w=r.w,h=r.h;
  tracegpu_clip_set(job.canvas,&job.rect);
  int visible=tracegpu_clip_rect(job.canvas,&x,&y,&w,&h);tracegpu_clip_reset();
  if(!visible){job.scan++;continue;}
  uint32_t color=primitive_color(job.plan,i);
  if(job.count&&((!job.colored&&color!=job.color)||job.count==TRACEGPU_PARTIAL_BATCH_RECTS))return 1;
  job.color=color;job.command_colors[job.count]=color;
  job.commands[job.count++]=(tracegpu_render_rect){(uint16_t)x,(uint16_t)y,(uint16_t)w,(uint16_t)h};
  job.command_pixels+=(uint32_t)w*h;job.scan++;
 }
 return job.scan==job.limit?1:0;
}
/* Exact wave_base pixels: opaque background, then its nine vertical grid
 * lines. All commands stay within the same aligned ownership band. The
 * legacy single-color backend retains the original CPU background path. */
static void seed_gpu_base(void){
 job.base_on_gpu=job.colored;
 if(!job.base_on_gpu)return;
 job.commands[job.count]=(tracegpu_render_rect){(uint16_t)job.rect.x,(uint16_t)job.rect.y,
  (uint16_t)job.rect.w,(uint16_t)job.rect.h};
 job.command_colors[job.count++]=0xff0b1220U;
 job.command_pixels+=(uint32_t)job.rect.w*job.rect.h;
 for(unsigned i=0;i<=8;i++){
  int x=TG_PLOT_LEFT+(TG_PLOT_RIGHT-TG_PLOT_LEFT)*(int)i/8;
  int y=TG_PLOT_TOP,h=TG_PLOT_BOTTOM-TG_PLOT_TOP,w=1;
  tracegpu_clip_set(job.canvas,&job.rect);int visible=tracegpu_clip_rect(job.canvas,&x,&y,&w,&h);tracegpu_clip_reset();
  if(visible){job.commands[job.count]=(tracegpu_render_rect){(uint16_t)x,(uint16_t)y,(uint16_t)w,(uint16_t)h};
   job.command_colors[job.count++]=0xff22334bU;job.command_pixels+=(uint32_t)w*h;}
 }
}
static void cache_rows(tracegpu_cpu_flush_fn flush,int handoff,int final){
 XTime begin,now;XTime_GetTime(&begin);
 for(unsigned budget=0;budget<4;budget++){
  uintptr_t a=job.canvas+((uintptr_t)(job.rect.y+(int)job.row)*800+(unsigned)job.rect.x)*4;
  uint32_t bytes=(unsigned)job.rect.w*4;XTime a_time,b_time;XTime_GetTime(&a_time);
  if(handoff||final){if(flush)flush(a,bytes);metrics.flush_bytes+=bytes;}
  if(!final)job.gpu->invalidate(job.context,a,bytes);
  XTime_GetTime(&b_time);metrics.cache_ticks+=(uint32_t)(b_time-a_time);
  if(++job.row==(unsigned)job.rect.h){
   job.row=0;
   if(final){tracegpu_damage_finish(&damage,&job.rect);job.phase=BAND_NEXT;}
   else job.phase=handoff?BAND_START:BAND_OVERLAY;
   break;
  }
  XTime_GetTime(&now);if(now-begin>COUNTS_PER_SECOND/4000U)break;
 }
}
static int step(unsigned front,tracegpu_cpu_flush_fn flush){
 int rc;unsigned wave_overlap=0;if(damage.fault||front>1U||front==damage.index){tracegpu_cpu_fault();return -1;}
 if(damage.ready)return 1;
 if(job.gpu&&!flush){tracegpu_cpu_fault();return -6;}
 job.started=1;
 if(job.phase==BAND_INDEX){
  unsigned end=job.index_cursor+256U;if(end>job.plan->count)end=job.plan->count;
  for(unsigned i=job.index_cursor;i<end;i++){
   const tracegpu_render_rect *r=&job.plan->rects[i];if(!tracegpu_render_rect_valid(r))continue;
   primitive_damage(job.plan,i,job.wave_tiles);
   for(unsigned row=r->y/16U;row<=(r->y+r->h-1U)/16U;row++){
    if(i<job.first[row])job.first[row]=i;
    job.end[row]=i+1U;
   }
  }
  job.index_cursor=end;if(end==job.plan->count)job.phase=BAND_NEXT;return 0;
 }
 if(job.phase==BAND_FLUSH){cache_rows(flush,0,1);return 0;}
 if(job.phase==BAND_HANDOFF){cache_rows(flush,1,0);return 0;}
 if(job.phase==BAND_ACQUIRE){cache_rows(flush,0,0);return 0;}
 if(job.phase==BAND_SCAN_FIRST||job.phase==BAND_SCAN_MORE){
  if(!scan_commands())return 0;
  if(!job.count){
   if(job.phase==BAND_SCAN_FIRST){metrics.cpu_bands++;draw_tail(0);}
   else{job.row=0;job.phase=BAND_ACQUIRE;}
  }else if(job.phase==BAND_SCAN_FIRST){
   if(!job.base_on_gpu){tracegpu_clip_set(job.canvas,&job.rect);tracegpu_dashboard_wave_base(job.canvas);tracegpu_clip_reset();}
   job.row=0;job.phase=BAND_HANDOFF;metrics.gpu_bands++;
  }else job.phase=BAND_START;
  return 0;
 }
 if(job.phase==BAND_START){
  /* Changing the color/configuration is legal only between completed jobs. */
  if(++batch_sequence==0)++batch_sequence;
  XTime_GetTime(&job.gpu_begin);
  rc=job.colored?job.gpu->begin_colored(job.context,job.canvas,batch_sequence,job.count):
    job.gpu->begin(job.context,job.canvas,job.color,batch_sequence,job.count);
  if(rc<0){tracegpu_cpu_fault();return -2;}
  job.submitted=0;job.phase=BAND_GPU;return 0;
 }
 if(job.phase==BAND_GPU){
  if(job.submitted<job.count){
   rc=job.colored?job.gpu->submit_colored(job.context,job.commands,job.command_colors,job.count,16):
     job.gpu->submit(job.context,job.commands,job.count,16);
   if(rc<0||(unsigned)rc>job.count-job.submitted){tracegpu_cpu_fault();return -3;}
   job.submitted+=(unsigned)rc;
  }
  uint32_t cycles=0;rc=job.gpu->poll(job.context,&cycles);
  if(rc<0){tracegpu_cpu_fault();return -4;}
  if(!rc)return 0;
  if(job.submitted!=job.count){tracegpu_cpu_fault();return -5;}
  XTime now;XTime_GetTime(&now);uint32_t elapsed=(uint32_t)(now-job.gpu_begin);
  metrics.gpu_wait_ticks+=elapsed;metrics.wave_ticks+=elapsed;metrics.pl_cycles+=cycles;
  metrics.gpu_batches++;metrics.gpu_rects+=job.count;metrics.gpu_pixels+=job.command_pixels;
  job.count=job.command_pixels=0;job.phase=BAND_SCAN_MORE;return 0;
 }
 if(job.phase==BAND_OVERLAY){draw_tail(1);return 0;}
 rc=tracegpu_damage_next(&damage,&job.rect);
 if(rc<0)return -1;
 if(!rc)return tracegpu_damage_ready(&damage);
 job.covered=(uint8_t)opaque_band();
 if(!job.covered&&!job.u->data_page){
  unsigned row=(unsigned)job.rect.y/16U;
  for(unsigned col=(unsigned)job.rect.x/32U;col<((unsigned)job.rect.x+(unsigned)job.rect.w+31U)/32U;col++){
   unsigned tile=row*TG_DAMAGE_COLS+col;
   if(job.wave_tiles[tile/32U]&(1U<<(tile%32U))){metrics.wave_dirty_bands++;wave_overlap=1;break;}
  }
 }
 if(job.gpu&&!job.covered&&!job.u->data_page&&job.plan->count&&!job.plan->overflow&&(uint32_t)job.rect.w*job.rect.h>=job.min_area){
  unsigned row=(unsigned)job.rect.y/16U;job.scan=job.first[row];job.limit=job.end[row];
  if(job.scan>job.limit)job.scan=job.limit;
  job.count=job.command_pixels=0;job.base_on_gpu=0;
  if(wave_overlap)seed_gpu_base();
  job.phase=BAND_SCAN_FIRST;
 }else{metrics.cpu_bands++;draw_tail(0);}
 return 0;
}
int tracegpu_cpu_step(unsigned front,tracegpu_cpu_flush_fn flush){
 XTime a,b;XTime_GetTime(&a);int rc=step(front,flush);XTime_GetTime(&b);
 metrics.work_ticks+=(uint32_t)(b-a);
 if(b-a>metrics.max_step_ticks)metrics.max_step_ticks=(uint32_t)(b-a);
 return rc;
}
int tracegpu_cpu_confirm(unsigned actual){int rc=tracegpu_damage_confirm(&damage,actual);if(!rc){metrics.frames++;completed=metrics;}return rc;}
void tracegpu_cpu_fault(void){tracegpu_damage_fault(&damage);tracegpu_clip_reset();tracegpu_wave_cpu_range(0,0,0);}
