#include "render_cpu.h"
#include <stdint.h>
static void gpu_partial_fixture(tracegpu_ui_state *,tracegpu_ui_shell *,const tracegpu_uart_event_ring *);
#define cpu_partial_fixture gpu_partial_fixture
#define TG_CPU_PARTIAL_TEST 1
#define main fixture_main
#include "preview_logic.c"
#undef main
#undef cpu_partial_fixture

/* This is deliberately not coherent RAM. Actual CPU painters see cache[],
 * GPU writes only ddr[], and only the callbacks below transfer ownership. */
enum {PIXELS=800*480,CACHE_LINES=PIXELS/8,MAX_BATCH=512};
static struct {
 uint32_t before;
 uint32_t cache[PIXELS] __attribute__((aligned(32)));
 uint32_t after;
} surface[2];
static uint32_t ddr[2][PIXELS],reference[PIXELS],front_cache_copy[PIXELS],front_ddr_copy[PIXELS],owned_cache_copy[PIXELS];
static uint8_t ownership[2][CACHE_LINES];
static unsigned front,back,frames,gpu_frames,cpu_frames,full_frames,small_frames;
static unsigned gpu_partial_frames,delayed_polls,delayed_flips,fault_cases,stress_frames,max_batch_seen;
static unsigned colored_jobs,colored_rectangles,legacy_stress_batches,colored_stress_batches;
static uint64_t batches,rectangles,gpu_pixels,flushed_bytes,invalidated_bytes;
static tracegpu_touch_frame touch;
static struct {
 uintptr_t canvas;
 tracegpu_render_rect rect[MAX_BATCH];
 uint32_t colors[MAX_BATCH];
 uint32_t color,expected,submitted,frame_id;
 unsigned active,delay,fail_mode,polls,colored;
} model;

