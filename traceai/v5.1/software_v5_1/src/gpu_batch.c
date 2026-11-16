#include "gpu_batch.h"
#include "board_config.h"
#include "xil_cache.h"
#include "xil_io.h"
#include "tracegpu_time.h"
#include "performance_monitor.h"
#include <string.h>
#define R(o) Xil_In32(TRACEGPU_GPU_BATCH_BASE+(o))
#define W(o,v) Xil_Out32(TRACEGPU_GPU_BATCH_BASE+(o),(v))
#define STATUS_BUSY 1U
#define STATUS_DONE 2U
#define STATUS_ERROR 4U
tracegpu_gpu_descriptor gpu_command_storage[TRACEGPU_GPU_COMMAND_CAPACITY]
 __attribute__((section(".gpu_commands"),aligned(64)));
static tracegpu_gpu_batch_job *list_owner;
#ifdef TRACEGPU_GPU_FORCE_FIFO
static unsigned force_fifo=1U;
#else
static unsigned force_fifo;
#endif
static void command_barrier(void){
#if defined(__arm__)
 __asm__ volatile("dsb sy" ::: "memory");
#else
 __asm__ volatile("" ::: "memory");
#endif
}
uint32_t tracegpu_gpu_batch_version(void){return R(0x00);}
uint32_t tracegpu_gpu_batch_capabilities(void){return R(0x3c);}
static unsigned known_abi(void){
 uint32_t version=tracegpu_gpu_batch_version(),caps=tracegpu_gpu_batch_capabilities();
 if(version==TRACEGPU_GPU_BATCH_VERSION&&(caps&TRACEGPU_GPU_BATCH_DMA_CAPS)==TRACEGPU_GPU_BATCH_DMA_CAPS)return 3U;
 if(version==TRACEGPU_GPU_BATCH_V2_VERSION&&(caps&TRACEGPU_GPU_BATCH_V2_CAPS)==TRACEGPU_GPU_BATCH_V2_CAPS)return 2U;
 if(version==TRACEGPU_GPU_BATCH_LEGACY_VERSION&&(caps&TRACEGPU_GPU_BATCH_LEGACY_CAPS)==TRACEGPU_GPU_BATCH_LEGACY_CAPS)return 1U;
 return 0U;
}
int tracegpu_gpu_batch_colored_available(void){return known_abi()>=2U;}
int tracegpu_gpu_batch_dma_available(void){return known_abi()==3U;}
int tracegpu_gpu_batch_force_fifo(int force){if(list_owner)return -1;force_fifo=(unsigned)(force!=0);return 0;}
int tracegpu_gpu_batch_reset_frame_perf(tracegpu_gpu_batch_job *j){
 if(!j||j->active||list_owner)return -1;
 memset(&j->frame_perf,0,sizeof j->frame_perf);return 0;
}
static void aggregate_completed(tracegpu_gpu_batch_job *j){
 tracegpu_gpu_batch_frame_perf *f=&j->frame_perf;const tracegpu_gpu_batch_perf *p=&j->perf;
 f->jobs++;f->dma_jobs+=j->dma;f->submit_ticks+=j->submit_ticks;
 f->descriptor_flush_ticks+=j->descriptor_flush_ticks;
 if(!j->abi_v2||!p->valid){f->invalid_jobs++;return;}
 if(p->queue_peak>f->queue_peak)f->queue_peak=p->queue_peak;
 f->busy_cycles+=p->busy_cycles;f->fifo_empty_cycles+=p->fifo_empty_cycles;
 f->aw_stall_cycles+=p->aw_stall_cycles;f->w_stall_cycles+=p->w_stall_cycles;
 f->b_wait_cycles+=p->b_wait_cycles;f->write_bytes+=p->write_bytes;
 f->bursts+=p->bursts;f->w_active_cycles+=p->w_active_cycles;
 f->descriptor_bytes+=p->descriptor_bytes;f->read_bursts+=p->read_bursts;
 f->read_wait_cycles+=p->read_wait_cycles;
}
int tracegpu_gpu_batch_begin(tracegpu_gpu_batch_job *j,uintptr_t canvas,uint32_t frame_id,uint32_t expected)
{return tracegpu_gpu_batch_begin_color(j,canvas,frame_id,expected,TRACEGPU_WAVE_COLOR);}
static int begin_job(tracegpu_gpu_batch_job *j,uintptr_t canvas,uint32_t frame_id,uint32_t expected,uint32_t color,int colored)
{
 uint32_t status;unsigned abi,dma;
 /* The implemented RTL accepts <=2187, not the larger eight-lane plan size. */
 if(!j||!expected||expected>2187U||list_owner)return -1;
 status=R(0x04);if(status&STATUS_BUSY)return -1;
 abi=known_abi();if(!abi||(colored&&abi<2U))return -7;
 dma=abi==3U&&!force_fifo&&expected<=TRACEGPU_GPU_COMMAND_CAPACITY;
 /* Do not write new registers to legacy hardware. MODE is a START-latched
    property on V2; explicitly restore MODE=0 for a later single-color job. */
 if(abi>=2U)W(0x44,(colored?1U:0U)|(dma?2U:0U));
 W(0x0c,(uint32_t)canvas);W(0x10,TRACEGPU_LCD_WIDTH);W(0x14,color);
 W(0x18,expected);W(0x24,frame_id);
 j->expected=expected;j->submitted=0;j->frame_id=frame_id;j->cycles=0;j->active=1;j->sealed=0;
 j->colored=(uint8_t)(colored!=0);j->abi_v2=(uint8_t)(abi>=2U);j->completed=0;
 j->dma=(uint8_t)dma;j->armed=0;j->abi_v3=(uint8_t)(abi==3U);
 j->descriptor_flush_ticks=j->submit_ticks=0;
 j->color=color;
 memset(&j->perf,0,sizeof j->perf);
 XTime_GetTime(&j->deadline);j->deadline+=COUNTS_PER_SECOND/5U;
 if(dma){list_owner=j;return 0;}
 W(0x08,1U);j->armed=1;tracegpu_perf_gpu_begin();
 if(!(R(0x04)&STATUS_BUSY)){tracegpu_perf_gpu_fault();return -2;}return 0;
}
int tracegpu_gpu_batch_begin_color(tracegpu_gpu_batch_job *j,uintptr_t canvas,uint32_t frame_id,uint32_t expected,uint32_t color)
{return begin_job(j,canvas,frame_id,expected,color,0);}
int tracegpu_gpu_batch_begin_colored(tracegpu_gpu_batch_job *j,uintptr_t canvas,uint32_t frame_id,uint32_t expected)
{return begin_job(j,canvas,frame_id,expected,TRACEGPU_WAVE_COLOR,1);}
static int submit_job(tracegpu_gpu_batch_job *j,const tracegpu_render_rect *r,const uint32_t *colors,uint32_t count,uint32_t budget,int colored)
{
 uint32_t status,level,n=0;XTime a,b;
 if(!j||!j->active)return -1;
 if(count!=j->expected)return -4;
 if(!r||colored!=(j->colored!=0)||(colored&&!colors))return -4;
 if(j->dma){
  if(list_owner!=j||j->expected>TRACEGPU_GPU_COMMAND_CAPACITY)return -8;
  XTime_GetTime(&a);
  while(j->submitted<count&&n<budget){
   unsigned i=j->submitted;if(!tracegpu_render_rect_valid(&r[i]))return -3;
   gpu_command_storage[i]=(tracegpu_gpu_descriptor){
    (uint32_t)r[i].x|((uint32_t)r[i].y<<16),
    (uint32_t)r[i].w|((uint32_t)r[i].h<<16),
    colored?colors[i]:j->color,j->frame_id};
   j->submitted++;n++;
  }
  if(j->submitted==j->expected&&!j->sealed){
   XTime f,g;XTime_GetTime(&f);
   Xil_DCacheFlushRange((uintptr_t)gpu_command_storage,(count*16U+63U)&~63U);
   command_barrier();XTime_GetTime(&g);j->descriptor_flush_ticks=(uint32_t)(g-f);
   W(0x70,TRACEGPU_GPU_COMMAND_BASE);W(0x08,1U);
   j->sealed=j->armed=1;tracegpu_perf_gpu_begin();
   status=R(0x04);
   if(!(status&(STATUS_BUSY|STATUS_DONE))||(status&STATUS_ERROR)){tracegpu_perf_gpu_fault();return -2;}
  }
  XTime_GetTime(&b);j->submit_ticks+=(uint32_t)(b-a);return (int)n;
 }
 XTime_GetTime(&a);
 while(j->submitted<count&&n<budget){
  status=R(0x04);if(status&STATUS_ERROR)return -2;
  level=(status>>19)&0x3ffU;if(level>=500U)break;
  if(!tracegpu_render_rect_valid(&r[j->submitted]))return -3;
  W(0x1c,(uint32_t)r[j->submitted].x|((uint32_t)r[j->submitted].y<<16));
  W(0x20,(uint32_t)r[j->submitted].w|((uint32_t)r[j->submitted].h<<16));
  if(colored)W(0x40,colors[j->submitted]);
  W(0x28,1U);j->submitted++;n++;
 }
 if(j->submitted==j->expected&&!j->sealed){W(0x08,2U);j->sealed=1;}
 XTime_GetTime(&b);j->submit_ticks+=(uint32_t)(b-a);
 return (int)n;
}
int tracegpu_gpu_batch_submit(tracegpu_gpu_batch_job *j,const tracegpu_render_rect *r,uint32_t count,uint32_t budget)
{return submit_job(j,r,0,count,budget,0);}
int tracegpu_gpu_batch_submit_colored(tracegpu_gpu_batch_job *j,const tracegpu_render_rect *r,const uint32_t *colors,uint32_t count,uint32_t budget)
{return submit_job(j,r,colors,count,budget,1);}
static int read_completed_perf(tracegpu_gpu_batch_job *j){
 tracegpu_gpu_batch_perf p={0};uint32_t status;
 p.status=R(0x48);
 /* The only valid V2 end states are VALID or OVERFLOW. Never publish a
    contradictory BUSY/ERROR snapshot as completed performance data. */
 if(p.status!=1U&&p.status!=2U)return -1;
 p.queue_peak=R(0x4c);p.busy_cycles=R(0x50);p.fifo_empty_cycles=R(0x54);
 p.aw_stall_cycles=R(0x58);p.w_stall_cycles=R(0x5c);p.b_wait_cycles=R(0x60);
 p.write_bytes=R(0x64);p.bursts=R(0x68);p.w_active_cycles=R(0x6c);
 if(j->abi_v3){p.descriptor_bytes=R(0x74);p.read_bursts=R(0x78);p.read_wait_cycles=R(0x7c);}
 if(j->dma&&(p.descriptor_bytes!=j->expected*16U||!p.read_bursts||p.read_bursts>j->expected))return -1;
 status=R(0x04);
 if(R(0x48)!=p.status||(status&(STATUS_BUSY|STATUS_ERROR))||!(status&STATUS_DONE)||
    R(0x24)!=j->frame_id||R(0x2c)!=j->expected||R(0x30)!=j->expected)return -1;
 p.valid=(uint8_t)(p.status==1U);p.overflow=(uint8_t)(p.status==2U);
 j->perf=p;return 0;
}
int tracegpu_gpu_batch_poll(tracegpu_gpu_batch_job *j)
{
 uint32_t status;XTime now;
 if(!j||!j->active)return -1;
 if(j->dma&&!j->armed){
  XTime_GetTime(&now);if(now>=j->deadline){tracegpu_perf_gpu_fault();j->active=0;return -5;}return 0;
 }
 status=R(0x04);
 if(status&STATUS_ERROR){tracegpu_perf_gpu_fault();j->active=0;return -2;}
 if(status&STATUS_DONE){
  if((status&STATUS_BUSY)||R(0x2c)!=j->expected||R(0x30)!=j->expected||R(0x24)!=j->frame_id){tracegpu_perf_gpu_fault();j->active=0;return -3;}
  if(j->abi_v2&&read_completed_perf(j)){tracegpu_perf_gpu_fault();j->active=0;return -6;}
  j->cycles=R(0x38);
  if(j->abi_v2&&!j->perf.valid){
   /* Saturation is a measurement failure, not a functional AXI failure.
      Close the interval while invalidating percentages and PL timings. */
   j->cycles=0;tracegpu_perf_gpu_fault();
  }
  tracegpu_perf_gpu_sample(j->cycles,1);j->active=0;j->completed=1;aggregate_completed(j);
  if(j->dma&&list_owner==j)list_owner=0;
  return 1;
 }
 XTime_GetTime(&now);
 /* Never cancel/reset a timed-out AXI writer or let CPU reuse its canvas.
    The scheduler latches FAULT and continues UART servicing instead. */
 if(now>=j->deadline){tracegpu_perf_gpu_fault();j->active=0;return -5;}
 return 0;
}
int tracegpu_gpu_batch_get_perf(const tracegpu_gpu_batch_job *j,tracegpu_gpu_batch_perf *out){
 if(!j||!out||j->active||!j->completed||!j->abi_v2)return -1;
 *out=j->perf;return j->perf.valid?1:0;
}
int tracegpu_gpu_batch_selftest(void)
{
 tracegpu_gpu_batch_job j={0};tracegpu_render_rect r={799,479,1,1};uint32_t *p=(uint32_t*)TRACEGPU_GPU_TEST_DST;XTime a,b;
 if(!known_abi())return -4;
 *p=0x11223344U;Xil_DCacheFlushRange((uintptr_t)p,4);XTime_GetTime(&a);
 if(tracegpu_gpu_batch_begin(&j,(uintptr_t)p-((479U*800U+799U)*4U),0x180U,1U))return -1;
 if(tracegpu_gpu_batch_submit(&j,&r,1,1)!=1)return -2;
 do{if(tracegpu_gpu_batch_poll(&j)<0)return -3;XTime_GetTime(&b);if(b-a>COUNTS_PER_SECOND/5U)return -5;}while(j.active);
 Xil_DCacheInvalidateRange((uintptr_t)p,4);return *p==TRACEGPU_WAVE_COLOR?0:-6;
}
