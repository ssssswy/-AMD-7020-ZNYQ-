#include "gpu_batch.h"
#include "board_config.h"
#include "xtime_l.h"
#include <assert.h>
#include <stdio.h>
static uint32_t regs[64],writes,commit_writes;
static XTime now;
uint32_t Xil_In32(uintptr_t a){return regs[(a-TRACEGPU_GPU_BATCH_BASE)/4U];}
void Xil_Out32(uintptr_t a,uint32_t v)
{
 unsigned n=(unsigned)((a-TRACEGPU_GPU_BATCH_BASE)/4U);regs[n]=v;writes++;
 if(n==2&&v==1)regs[1]=1;
 if(n==10){regs[11]++;commit_writes++;}
 if(n==2&&v==2){regs[1]=2;regs[12]=regs[11];regs[14]=4321;}
}
void Xil_DCacheFlushRange(uintptr_t a,unsigned n){(void)a;(void)n;}
void Xil_DCacheInvalidateRange(uintptr_t a,unsigned n){(void)a;(void)n;}
void XTime_GetTime(XTime*t){*t=now;}
int tracegpu_render_rect_valid(const tracegpu_render_rect*r){return r&&r->w&&r->h&&r->x+r->w<=800&&r->y+r->h<=480;}
int main(void)
{
 tracegpu_gpu_batch_job j={0};tracegpu_render_rect r[2]={{1,2,3,4},{799,479,1,1}};int n;
 regs[0]=TRACEGPU_GPU_BATCH_VERSION;
 assert(!tracegpu_gpu_batch_begin(&j,TRACEGPU_UI_CANVAS_0,9,2));
 n=tracegpu_gpu_batch_submit(&j,r,2,1);assert(n==1&&j.submitted==1&&!j.sealed);
 n=tracegpu_gpu_batch_submit(&j,r,2,1);assert(n==1&&j.submitted==2&&j.sealed&&commit_writes==2);
 assert(tracegpu_gpu_batch_poll(&j)==1&&!j.active&&j.cycles==4321);
 assert(regs[3]==TRACEGPU_UI_CANVAS_0&&regs[4]==800&&regs[9]==9&&writes==13);
 r[0].w=0;j.active=1;j.submitted=0;j.expected=1;assert(tracegpu_gpu_batch_submit(&j,r,1,1)==-3);
 assert(tracegpu_gpu_batch_submit(&j,r,2,1)==-4);
 regs[1]=1;j.active=1;now=j.deadline;assert(tracegpu_gpu_batch_poll(&j)==-5&&!j.active);
 puts("PASS gpu_batch driver: bounded segmented submit, packed descriptors, seal and exact completion identity");return 0;
}