static uint32_t checksum(const uint32_t *p){
 uint32_t h=0;for(unsigned i=0;i<PIXELS;i++)h=(h*33U)^p[i];return h;
}
static unsigned address_buffer(uintptr_t address,uint32_t bytes,unsigned *offset){
 assert(!(address&31U)&&!(bytes&31U)&&bytes&&bytes<=3200U);
 for(unsigned i=0;i<2;i++){
  uintptr_t begin=(uintptr_t)surface[i].cache,end=begin+sizeof surface[i].cache;
  if(address>=begin&&address+bytes<=end){*offset=(unsigned)((address-begin)/4U);return i;}
 }
 assert(!"cache operation outside either framebuffer");return 0;
}
static void unchanged_front(void){
 assert(!memcmp(surface[front].cache,front_cache_copy,sizeof front_cache_copy));
 assert(!memcmp(ddr[front],front_ddr_copy,sizeof front_ddr_copy));
 for(unsigned i=0;i<2;i++)assert(surface[i].before==0x12345678U&&surface[i].after==0x98765432U);
}
static void flush_cache(uintptr_t address,uint32_t bytes){
 unsigned offset,index=address_buffer(address,bytes,&offset);
 assert(index==back&&index!=front&&!model.active);
 memcpy(ddr[index]+offset,surface[index].cache+offset,bytes);
 for(unsigned q=offset/8U;q<(offset+bytes/4U)/8U;q++)ownership[index][q]|=1U;
 flushed_bytes+=bytes;
}
static void invalidate_cache(void *context,uintptr_t address,uint32_t bytes){
 (void)context;unsigned offset,index=address_buffer(address,bytes,&offset);
 assert(index==back&&index!=front&&!model.active);
 memcpy(surface[index].cache+offset,ddr[index]+offset,bytes);
 for(unsigned q=offset/8U;q<(offset+bytes/4U)/8U;q++)ownership[index][q]|=2U;
 invalidated_bytes+=bytes;
}
static void gpu_owns_cache_unchanged(void){
 unchanged_front();assert(model.active);
 assert(!memcmp(surface[back].cache,owned_cache_copy,sizeof owned_cache_copy));
}
static int gpu_begin(void *context,uintptr_t canvas,uint32_t color,uint32_t frame_id,uint32_t count){
 (void)context;assert(!model.active&&canvas==(uintptr_t)surface[back].cache);
 assert(count&&count<=MAX_BATCH);unchanged_front();
 if(count>max_batch_seen)max_batch_seen=count;
 model.canvas=canvas;model.color=color;model.expected=count;model.submitted=0;
 model.colored=0;
 model.frame_id=frame_id;memcpy(owned_cache_copy,surface[back].cache,sizeof owned_cache_copy);
 model.active=1;model.delay=2U+(frame_id%3U);model.polls=0;batches++;
 /* A start error can occur after an AXI writer has already been launched. */
 return model.fail_mode==1U?-2:0;
}
static int gpu_submit(void *context,const tracegpu_render_rect *rect,uint32_t count,uint32_t budget){
 (void)context;gpu_owns_cache_unchanged();assert(count==model.expected&&budget);
 if(model.fail_mode==2U)return -2;
 unsigned submitted=0;
 while(model.submitted<count&&submitted<budget){
  tracegpu_render_rect r=rect[model.submitted];assert(tracegpu_render_rect_valid(&r));
  /* Every cache line the GPU can touch must have been cleaned AND invalidated
   * during this frame, not just inherited from a previous successful frame. */
  for(unsigned y=r.y;y<(unsigned)r.y+r.h;y++){
   unsigned start=(y*800U+r.x)/8U,end=(y*800U+r.x+r.w-1U)/8U;
   for(unsigned q=start;q<=end;q++)assert(ownership[back][q]==3U);
  }
  model.rect[model.submitted++]=r;submitted++;rectangles++;
 }
 return (int)submitted;
}
static void gpu_complete(void){
 assert(model.active);
 for(unsigned q=0;q<model.submitted;q++){
  const tracegpu_render_rect *r=&model.rect[q];
  for(unsigned y=r->y;y<(unsigned)r->y+r->h;y++)
   for(unsigned x=r->x;x<(unsigned)r->x+r->w;x++)ddr[back][y*800U+x]=model.colored?model.colors[q]:model.color;
  gpu_pixels+=(uint32_t)r->w*r->h;
 }
 model.active=0;
}
static int gpu_poll(void *context,uint32_t *cycles){
 (void)context;gpu_owns_cache_unchanged();model.polls++;
 if(model.fail_mode==3U)return -3;
 if(model.fail_mode==4U)return -5; /* The write remains potentially in flight. */
 if(model.submitted<model.expected||model.delay){
  if(model.submitted==model.expected)model.delay--;
  delayed_polls++;return 0;
 }
 gpu_complete();*cycles=137U+model.expected*7U;return 1;
}
static int gpu_begin_colored(void *context,uintptr_t canvas,uint32_t frame_id,uint32_t count){
 int rc=gpu_begin(context,canvas,0,frame_id,count);model.colored=1;colored_jobs++;return rc;
}
static int gpu_submit_colored(void *context,const tracegpu_render_rect *rect,const uint32_t *colors,uint32_t count,uint32_t budget){
 unsigned first=model.submitted;int rc=gpu_submit(context,rect,count,budget);
 if(rc>0)for(unsigned q=first;q<model.submitted;q++){model.colors[q]=colors[q];colored_rectangles++;}
 return rc;
}
static const tracegpu_partial_gpu_ops ops={gpu_begin,gpu_submit,gpu_poll,invalidate_cache,0,0};
static const tracegpu_partial_gpu_ops colored_ops={gpu_begin,gpu_submit,gpu_poll,invalidate_cache,gpu_begin_colored,gpu_submit_colored};

