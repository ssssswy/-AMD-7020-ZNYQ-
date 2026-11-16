#include "render_scheduler.h"
#include <assert.h>
#include <stdio.h>
static XTime now;
static unsigned scan,selected=99,overlay,touches,flushes,invalidates,converts;
static uintptr_t flushed;
static int batch_done,hardware_error;
void XTime_GetTime(XTime *t){*t=now;}
void Xil_DCacheFlushRange(uintptr_t a,unsigned n){assert(n);flushed=a;flushes++;now++;}
void Xil_DCacheInvalidateRange(uintptr_t a,unsigned n){(void)a;assert(n);invalidates++;}
unsigned tracegpu_video_current_buffer(void){return scan;}
void tracegpu_video_select(unsigned i){assert(flushed==(i?TRACEGPU_UI_CANVAS_1:TRACEGPU_UI_CANVAS_0));selected=i;}
int tracegpu_gpu_batch_begin(tracegpu_gpu_batch_job*j,uintptr_t c,uint32_t f,uint32_t e){(void)c;j->active=1;j->frame_id=f;j->expected=e;return 0;}
int tracegpu_gpu_batch_submit(tracegpu_gpu_batch_job*j,const tracegpu_render_rect*r,uint32_t c,uint32_t b){(void)r;(void)b;j->submitted=c;return (int)c;}
int tracegpu_gpu_batch_poll(tracegpu_gpu_batch_job*j){if(hardware_error)return -5;if(!batch_done)return 0;j->active=0;j->cycles=123;return 1;}
int tracegpu_render_policy_choose(tracegpu_render_policy*p,uint32_t r){(void)p;(void)r;return 0;}
void tracegpu_render_policy_observe(tracegpu_render_policy*p,uint32_t r,int g,uint32_t t){(void)p;(void)r;(void)g;(void)t;}
void tracegpu_wave_build(tracegpu_wave_plan*p,const tracegpu_ui_state*u,const tracegpu_uart_event_ring*r){(void)u;(void)r;p->count=1;p->pixels=729*86;p->rects[0]=(tracegpu_render_rect){32,120,729,86};}
void tracegpu_dashboard_draw_uart(uintptr_t a,const tracegpu_ui_state*u,const tracegpu_uart_event_ring*r){(void)a;(void)u;(void)r;now+=4;}
void tracegpu_dashboard_draw_settings(uintptr_t a,const tracegpu_ui_state*u){(void)a;(void)u;now+=4;}
void tracegpu_dashboard_set_wave_plan(const struct tracegpu_wave_plan*p){(void)p;}
uint32_t tracegpu_dashboard_cpu_wave_ticks(void){return 2;}
void tracegpu_dashboard_draw_wave_overlay(uintptr_t a,const tracegpu_ui_state*u){(void)a;(void)u;overlay++;}
void tracegpu_dashboard_draw_touch_points(uintptr_t a,const tracegpu_touch_frame*t){(void)a;(void)t;touches++;}
int tracegpu_gpu_rect_begin(tracegpu_gpu_job*j,uintptr_t a,const tracegpu_render_rect*r){(void)a;(void)r;j->phase=1;return 0;}
int tracegpu_gpu_job_poll(tracegpu_gpu_job*j){if(hardware_error)return -3;j->phase=0;return 1;}
int tracegpu_gpu_convert_begin(tracegpu_gpu_job*j,uintptr_t a,uintptr_t f){(void)j;(void)a;(void)f;converts++;return -1;}
static tracegpu_render_scheduler cpu,gpu,failed,timeout;
void tracegpu_dashboard_draw_analysis(uintptr_t a,const tracegpu_ui_state *u){(void)a;(void)u;}
void tracegpu_freeze_resolve(tracegpu_ui_state *u,tracegpu_wave_plan *p,const tracegpu_uart_event_ring *r,int frozen,int take){(void)frozen;(void)take;tracegpu_wave_build(p,u,r);}
void tracegpu_ui_shell_draw(uintptr_t a,const tracegpu_ui_shell *s,const tracegpu_ui_state *u){(void)s;(void)u;assert(a!=(scan?TRACEGPU_UI_CANVAS_1:TRACEGPU_UI_CANVAS_0));}
int main(void){
 tracegpu_ui_state ui={0};tracegpu_gpu_display_stats stats={0};
 assert(tracegpu_render_begin(&cpu,&ui,0,0,0,0)==-2);
 assert(!tracegpu_render_begin(&cpu,&ui,0,0,0,1));
 assert(cpu.phase==2&&selected==99);
 assert(!tracegpu_render_poll(&cpu,&stats));
 assert(cpu.phase==5&&selected==1&&flushes==1&&converts==0);
 assert(cpu.ready_frames==1&&cpu.frames==0&&!cpu.completed_metrics.valid);
 assert(tracegpu_render_begin(&cpu,&ui,0,0,0,1)==-1);
 scan=1;now=22;assert(tracegpu_render_poll(&cpu,&stats)==1);
 assert(cpu.frames==1&&stats.frames_ok==1&&cpu.completed_metrics.valid);
 assert(cpu.completed_metrics.convert_ticks==0&&cpu.completed_metrics.present_flush_ticks==1);
 assert(cpu.completed_metrics.ready_ticks==5&&cpu.completed_metrics.wave_ticks==2);
 ui.gpu_active=1;gpu.wave_available=gpu.batch_available=gpu.requested=1;
 assert(!tracegpu_render_begin(&gpu,&ui,0,0,0,0));
 assert(gpu.phase==7&&gpu.dirty_bytes==86*800*4&&invalidates==1);
 assert(!tracegpu_render_poll(&gpu,&stats)&&selected==1&&overlay==0);
 batch_done=1;now=32;assert(!tracegpu_render_poll(&gpu,&stats));
 assert(gpu.phase==5&&selected==0&&overlay==1&&invalidates==2&&converts==0);
 scan=0;now=44;assert(tracegpu_render_poll(&gpu,&stats)==1);
 assert(gpu.completed_metrics.pl_cycles==123&&gpu.completed_metrics.gpu_wave);
 assert(gpu.completed_metrics.convert_ticks==0&&gpu.ready_frames==1);
 failed.wave_available=failed.batch_available=failed.requested=1;batch_done=0;
 assert(!tracegpu_render_begin(&failed,&ui,0,0,0,1));
 hardware_error=1;assert(tracegpu_render_poll(&failed,&stats)==-5);
 assert(failed.phase==6&&selected==0&&!failed.ready_frames);
 assert(!tracegpu_render_poll(&failed,&stats)&&stats.frames_failed==1);
 hardware_error=0;assert(!tracegpu_render_begin(&timeout,&ui,0,0,1,1));
 assert(!tracegpu_render_poll(&timeout,&stats)&&timeout.phase==5);
 now+=201;assert(tracegpu_render_poll(&timeout,&stats)==-7);
 assert(timeout.phase==6&&timeout.frames==0&&converts==0&&touches==3);
 puts("PASS direct scheduler: CPU without GPU, flush-before-park, no conversion, batch ownership/error barrier, immutable metrics and switch timeout");
 return 0;
}
