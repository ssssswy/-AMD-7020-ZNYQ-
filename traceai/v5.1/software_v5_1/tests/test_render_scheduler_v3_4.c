/* The script creates an exact copy of test_render_gpu_partial.c with ONLY its
 * final entry point renamed. This reuses the actual painter/cache model. */
#define XTime_GetTime gpu_fixture_zero_time
#include "test_render_gpu_partial_embedded.c"
#undef XTime_GetTime
/* The older pixel fixture intentionally uses time=0. Scheduler policy needs
 * a monotonic nonzero completed-work interval; this is deterministic test
 * time, not PC or board performance. */
static uint64_t scheduler_clock;
void XTime_GetTime(uint64_t *ticks){*ticks=++scheduler_clock;}
#include "render_scheduler.h"
#include "ui_freeze_snapshot.h"
#include "performance_monitor.h"
#include "video_pipeline.h"

#undef TRACEGPU_UI_CANVAS_0
#undef TRACEGPU_UI_CANVAS_1
#define TRACEGPU_UI_CANVAS_0 ((uintptr_t)surface[0].cache)
#define TRACEGPU_UI_CANVAS_1 ((uintptr_t)surface[1].cache)
static unsigned video_front,video_requested=UINT32_MAX,video_requests;
static unsigned scheduler_frames,scheduler_gpu_frames,scheduler_cpu_frames;
static unsigned scheduler_faults,scheduler_fallbacks,scheduler_full_frames;
static unsigned publication_checks,freeze_cases,empty_wave_cases,perf_enter_calls,perf_leave_calls,planning_yields;
static unsigned inject_overflow,inject_invalid_rectangles;
static tracegpu_cpu_metrics old_partial_completed;
static tracegpu_render_bucket unconfirmed_policy[TRACEGPU_RENDER_PARTIAL_BUCKETS];
static unsigned partial_policy_samples;
static void scheduler_freeze_resolve(tracegpu_ui_state *s,tracegpu_wave_plan *p,
 const tracegpu_uart_event_ring *r,int frozen,int take){
 tracegpu_freeze_resolve(s,p,r,frozen,take);
 /* Fault injection is limited to the produced PLAN. All freeze resolution and
  * frozen/live state handling still execute their maintained implementation. */
 if(inject_overflow)p->overflow=1;
 if(inject_invalid_rectangles)for(unsigned i=0;i<p->count;i++)p->rects[i].w=0;
}
#define tracegpu_freeze_resolve scheduler_freeze_resolve
#include "../src/render_scheduler.c"
#undef tracegpu_freeze_resolve

unsigned tracegpu_video_current_buffer(void){return video_front;}
void tracegpu_video_select(unsigned index){
 assert(index==back&&index!=front&&!model.active);video_requested=index;video_requests++;
}
void Xil_DCacheFlushRange(uintptr_t address,unsigned length){
 /* The settings compatibility path deliberately flushes a complete frame. */
 while(length){unsigned n=length>3200U?3200U:length;flush_cache(address,n);address+=n;length-=n;}
}
void Xil_DCacheInvalidateRange(uintptr_t address,unsigned length){
 while(length){unsigned n=length>3200U?3200U:length;invalidate_cache(0,address,n);address+=n;length-=n;}
}
uint32_t Xil_In32(uintptr_t address){(void)address;assert(!"unexpected MMIO read");return 0;}
void Xil_Out32(uintptr_t address,uint32_t value){(void)address;(void)value;assert(!"unexpected MMIO write");}
void tracegpu_perf_enter(unsigned category,uint64_t now){(void)now;assert(category==TG_PERF_CACHE);perf_enter_calls++;}
void tracegpu_perf_leave(uint64_t now){(void)now;perf_leave_calls++;}
int tracegpu_gpu_batch_begin_color(tracegpu_gpu_batch_job *j,uintptr_t canvas,
 uint32_t frame_id,uint32_t expected,uint32_t color){
 int rc=gpu_begin(0,canvas,color,frame_id,expected);
 j->expected=expected;j->submitted=0;j->frame_id=frame_id;j->cycles=0;j->active=1;j->sealed=0;
 return rc;
}
/* This regression deliberately exercises the supported legacy ABI. The V2
   mixed-color engine and real MMIO driver are covered by their own suites. */