static void reference_draw(tracegpu_ui_state *u,tracegpu_ui_shell *s,const tracegpu_uart_event_ring *r){
 tracegpu_clip_reset();tracegpu_dashboard_set_wave_plan(&plan);
 tracegpu_dashboard_draw_uart((uintptr_t)reference,u,r);
 tracegpu_dashboard_set_wave_plan(0);tracegpu_dashboard_draw_analysis((uintptr_t)reference,u);
 tracegpu_ui_shell_draw((uintptr_t)reference,s,u);
 tracegpu_dashboard_draw_touch_points((uintptr_t)reference,&touch);
}
static void reset_surfaces(void){
 tracegpu_cpu_reset();memset(&touch,0,sizeof touch);memset(&model,0,sizeof model);memset(ownership,0,sizeof ownership);
 front=0;back=1;
 for(unsigned i=0;i<2;i++){
  surface[i].before=0x12345678U;surface[i].after=0x98765432U;
  memset(surface[i].cache,0x5a,sizeof surface[i].cache);memset(ddr[i],0xa5,sizeof ddr[i]);
 }
}
static void compare_pixels(unsigned step){
 for(unsigned p=0;p<PIXELS;p++)if(surface[back].cache[p]!=reference[p]||ddr[back][p]!=reference[p]){
  fprintf(stderr,"GPU_PARTIAL_DIFFERENCE frame=%u step=%u x=%u y=%u cache=%08x ddr=%08x expected=%08x\n",
   frames,step,p%800U,p/800U,surface[back].cache[p],ddr[back][p],reference[p]);abort();
 }
}
static void render_frame(tracegpu_ui_state *u,tracegpu_ui_shell *s,const tracegpu_uart_event_ring *r,
                         unsigned step,int use_gpu,int full){
 back=front^1U;memcpy(front_cache_copy,surface[front].cache,sizeof front_cache_copy);memcpy(front_ddr_copy,ddr[front],sizeof front_ddr_copy);
 memset(ownership[back],0,sizeof ownership[back]);reference_draw(u,s,r);
 tracegpu_dashboard_cpu_latch(u,r);
 assert(!tracegpu_cpu_begin(back,front,(uintptr_t)surface[back].cache,u,u,s,&touch,&plan,full));
 if(use_gpu)assert(!tracegpu_cpu_set_gpu(use_gpu==2?&colored_ops:&ops,&model));
 assert(tracegpu_cpu_begin(back,front,(uintptr_t)surface[back].cache,u,u,s,&touch,&plan,full)<0);
 assert(tracegpu_cpu_confirm(back)<0);
 int rc=0;unsigned polls=0;
 while(!rc){rc=tracegpu_cpu_step(front,flush_cache);assert(rc>=0&&++polls<30000);unchanged_front();}
 assert(!model.active);assert(tracegpu_cpu_confirm(front)<0);compare_pixels(step);
 /* Keep VDMA on its old front for several scheduler polls even after READY. */
 for(unsigned q=0;q<3;q++){assert(tracegpu_cpu_step(front,flush_cache)==1);unchanged_front();delayed_flips++;}
 const tracegpu_cpu_metrics *m=tracegpu_cpu_stats();
 if(m->gpu_batches){gpu_frames++;if(m->tiles<750U)gpu_partial_frames++;}else cpu_frames++;
 if(m->tiles<188U&&m->flush_bytes<384000U)small_frames++;
 if(full)full_frames++;
 front=back;assert(!tracegpu_cpu_confirm(front));tracegpu_dashboard_cpu_release();frames++;
}
static void fault_tests(tracegpu_ui_state *u,tracegpu_ui_shell *s,const tracegpu_uart_event_ring *r){
 /* Faults are tested with a real, non-empty waveform, not just the mock API. */
 if(!plan.count||u->data_page||u->bus_mode)return;
 for(unsigned backend=0;backend<2;backend++)for(unsigned failure=1;failure<=4;failure++){
  reset_surfaces();memcpy(front_cache_copy,surface[front].cache,sizeof front_cache_copy);memcpy(front_ddr_copy,ddr[front],sizeof front_ddr_copy);
  model.fail_mode=failure;tracegpu_dashboard_cpu_latch(u,r);
  assert(!tracegpu_cpu_begin(back,front,(uintptr_t)surface[back].cache,u,u,s,&touch,&plan,0));
  assert(!tracegpu_cpu_set_gpu(backend?&colored_ops:&ops,&model));
  int rc=0;unsigned polls=0;
  while(!rc){rc=tracegpu_cpu_step(front,flush_cache);assert(++polls<30000);unchanged_front();}
  assert(rc<0&&model.active);uint32_t poisoned=checksum(surface[back].cache);
  /* A late DMA write after the reported error is allowed to reach DDR. It must
   * never be scanned out or overwritten by an optimistic CPU fallback. */
  ddr[back][120U*800U+100U]^=0x00ffffffU;gpu_complete();
  assert(tracegpu_cpu_step(front,flush_cache)<0);assert(tracegpu_cpu_confirm(back)<0);
  assert(tracegpu_cpu_begin(back,front,(uintptr_t)surface[back].cache,u,u,s,&touch,&plan,1)<0);
  assert(checksum(surface[back].cache)==poisoned);unchanged_front();
  tracegpu_dashboard_cpu_release();fault_cases++;
 }
}
static void stress_tests(tracegpu_ui_state *u,tracegpu_ui_shell *s,const tracegpu_uart_event_ring *r){
 static tracegpu_wave_plan saved;
 saved=plan;reset_surfaces();memset(&plan,0,sizeof plan);plan.dual=1;plan.count=2404;
 assert(TRACEGPU_WAVE_MAX_RECTS>=plan.count);
 /* More than the old 2,187-command limit, multiple >512 same-color runs,
  * overlapping colors, and both explicit and legacy fallback colors. */
 for(unsigned i=0;i<2400;i++){
  plan.rects[i]=(tracegpu_render_rect){(uint16_t)(100U+i%600U),(uint16_t)(120U+i%3U),13,5};
  plan.colors[i]=i<700U?0xff395a91U:i<1400U?0xffc36b4aU:i<2100U?0xff42c698U:0U;
 }
 plan.rects[2400]=(tracegpu_render_rect){104,180,300,3}; /* dual fallback orange */
 plan.rects[2401]=(tracegpu_render_rect){799,479,2,2}; /* invalid: CPU skips it */
 plan.rects[2402]=(tracegpu_render_rect){0,0,0,12}; /* invalid: zero width */
 plan.rects[2403]=(tracegpu_render_rect){107,121,500,3};plan.colors[2403]=0xffde53c9U;
 render_frame(u,s,r,1000,1,1);assert(tracegpu_cpu_stats()->gpu_rects>2187U);stress_frames++;
 legacy_stress_batches=tracegpu_cpu_stats()->gpu_batches;
 render_frame(u,s,r,1006,2,1);assert(tracegpu_cpu_stats()->gpu_rects>2187U);stress_frames++;
 colored_stress_batches=tracegpu_cpu_stats()->gpu_batches;
 assert(colored_stress_batches<legacy_stress_batches);
 render_frame(u,s,r,1001,0,1);assert(!tracegpu_cpu_stats()->gpu_batches);stress_frames++;
 touch.online=1;touch.count=1;touch.sequence++;touch.point[0].x=120;touch.point[0].y=121;
 render_frame(u,s,r,1002,1,0);assert(tracegpu_cpu_stats()->gpu_batches);stress_frames++;
 plan.overflow=1;
 render_frame(u,s,r,1003,1,1);assert(!tracegpu_cpu_stats()->gpu_batches);stress_frames++;
 plan.overflow=0;plan.count=0;
 render_frame(u,s,r,1004,1,1);assert(!tracegpu_cpu_stats()->gpu_batches);stress_frames++;
 plan.count=2;plan.rects[0]=(tracegpu_render_rect){799,479,2,2};plan.rects[1]=(tracegpu_render_rect){0,0,0,12};
 render_frame(u,s,r,1005,1,1);assert(!tracegpu_cpu_stats()->gpu_batches);stress_frames++;
 /* Corrupt plan membership is rejected before any out-of-bounds plan read. */
 plan.count=TRACEGPU_WAVE_MAX_RECTS+1U;
 assert(tracegpu_cpu_begin(front^1U,front,(uintptr_t)surface[front^1U].cache,u,u,s,&touch,&plan,1)<0);
 assert(tracegpu_cpu_set_gpu(&ops,&model)<0);
 plan=saved;
}
static void gpu_partial_fixture(tracegpu_ui_state *u,tracegpu_ui_shell *s,const tracegpu_uart_event_ring *r){
 static unsigned fixtures;
 if(!fixtures){fault_tests(u,s,r);stress_tests(u,s,r);}
 fixtures++;reset_surfaces();
 for(unsigned step=0;step<28;step++){
  if(step>=2&&step<=15){touch.online=1;touch.count=1;touch.sequence++;touch.point[0].x=(uint16_t)((step*71U)%800U);touch.point[0].y=(uint16_t)((step*43U)%480U);}
  if(step==16)touch.count=0;
  if(step==17)s->pressed=UI_START;
  if(step==18){s->pressed=0;s->motion=1;s->feedback_id=UI_START;s->now=100;s->feedback_until=240;}
  if(step==19){s->now=180;s->show_cpu=1;u->cpu_percent_valid=1;u->cpu_percent=100;}
  if(step==20){s->now=250;u->cpu_percent=9;}
  if(step==21){s->drawer=1;s->position=256;}
  if(step==22){s->drawer=0;s->position=0;}
  if(step==23){u->cursors.enabled=1;u->cursors.point[0].ticks=1300000;u->cursors.point[1].ticks=1700000;}
  if(step==24){u->time_window_ticks/=2;tracegpu_wave_build(&plan,u,r);}
  if(step==25){s->notice="GPU partial test";s->notice_until=400;s->now=300;}
  if(step==26)s->now=500;
  if(step==27)u->data_page=s->data_page=4;
  /* Alternate CPU/GPU without resetting the shared per-buffer epochs. The
   * forced-full frames also use both backends and must repair a stale back. */
  render_frame(u,s,r,step,(step%3U)?((step%2U)?2:1):0,(step%9U)==0U);
  if(step==20)memcpy(image.pixels,surface[front].cache,sizeof image.pixels);
 }
}
int main(int argc,char **argv){
 assert(argc==2);
 for(unsigned mode=0;mode<25;mode++){
  char value[16],file[1024];snprintf(value,sizeof value,"%u",mode);
  snprintf(file,sizeof file,"%s/mode_%u.bmp",argv[1],mode);
  char *args[]={argv[0],file,value,0};assert(!fixture_main(3,args));
 }
 assert(frames==707U&&gpu_frames&&cpu_frames&&gpu_partial_frames&&fault_cases==8U);
 assert(stress_frames==7U&&max_batch_seen==512U);
 assert(batches&&rectangles&&gpu_pixels&&flushed_bytes&&invalidated_bytes&&small_frames);
 printf("GPU_PARTIAL_PIXELS_PASS modes=25 frames=%u gpu_frames=%u cpu_frames=%u forced_full=%u small_frames=%u gpu_partial_frames=%u differences=0 board_tested=0\n",
  frames,gpu_frames,cpu_frames,full_frames,small_frames,gpu_partial_frames);
 printf("GPU_PARTIAL_CACHE_PASS batches=%llu rects=%llu gpu_pixels=%llu flush_bytes=%llu invalidate_bytes=%llu delayed_polls=%u delayed_flips=%u fault_cases=%u cache_ddr_separate=1 quarantine=1\n",
  (unsigned long long)batches,(unsigned long long)rectangles,(unsigned long long)gpu_pixels,
  (unsigned long long)flushed_bytes,(unsigned long long)invalidated_bytes,delayed_polls,delayed_flips,fault_cases);
 printf("GPU_PARTIAL_STRESS_PASS frames=%u plan_rectangles=2404 max_batch=%u overlapping_colors=1 invalid_rect_skip=1 overflow_cpu_fallback=1 invalid_count_rejected=1\n",stress_frames,max_batch_seen);
 assert(colored_jobs&&colored_rectangles&&colored_stress_batches<legacy_stress_batches);
 printf("GPU_PARTIAL_COLORED_PASS colored_jobs=%u colored_rectangles=%u legacy_stress_batches=%u colored_stress_batches=%u order_preserved=1 both_backend_faults=8\n",colored_jobs,colored_rectangles,legacy_stress_batches,colored_stress_batches);
 return 0;
}
