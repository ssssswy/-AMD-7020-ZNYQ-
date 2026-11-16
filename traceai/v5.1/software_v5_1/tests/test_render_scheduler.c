#include "render_scheduler.h"
#include <assert.h>
#include <stdio.h>
static XTime now;static unsigned scan,selected,rect_started,overlay,touches;
static int completion,hardware_error;
static int batch_done;
uint32_t tracegpu_gpu_batch_version(void){return 0;}
int tracegpu_gpu_batch_selftest(void){return -4;}
int tracegpu_gpu_batch_begin(tracegpu_gpu_batch_job*j,uintptr_t c,uint32_t f,uint32_t e){(void)c;j->active=1;j->frame_id=f;j->expected=e;return 0;}
int tracegpu_gpu_batch_submit(tracegpu_gpu_batch_job*j,const tracegpu_render_rect*r,uint32_t c,uint32_t b){(void)r;(void)b;j->submitted=c;return (int)c;}
int tracegpu_gpu_batch_poll(tracegpu_gpu_batch_job*j){if(hardware_error)return -5;if(!batch_done)return 0;j->active=0;j->cycles=123;return 1;}
int tracegpu_render_policy_choose(tracegpu_render_policy*p,uint32_t r){(void)p;(void)r;return 0;}
void tracegpu_render_policy_observe(tracegpu_render_policy*p,uint32_t r,int g,uint32_t t){(void)p;(void)r;(void)g;(void)t;}
void XTime_GetTime(XTime *t){*t=now;}
void Xil_DCacheFlushRange(uintptr_t a,unsigned n){(void)a;(void)n;}
void Xil_DCacheInvalidateRange(uintptr_t a,unsigned n){(void)a;(void)n;}
unsigned tracegpu_video_current_buffer(void){return scan;}
void tracegpu_video_select(unsigned index){selected=index;}
void tracegpu_wave_build(tracegpu_wave_plan *p,const tracegpu_ui_state *u,const tracegpu_uart_event_ring *r)
{(void)u;(void)r;p->count=1U;p->rects[0]=(tracegpu_render_rect){32,120,729,86};}
void tracegpu_dashboard_draw_uart(uintptr_t a,const tracegpu_ui_state *u,const tracegpu_uart_event_ring *r)
{(void)a;(void)r;assert(u->gpu_wave_defer);}
void tracegpu_dashboard_draw_settings(uintptr_t a,const tracegpu_ui_state *u){(void)a;(void)u;}
void tracegpu_dashboard_set_wave_plan(const struct tracegpu_wave_plan *p){(void)p;}
uint32_t tracegpu_dashboard_cpu_wave_ticks(void){return 0U;}
void tracegpu_dashboard_draw_wave_overlay(uintptr_t a,const tracegpu_ui_state *u){(void)a;(void)u;overlay++;}
void tracegpu_dashboard_draw_touch_points(uintptr_t a,const tracegpu_touch_frame *t){(void)a;(void)t;touches++;}
int tracegpu_gpu_rect_begin(tracegpu_gpu_job *j,uintptr_t a,const tracegpu_render_rect *r)
{(void)a;(void)r;j->phase=1U;rect_started++;return 0;}
int tracegpu_gpu_convert_begin(tracegpu_gpu_job *j,uintptr_t a,uintptr_t f)
{(void)a;(void)f;j->phase=1U;j->last_state=8U;return 0;}
int tracegpu_gpu_job_poll(tracegpu_gpu_job *j)
{if(hardware_error)return -3;if(!completion)return 0;completion--;j->phase=0U;j->last_ticks=8U;return 1;}
static tracegpu_render_scheduler s;
static tracegpu_render_scheduler second;
static tracegpu_render_scheduler batch_s;
int main(void)
{
    tracegpu_ui_state ui={0};tracegpu_gpu_display_stats stats={0};
    ui.gpu_active=1U;s.wave_available=1U;s.requested=1U;
    assert(tracegpu_render_begin(&s,&ui,0,0,0,0U)==-2); /* scanout ownership */
    assert(!tracegpu_render_begin(&s,&ui,0,0,0,1U));
    assert(tracegpu_render_begin(&s,&ui,0,0,0,1U)<0);
    assert(!tracegpu_render_poll(&s,&stats)&&rect_started==1U&&overlay==0U);
    completion=1;now=10U;assert(!tracegpu_render_poll(&s,&stats)&&overlay==1U&&touches==1U);
    assert(s.phase==3U&&stats.frames_ok==0U);
    completion=1;assert(!tracegpu_render_poll(&s,&stats)&&s.phase==5U&&selected==1U);
    assert(stats.frames_ok==1U&&s.frames==0U); /* converted != displayed */
    assert(!s.completed_metrics.valid);
    scan=1U;assert(tracegpu_render_poll(&s,&stats)==1&&s.frames==1U&&s.wave_frames==1U);
    assert(s.completed_metrics.valid&&s.completed_metrics.frame_id==1U);
    assert(s.completed_metrics.wave_ticks==10U&&s.completed_metrics.convert_ticks==8U);
    assert(!tracegpu_render_begin(&s,&ui,0,0,0,0U));
    assert(s.completed_metrics.wave_ticks==10U&&s.completed_metrics.frame_id==1U);
    assert(!tracegpu_render_poll(&s,&stats));hardware_error=1;
    assert(tracegpu_render_poll(&s,&stats)<0&&s.phase==6U&&stats.frames_failed==1U);
    assert(!tracegpu_render_poll(&s,&stats)&&stats.frames_failed==1U&&selected==1U);
    assert(tracegpu_render_begin(&s,&ui,0,0,0,0U)<0);
    hardware_error=0;second.wave_available=1U;second.requested=1U;
    assert(!tracegpu_render_begin(&second,&ui,0,0,0,0U));
    assert(!tracegpu_render_poll(&second,&stats));
    completion=1;assert(!tracegpu_render_poll(&second,&stats));
    completion=1;assert(!tracegpu_render_poll(&second,&stats)&&second.phase==5U);
    now=220U;assert(tracegpu_render_poll(&second,&stats)==-7&&second.phase==6U&&second.frames==0U);
    batch_s.batch_available=batch_s.wave_available=batch_s.requested=1U;now=300;
    assert(!tracegpu_render_begin(&batch_s,&ui,0,0,0,0U));
    assert(batch_s.phase==7U&&batch_s.dirty_address==TRACEGPU_UI_CANVAS_0+120U*800U*4U);
    assert(batch_s.dirty_bytes==86U*800U*4U);
    assert(!tracegpu_render_poll(&batch_s,&stats)&&batch_s.phase==7U&&!batch_s.completed_metrics.valid);
    batch_done=1;now=310;
    assert(!tracegpu_render_poll(&batch_s,&stats)&&batch_s.phase==3U);
    completion=1;assert(!tracegpu_render_poll(&batch_s,&stats)&&batch_s.phase==5U);
    scan=0;now=320;
    assert(tracegpu_render_poll(&batch_s,&stats)==1&&batch_s.completed_metrics.pl_cycles==123);
    assert(batch_s.completed_metrics.frame_id==1U&&batch_s.completed_metrics.gpu_wave);
    puts("PASS render_scheduler: scanout ownership, overlay barrier, conversion/display separation, hardware fault and switch timeout latch");
    return 0;
}