int tracegpu_gpu_batch_colored_available(void){return 0;}
int tracegpu_gpu_batch_begin_colored(tracegpu_gpu_batch_job *j,uintptr_t canvas,uint32_t id,uint32_t count){
 (void)j;(void)canvas;(void)id;(void)count;assert(!"V2 must not be selected for legacy capabilities");return -100;
}
int tracegpu_gpu_batch_submit_colored(tracegpu_gpu_batch_job *j,const tracegpu_render_rect *r,const uint32_t *colors,uint32_t count,uint32_t budget){
 (void)j;(void)r;(void)colors;(void)count;(void)budget;assert(!"V2 descriptor on legacy path");return -100;
}
int tracegpu_gpu_batch_submit(tracegpu_gpu_batch_job *j,const tracegpu_render_rect *r,uint32_t count,uint32_t budget){
 int rc=gpu_submit(0,r,count,budget);j->submitted=model.submitted;
 j->sealed=(uint8_t)(j->submitted==j->expected);return rc;
}
int tracegpu_gpu_batch_poll(tracegpu_gpu_batch_job *j){
 uint32_t cycles=0;int rc=gpu_poll(0,&cycles);
 if(rc>0){j->cycles=cycles;j->active=0;}return rc;
}
int tracegpu_gpu_batch_begin(tracegpu_gpu_batch_job *j,uintptr_t canvas,uint32_t id,uint32_t count){
 (void)j;(void)canvas;(void)id;(void)count;assert(!"legacy full GPU path must not execute");return -100;
}
int tracegpu_gpu_rect_begin(tracegpu_gpu_job *j,uintptr_t canvas,const tracegpu_render_rect *r){
 (void)j;(void)canvas;(void)r;assert(!"legacy rect path must fall back before ownership");return -100;
}
int tracegpu_gpu_job_poll(tracegpu_gpu_job *j){(void)j;assert(!"legacy rect poll must not execute");return -100;}

