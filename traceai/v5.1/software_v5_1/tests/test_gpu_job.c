#include "gpu_render.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t registers[64];static XTime now;static unsigned writes;
void tracegpu_debug_printf(const char *f,...){(void)f;}
void tracegpu_debug_flush_startup(void){}
void XTime_GetTime(XTime *t){*t=now;}
uint32_t Xil_In32(uintptr_t a){return registers[(a-TRACEGPU_GPU_BASE)/4U];}
void Xil_Out32(uintptr_t a,uint32_t v){registers[(a-TRACEGPU_GPU_BASE)/4U]=v;writes++;}
void Xil_DCacheFlushRange(uintptr_t a,unsigned n){(void)a;(void)n;}
void Xil_DCacheInvalidateRange(uintptr_t a,unsigned n){(void)a;(void)n;}
int tracegpu_render_rect_valid(const tracegpu_render_rect *r)
{return r&&r->w&&r->h&&r->x<800U&&r->y<480U&&r->w<=800U-r->x&&r->h<=480U-r->y;}
int main(void)
{
    tracegpu_gpu_job j={0};tracegpu_render_rect r={64,120,9,86};uint32_t p[4]={1,2,3,4};
    tracegpu_gpu_render_init();
    assert(!tracegpu_gpu_rect_begin(&j,TRACEGPU_UI_CANVAS_0,&r));
    assert(tracegpu_gpu_rect_begin(&j,TRACEGPU_UI_CANVAS_0,&r)<0);
    registers[0x54U/4U]=11U;writes=0U;
    assert(!tracegpu_gpu_job_poll(&j)&&writes==0U); /* stale DONE is not a new completion */
    registers[0x54U/4U]=0U;
    assert(!tracegpu_gpu_job_poll(&j)&&registers[0]==3U);
    assert(registers[2]==0U&&registers[3]==9U&&registers[4]==86U);
    assert(registers[5]==TRACEGPU_UI_CANVAS_0+4U*(120U*800U+64U));
    assert(registers[6]==800U&&registers[8]==0x80808080U);
    assert(TRACEGPU_GPU_RENDER_ARGB==1U && registers[7]==1U);
    registers[0x54U/4U]=10U;assert(!tracegpu_gpu_job_poll(&j));
    registers[0x54U/4U]=16U;assert(!tracegpu_gpu_job_poll(&j)); /* valid rasterizer generation state */
    now=8U;registers[0x54U/4U]=11U;assert(tracegpu_gpu_job_poll(&j)==1&&j.last_ticks==8U);
    assert(!tracegpu_gpu_job_begin(&j,4U,p,4U));registers[0x58U/4U]=0U;
    assert(!tracegpu_gpu_job_poll(&j)&&registers[0]==4U);
    registers[0x58U/4U]=9U;assert(tracegpu_gpu_job_poll(&j)<0&&j.phase==3U);
    assert(tracegpu_gpu_job_begin(&j,4U,p,4U)<0);
    memset(&j,0,sizeof(j));now=10U;assert(!tracegpu_gpu_job_begin(&j,4U,p,4U));
    now=210U;assert(tracegpu_gpu_job_poll(&j)==-2&&j.phase==3U);
    memset(&j,0,sizeof(j));now=220U;
    assert(!tracegpu_gpu_convert_begin(&j,TRACEGPU_UI_CANVAS_0,TRACEGPU_FRAMEBUFFER_1));
    registers[0x44U/4U]=registers[0x58U/4U]=0U;
    assert(!tracegpu_gpu_job_poll(&j));
    assert(registers[4]==TRACEGPU_GPU_DISPLAY_FORMAT && registers[4]==4U);
    puts("PASS gpu_job: register mapping, reset handshake, busy rejection, completion, error and deadline latch");
    return 0;
}
