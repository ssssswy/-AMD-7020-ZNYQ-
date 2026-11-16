#include "render_cpu.h"
#include <stdint.h>
static void cpu_partial_fixture(tracegpu_ui_state *,tracegpu_ui_shell *,const tracegpu_uart_event_ring *);
#define TG_CPU_PARTIAL_TEST 1
#define main fixture_main
#include "preview_logic.c"
#undef main
static struct {uint32_t before;uint32_t pixels[800*480] __attribute__((aligned(32)));uint32_t after;} surface[2];
static uint32_t reference[800*480];
static unsigned front,back,frames,small_updates;
static uint64_t total_pixels;
static tracegpu_touch_frame touch;
static uint32_t checksum(const uint32_t *p){uint32_t h=0;for(unsigned i=0;i<800*480;i++)h=(h*33)^p[i];return h;}
static void flush_check(uintptr_t a,uint32_t n){
 assert(!(a&31)&&!(n&31)&&n>0&&n<=3200);
 assert(a>=(uintptr_t)surface[back].pixels&&a+n<=(uintptr_t)(surface[back].pixels+800*480));
 assert(a<(uintptr_t)surface[front].pixels||a>=(uintptr_t)(surface[front].pixels+800*480));
}
static void reference_draw(tracegpu_ui_state *u,tracegpu_ui_shell *s){
 tracegpu_clip_reset();tracegpu_dashboard_set_wave_plan(&plan);
 tracegpu_dashboard_draw_uart((uintptr_t)reference,u,&rings[0]);
 tracegpu_dashboard_set_wave_plan(0);tracegpu_dashboard_draw_analysis((uintptr_t)reference,u);
 tracegpu_ui_shell_draw((uintptr_t)reference,s,u);tracegpu_dashboard_draw_touch_points((uintptr_t)reference,&touch);
}
static void cpu_partial_fixture(tracegpu_ui_state *u,tracegpu_ui_shell *s,const tracegpu_uart_event_ring *r){
 tracegpu_cpu_reset();memset(&touch,0,sizeof touch);front=0;
 for(unsigned i=0;i<2;i++){surface[i].before=0x12345678;surface[i].after=0x98765432;memset(surface[i].pixels,0x5a,sizeof surface[i].pixels);}
 /* All actual page/glyph/wave writers must preserve pixels outside the clip. */
 const tracegpu_damage_rect probes[]={{0,0,32,16},{768,464,32,16},{96,112,32,16}};
 for(unsigned q=0;q<3;q++){
  memset(surface[1].pixels,0x5a,sizeof surface[1].pixels);tracegpu_clip_set((uintptr_t)surface[1].pixels,&probes[q]);
  tracegpu_dashboard_set_wave_plan(&plan);tracegpu_dashboard_draw_uart((uintptr_t)surface[1].pixels,u,r);
  tracegpu_dashboard_set_wave_plan(0);tracegpu_dashboard_draw_analysis((uintptr_t)surface[1].pixels,u);
  tracegpu_ui_shell_draw((uintptr_t)surface[1].pixels,s,u);tracegpu_clip_reset();
  for(unsigned p=0;p<800*480;p++)if((int)(p%800)<probes[q].x||(int)(p%800)>=probes[q].x+probes[q].w||(int)(p/800)<probes[q].y||(int)(p/800)>=probes[q].y+probes[q].h)assert(surface[1].pixels[p]==0x5a5a5a5a);
 }
 for(unsigned step=0;step<28;step++){
  if(step>=2&&step<=15){touch.online=1;touch.count=1;touch.sequence++;touch.point[0].x=(uint16_t)((step*71)%800);touch.point[0].y=(uint16_t)((step*43)%480);}
  if(step==16)touch.count=0;
  if(step==17)s->pressed=UI_START;
  if(step==18){s->pressed=0;s->motion=1;s->feedback_id=UI_START;s->now=100;s->feedback_until=240;}
  if(step==19){s->now=180;s->show_cpu=1;u->cpu_percent_valid=1;u->cpu_percent=100;}
  if(step==20){s->now=250;u->cpu_percent=9;}
  if(step==21){s->drawer=1;s->position=256;}
  if(step==22){s->drawer=0;s->position=0;}
  if(step==23){u->cursors.enabled=1;u->cursors.point[0].ticks=1300000;u->cursors.point[1].ticks=1700000;}
  if(step==24){u->time_window_ticks/=2;tracegpu_wave_build(&plan,u,r);}
  if(step==25){s->notice="CPU partial test";s->notice_until=400;s->now=300;}
  if(step==26)s->now=500;
  if(step==27)u->data_page=s->data_page=4;
  reference_draw(u,s);back=front^1U;uint32_t held=checksum(surface[front].pixels);
  tracegpu_dashboard_cpu_latch(u,r);
  assert(!tracegpu_cpu_begin(back,front,(uintptr_t)surface[back].pixels,u,u,s,&touch,&plan,0));
  assert(tracegpu_cpu_begin(back,front,(uintptr_t)surface[back].pixels,u,u,s,&touch,&plan,0)<0);
  assert(tracegpu_cpu_confirm(back)<0);
  int rc=0;unsigned polls=0;while(!rc){rc=tracegpu_cpu_step(front,flush_check);assert(rc>=0&&++polls<2000);}
  assert(tracegpu_cpu_confirm(front)<0); /* A requested flip is not an actual flip. */
  assert(checksum(surface[front].pixels)==held);
  for(unsigned p=0;p<800*480;p++)if(surface[back].pixels[p]!=reference[p]){
   fprintf(stderr,"PIXEL_DIFFERENCE frame=%u step=%u x=%u y=%u got=%08x expected=%08x\n",frames,step,p%800,p/800,surface[back].pixels[p],reference[p]);abort();
  }
  const tracegpu_cpu_metrics *m=tracegpu_cpu_stats();
  if(step>=3&&step<=15){assert(m->tiles<188);assert(m->flush_bytes<384000);small_updates++;}
  total_pixels+=m->painted_pixels;front=back;assert(!tracegpu_cpu_confirm(front));tracegpu_dashboard_cpu_release();
  for(unsigned i=0;i<2;i++)assert(surface[i].before==0x12345678&&surface[i].after==0x98765432);
  frames++;
  if(step==20)memcpy(image.pixels,surface[front].pixels,sizeof image.pixels);
 }
}
static void damage_tests(void){
 static tracegpu_damage d;tracegpu_damage_rect r;unsigned visible=0;uint32_t random=12345;
 tracegpu_damage_init(&d);assert(tracegpu_damage_begin(&d,0,0,0)<0);assert(tracegpu_damage_confirm(&d,1)<0);
 for(unsigned frame=0;frame<10000;frame++){
  random=random*1664525+1013904223;tracegpu_damage_mark(&d,(int)(random%850)-25,(int)((random>>12)%530)-25,55,35);
  unsigned b=visible^1;assert(!tracegpu_damage_begin(&d,b,visible,frame%17==0));
  if(frame==5)tracegpu_damage_mark(&d,96,112,32,16); /* New desired version while target is frozen. */
  unsigned painted=0;int rc;while((rc=tracegpu_damage_next(&d,&r))>0){tracegpu_damage_finish(&d,&r);painted++;}
  assert(!rc&&painted<=750&&tracegpu_damage_ready(&d)==1);
  assert(tracegpu_damage_confirm(&d,visible)<0);visible=b;assert(!tracegpu_damage_confirm(&d,visible));
 }
 tracegpu_damage_fault(&d);assert(tracegpu_damage_begin(&d,visible^1,visible,0)<0);
 tracegpu_damage_init(&d);d.epoch=UINT32_MAX;tracegpu_damage_all(&d);assert(d.fault);
 int x=INT32_MIN,y=0,w=INT32_MAX,h=1;assert(!tracegpu_clip_rect(0,&x,&y,&w,&h));
 puts("DAMAGE_STATE_PASS swaps=10000 delayed_confirm mid_job_invalidation quarantine epoch_wrap");
}
int main(int argc,char **argv){
 assert(argc==2);damage_tests();
 for(unsigned mode=0;mode<25;mode++){
  char value[16],file[1024];snprintf(value,sizeof value,"%u",mode);snprintf(file,sizeof file,"%s/mode_%u.bmp",argv[1],mode);
  char *args[]={argv[0],file,value,0};assert(!fixture_main(3,args));
 }
 printf("CPU_PARTIAL_PIXELS_PASS modes=25 frames=%u small_updates=%u changed_pixels=%llu differences=0 board_tested=0\n",frames,small_updates,(unsigned long long)total_pixels);
 printf("CPU_PARTIAL_SIZES damage=%u ui=%u shell=%u ARM_stack_large_copies=0\n",(unsigned)sizeof(tracegpu_damage),(unsigned)sizeof(tracegpu_ui_state),(unsigned)sizeof(tracegpu_ui_shell));
 return 0;
}