static void scheduler_hold_front(void){
 memcpy(front_cache_copy,surface[front].cache,sizeof front_cache_copy);
 memcpy(front_ddr_copy,ddr[front],sizeof front_ddr_copy);
}
static void scheduler_reference(const tracegpu_render_scheduler *s,const tracegpu_uart_event_ring *r,int settings){
 tracegpu_clip_reset();tracegpu_dashboard_set_wave_plan(settings?0:&s->plan);
 if(settings)tracegpu_dashboard_draw_settings((uintptr_t)reference,&s->snapshot);
 else tracegpu_dashboard_draw_uart((uintptr_t)reference,&s->snapshot,r);
 tracegpu_dashboard_set_wave_plan(0);
 tracegpu_dashboard_draw_analysis((uintptr_t)reference,&s->snapshot);
 tracegpu_ui_shell_draw((uintptr_t)reference,&s->shell,&s->live);
 tracegpu_dashboard_draw_touch_points((uintptr_t)reference,&s->touch);
}
static void scheduler_initialise(tracegpu_render_scheduler *s,tracegpu_gpu_display_stats *stats,
 tracegpu_ui_state *u){
 reset_surfaces();video_front=front;video_requested=UINT32_MAX;
 memset(s,0,sizeof *s);memset(stats,0,sizeof *stats);stats->active=1;
 tracegpu_ui_shell_init(&s->shell,u);s->shell.protocol_mode=3;s->shell.motion=0;
 s->batch_available=s->wave_available=1;s->requested=0;
 s->completed_metrics.frame_id=0x5a5aU;s->completed_metrics.valid=1;
 inject_overflow=inject_invalid_rectangles=0;
 memcpy(unconfirmed_policy,s->policy.partial,sizeof unconfirmed_policy);
}
static void check_not_published(const tracegpu_render_scheduler *s,const tracegpu_gpu_display_stats *stats,
 const tracegpu_render_metrics *old,uint32_t old_frames,uint32_t old_ok){
 assert(s->frames==old_frames&&stats->frames_ok==old_ok);
 assert(!memcmp(&s->completed_metrics,old,sizeof *old));publication_checks++;
 assert(!memcmp(tracegpu_cpu_completed(),&old_partial_completed,sizeof old_partial_completed));
 assert(!memcmp(s->policy.partial,unconfirmed_policy,sizeof unconfirmed_policy));
}
static void scheduler_frame(tracegpu_render_scheduler *s,tracegpu_gpu_display_stats *stats,
 tracegpu_ui_state *u,int settings,unsigned expected_reason,int expected_gpu){
 back=front^1U;video_front=front;video_requested=UINT32_MAX;scheduler_hold_front();
 memset(ownership[back],0,sizeof ownership[back]);
 tracegpu_render_metrics old=s->completed_metrics;uint32_t old_frames=s->frames,old_ok=stats->frames_ok;
 old_partial_completed=*tracegpu_cpu_completed();
 assert(!tracegpu_render_begin(s,u,&rings[0],&touch,settings,back));
 memcpy(unconfirmed_policy,s->policy.partial,sizeof unconfirmed_policy);
 assert(tracegpu_render_begin(s,u,&rings[0],&touch,settings,back)<0);
 while(s->phase==9U){
  assert(!model.active&&video_requested==UINT32_MAX);
  assert(!tracegpu_render_poll(s,stats));planning_yields++;assert(planning_yields<30000U);
  unchanged_front();check_not_published(s,stats,&old,old_frames,old_ok);
 }
 scheduler_reference(s,&rings[0],settings);
 check_not_published(s,stats,&old,old_frames,old_ok);
 unsigned polls=0;
 while(s->phase!=5U){
  assert(!tracegpu_render_poll(s,stats));assert(s->phase!=6U&&++polls<30000U);
  check_not_published(s,stats,&old,old_frames,old_ok);unchanged_front();
 }
 assert(video_requested==back&&!model.active);compare_pixels(2000U+scheduler_frames);
 for(unsigned q=0;q<4;q++){
  assert(!tracegpu_render_poll(s,stats));assert(s->phase==5U);
  check_not_published(s,stats,&old,old_frames,old_ok);unchanged_front();
 }
 front=back;video_front=front;
 assert(tracegpu_render_poll(s,stats)==1&&s->phase==0U);
 assert(s->frames==old_frames+1U&&stats->frames_ok==old_ok+1U);
 assert(s->completed_metrics.valid&&s->completed_metrics.frame_id==s->frames);
 unsigned policy_delta=0;
 for(unsigned i=0;i<TRACEGPU_RENDER_PARTIAL_BUCKETS;i++){
  unsigned before=unconfirmed_policy[i].cpu_samples+unconfirmed_policy[i].gpu_samples;
  unsigned after=s->policy.partial[i].cpu_samples+s->policy.partial[i].gpu_samples;
  assert(after>=before&&after<=before+1U);policy_delta+=after-before;
 }
 assert(policy_delta<=1U);
 if(s->requested==2U)printf("PARTIAL_POLICY_SAMPLE frame=%u delta=%u tiles=%u dirty_wave=%u attempted=%u gpu=%u invalid=%u active=%u\n",scheduler_frames,policy_delta,tracegpu_cpu_stats()->tiles,tracegpu_cpu_stats()->wave_dirty_bands,s->policy_requested_gpu,s->use_wave,s->batch.frame_perf.invalid_jobs,s->snapshot.gpu_active);
 if(policy_delta){assert(s->requested==2U&&s->cpu_partial&&!s->cpu_full_reference&&tracegpu_cpu_stats()->wave_dirty_bands);partial_policy_samples++;}
 if(s->completed_metrics.path_reason!=expected_reason){
  fprintf(stderr,"SCHEDULER_REASON_DIFFERENCE test_frame=%u requested=%u actual=%u expected=%u gpu=%u tiles=%u\n",
   scheduler_frames,s->requested,s->completed_metrics.path_reason,expected_reason,s->completed_metrics.gpu_wave,tracegpu_cpu_stats()->tiles);
  abort();
 }
 assert((s->completed_metrics.gpu_wave!=0U)==(expected_gpu!=0));
 assert((s->use_wave!=0U)==(expected_gpu!=0));
 if(expected_gpu){assert(s->completed_metrics.pl_cycles);scheduler_gpu_frames++;}
 else scheduler_cpu_frames++;
 if(s->cpu_partial)assert(!memcmp(tracegpu_cpu_completed(),tracegpu_cpu_stats(),sizeof old_partial_completed));
 if(s->cpu_full_reference)scheduler_full_frames++;
 if(expected_reason==TG_RENDER_REASON_CPU_FALLBACK||expected_reason==TG_RENDER_REASON_GPU_UNAVAILABLE)scheduler_fallbacks++;
 if(expected_reason==TG_RENDER_REASON_NO_DIRTY_WAVE)empty_wave_cases++;
 scheduler_frames++;
}
static void scheduler_fault_case(tracegpu_render_scheduler *s,tracegpu_gpu_display_stats *stats,
 tracegpu_ui_state *u,unsigned fault){
 scheduler_initialise(s,stats,u);s->requested=1;back=front^1U;scheduler_hold_front();
 model.fail_mode=fault;tracegpu_render_metrics old=s->completed_metrics;
 old_partial_completed=*tracegpu_cpu_completed();
 assert(!tracegpu_render_begin(s,u,&rings[0],&touch,0,back));
 memcpy(unconfirmed_policy,s->policy.partial,sizeof unconfirmed_policy);
 unsigned polls=0;int rc=0;
 while(!rc){rc=tracegpu_render_poll(s,stats);assert(++polls<30000U);unchanged_front();}
 assert(rc<0&&s->phase==6U&&stats->frames_failed==1U&&!stats->active);
 assert(model.active&&video_requested==UINT32_MAX);check_not_published(s,stats,&old,0,0);
 uint32_t cache_before=checksum(surface[back].cache);
 ddr[back][121U*800U+103U]^=0x00ffffffU;gpu_complete();
 for(unsigned q=0;q<8;q++){
  assert(!tracegpu_render_poll(s,stats));assert(s->phase==6U&&stats->frames_failed==1U);
  assert(tracegpu_render_begin(s,u,&rings[0],&touch,0,back)<0);unchanged_front();
  check_not_published(s,stats,&old,0,0);
 }
 assert(checksum(surface[back].cache)==cache_before);scheduler_faults++;
}
static void scheduler_tests(void){
 static tracegpu_render_scheduler s;static tracegpu_gpu_display_stats stats;static tracegpu_ui_state u;
 memset(&u,0,sizeof u);tracegpu_vector_stream_init(&stream,records,128,0,0);
 put(1000000,TG_VEC_INIT,255);unsigned levels=255;
 for(unsigned i=1;i<50;i++){levels^=1U<<(i%8U);put(1000000U+i*10000U,TG_VEC_CHANGE,levels);}
 put(2000000,TG_VEC_END,levels);
 u.logic_mode=u.dual_channel=u.quad_channel=1;u.channel_enabled_mask=15;
 u.logic_capture_mask=255;u.logic_channel_count=8;u.time_window_ticks=1000000;
 u.view_anchor_ticks=u.latest_event_ticks=2000000;u.logic_begin=1000000;u.logic_end=2000000;
 u.logic_records=stream.history.count;u.drain_state=2;u.gpu_active=1;
 for(unsigned i=0;i<8;i++){tracegpu_uart_ring_vector_view(&rings[i],&stream,i);u.channel_ring[i]=&rings[i];}
 for(unsigned i=0;i<8;i++)u.logic_rows[u.logic_count++]=tracegpu_vector_unpack(tracegpu_vector_oldest(&stream.history,stream.history.count-8U+i));
 scheduler_initialise(&s,&stats,&u);
 scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_CPU_REQUEST,0);
 s.requested=1;touch.online=1;touch.count=1;touch.sequence++;touch.point[0].x=150;touch.point[0].y=140;
 scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_GPU_REQUEST,1);
 s.requested=2;u.view_offset_ticks+=1000;
 /* A cold partial bin now samples CPU first and balances its own confirmed
  * samples. Do not depend on the unrelated legacy global probe toggle. */
 scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_AUTO_PROBE,0);
 unsigned probe_bin=s.policy_bucket;
 assert(s.policy.partial[probe_bin].cpu_samples==1U&&!s.policy.partial[probe_bin].gpu_samples);
 u.view_offset_ticks+=1000;scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_AUTO_PROBE,1);
 assert(s.policy_bucket==probe_bin&&s.policy.partial[probe_bin].cpu_samples==1U&&s.policy.partial[probe_bin].gpu_samples==1U);
 /* Explicit host-policy inputs, not measured hardware costs. */
 for(unsigned i=0;i<TRACEGPU_RENDER_PARTIAL_BUCKETS;i++)s.policy.partial[i]=(tracegpu_render_bucket){100,200,2,2};
 u.view_offset_ticks+=1000;scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_AUTO_CPU,0);
 for(unsigned i=0;i<TRACEGPU_RENDER_PARTIAL_BUCKETS;i++)s.policy.partial[i]=(tracegpu_render_bucket){200,100,2,2};
 u.view_offset_ticks+=1000;scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_AUTO_GPU,1);
 s.requested=1;s.cpu_full_reference=1;
 scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_FULL_CPU,0);assert(tracegpu_cpu_stats()->tiles==750U);
 s.cpu_full_reference=0;s.batch_available=0;u.view_offset_ticks+=1000;
 scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_GPU_UNAVAILABLE,0);
 s.batch_available=1;s.wave_available=0;u.view_offset_ticks+=1000;
 scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_GPU_UNAVAILABLE,0);
 s.wave_available=1;u.gpu_active=0;u.view_offset_ticks+=1000;
 scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_GPU_UNAVAILABLE,0);
 u.gpu_active=1;u.data_page=s.shell.data_page=4;
 scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_CPU_FALLBACK,0);
 u.data_page=s.shell.data_page=0;inject_overflow=1;u.view_offset_ticks+=1000;
 scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_CPU_FALLBACK,0);
 inject_overflow=0;inject_invalid_rectangles=1;u.view_offset_ticks+=1000;
 scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_NO_DIRTY_WAVE,0);inject_invalid_rectangles=0;
 /* The unchanged warmed-up view contains no dirty wave even if GPU requested. */
 u.view_offset_ticks+=1000;scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_GPU_REQUEST,1);
 scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_GPU_REQUEST,1);
 scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_NO_DIRTY_WAVE,0);
 assert(!tracegpu_cpu_stats()->gpu_batches&&!tracegpu_cpu_stats()->tiles);
 s.requested=2;touch.sequence++;touch.point[0].x++;touch.point[0].y++;
 scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_NO_DIRTY_WAVE,0);
 assert(tracegpu_cpu_stats()->tiles>0U&&tracegpu_cpu_stats()->tiles<188U);
 assert(tracegpu_cpu_stats()->cpu_bands&&!tracegpu_cpu_stats()->gpu_batches);
 s.requested=0;scheduler_frame(&s,&stats,&u,1,TG_RENDER_REASON_NO_WAVE,0);
 s.requested=1;s.shell.freeze_pending=1;u.view_offset_ticks+=1000;
 scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_GPU_REQUEST,1);freeze_cases++;
 s.shell.freeze_pending=0;s.shell.frozen=1;u.view_anchor_ticks+=100000;u.running=1;
 scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_GPU_REQUEST,1);
 assert(s.snapshot.frozen&&s.snapshot.view_anchor_ticks!=s.live.view_anchor_ticks);
 assert(s.snapshot.running!=s.live.running);freeze_cases++;
 s.shell.frozen=0;u.running=1;u.view_offset_ticks+=20000;
 scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_GPU_REQUEST,1);
 s.shell.freeze_pending=1;u.view_offset_ticks+=20000;
 scheduler_frame(&s,&stats,&u,0,TG_RENDER_REASON_GPU_REQUEST,1);
 assert(s.freeze_ready&&s.snapshot.frozen&&planning_yields>1U);freeze_cases++;
 s.freeze_ready=0;s.shell.freeze_pending=0;
 /* Producer reset during an unfinished plan must cancel before framebuffer/GPU ownership. */
 back=front^1U;video_front=front;video_requested=UINT32_MAX;scheduler_hold_front();
 assert(!tracegpu_render_begin(&s,&u,&rings[0],&touch,0,back)&&s.phase==9U);
 unsigned failures_before=s.failures;tracegpu_wave_lod_reset();
 assert(!tracegpu_render_poll(&s,&stats)&&!s.phase&&!s.freeze_ready&&s.failures==failures_before);
 assert(video_requested==UINT32_MAX&&!model.active);unchanged_front();
 printf("SLICED_SCHEDULER_PASS yields=%u live_frames=2 freeze_commit=1 stale_cancel_before_ownership=1\n",planning_yields);
 s.shell.frozen=0;u.running=0;
 for(unsigned fault=1;fault<=4;fault++)scheduler_fault_case(&s,&stats,&u,fault);
 /* An invalid hardware frame-store ID must fail closed before any repaint. */
 scheduler_initialise(&s,&stats,&u);back=front^1U;scheduler_hold_front();
 tracegpu_render_metrics old=s.completed_metrics;
 old_partial_completed=*tracegpu_cpu_completed();
 assert(!tracegpu_render_begin(&s,&u,&rings[0],&touch,0,back));video_front=2;
 assert(tracegpu_render_poll(&s,&stats)<0&&s.phase==6U);video_front=front;
 assert(!model.active&&video_requested==UINT32_MAX);unchanged_front();check_not_published(&s,&stats,&old,0,0);scheduler_faults++;
 /* READY is still not presented if the scanout-switch deadline expires. */
 scheduler_initialise(&s,&stats,&u);back=front^1U;scheduler_hold_front();old=s.completed_metrics;
 old_partial_completed=*tracegpu_cpu_completed();
 assert(!tracegpu_render_begin(&s,&u,&rings[0],&touch,0,back));
 unsigned polls=0;while(s.phase!=5U){assert(!tracegpu_render_poll(&s,&stats)&&++polls<30000U);}
 s.switch_deadline=0;assert(tracegpu_render_poll(&s,&stats)<0&&s.phase==6U);
 unchanged_front();check_not_published(&s,&stats,&old,0,0);scheduler_faults++;
 assert(scheduler_frames==22U&&scheduler_faults==6U&&freeze_cases==3U);
 assert(partial_policy_samples>=4U);
 printf("PARTIAL_POLICY_CONFIRM_PASS trained_frames=%u before_flip_samples_unchanged=1\n",partial_policy_samples);
 assert(perf_enter_calls&&perf_enter_calls==perf_leave_calls);
 printf("SCHEDULER_INTEGRATION_PASS frames=%u gpu_frames=%u cpu_frames=%u fallback_frames=%u forced_full=%u publication_checks=%u fault_cases=%u freeze_cases=%u no_dirty_wave=%u video_requests=%u actual_flip_barrier=1 cache_ddr_separate=1 legacy_gpu_unused=1 board_tested=0\n",
  scheduler_frames,scheduler_gpu_frames,scheduler_cpu_frames,scheduler_fallbacks,scheduler_full_frames,
  publication_checks,scheduler_faults,freeze_cases,empty_wave_cases,video_requests);
}
int main(int argc,char **argv){
 setvbuf(stdout,0,_IONBF,0);assert(!gpu_suite_main(argc,argv));scheduler_tests();return 0;
}
