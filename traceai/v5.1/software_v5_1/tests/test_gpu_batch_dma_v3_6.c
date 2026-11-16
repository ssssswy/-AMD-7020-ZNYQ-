/* Real driver; independent non-coherent DDR descriptor and delayed GPU model.
 * A fresh process is used after each quarantine. This is not RTL/board timing. */
#include "gpu_batch.h"
#include "board_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,reads,writes,starts,seals,pushes,flushes,samples,faults,begins;
static uint32_t regs[32],status;
static XTime now=100;
static tracegpu_gpu_descriptor device_list[512];
static uint32_t pixels[800*480],reference[800*480];
static unsigned immediate_done;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
void XTime_GetTime(XTime *t){*t=++now;}
uint32_t Xil_In32(uintptr_t a){
 CHECK(a>=TRACEGPU_GPU_BATCH_BASE&&a<TRACEGPU_GPU_BATCH_BASE+128U&&!(a&3U));
 reads++;now+=5;unsigned off=(unsigned)(a-TRACEGPU_GPU_BATCH_BASE);
 return off==4U?status:regs[off/4U];
}
static void complete(void){
 CHECK(starts&&status==1U);
 for(unsigned i=0;i<regs[0x18/4];i++){
  const tracegpu_gpu_descriptor *d=&device_list[i];
  unsigned x=d->xy&65535U,y=d->xy>>16,w=d->wh&65535U,h=d->wh>>16;
  if(!w||!h||x+w>800U||y+h>480U||d->frame_id!=regs[0x24/4]){status=4;return;}
  for(unsigned row=y;row<y+h;row++)for(unsigned col=x;col<x+w;col++)pixels[row*800U+col]=d->color;
 }
 regs[0x2c/4]=regs[0x30/4]=regs[0x18/4];
 regs[0x38/4]=regs[0x50/4]=900;regs[0x48/4]=1;regs[0x4c/4]=16;
 regs[0x54/4]=20;regs[0x58/4]=30;regs[0x5c/4]=40;regs[0x60/4]=50;
 regs[0x64/4]=400;regs[0x68/4]=4;regs[0x6c/4]=100;
 regs[0x74/4]=regs[0x18/4]*16U;regs[0x78/4]=(regs[0x18/4]+15U)/16U;regs[0x7c/4]=12;
 status=2;
}
void Xil_Out32(uintptr_t a,uint32_t v){
 CHECK(a>=TRACEGPU_GPU_BATCH_BASE&&a<TRACEGPU_GPU_BATCH_BASE+128U&&!(a&3U));
 unsigned off=(unsigned)(a-TRACEGPU_GPU_BATCH_BASE);writes++;now+=5;regs[off/4]=v;
 if(off==8U&&v==1U){starts++;status=1;if(regs[0x44/4]&2U){CHECK(flushes&&regs[0x70/4]==TRACEGPU_GPU_COMMAND_BASE);if(immediate_done)complete();}}
 if(off==8U&&v==2U)seals++;
 if(off==0x28U)pushes++;
}
void Xil_DCacheFlushRange(uintptr_t a,unsigned bytes){
 CHECK(a==(uintptr_t)gpu_command_storage&&!(a&63U)&&bytes&&!(bytes&63U)&&bytes<=8192U);
 CHECK(!starts||status==2U);memcpy(device_list,gpu_command_storage,bytes);flushes++;now+=7;
}
void Xil_DCacheInvalidateRange(uintptr_t a,unsigned bytes){(void)a;(void)bytes;CHECK(!"descriptor memory must not be invalidated after CPU construction");}
void tracegpu_perf_gpu_begin(void){begins++;}
void tracegpu_perf_gpu_fault(void){faults++;}
void tracegpu_perf_gpu_sample(uint32_t cycles,int done){CHECK((cycles==900U||cycles==0U)&&done);samples++;}
int tracegpu_render_rect_valid(const tracegpu_render_rect *r){return r&&r->w&&r->h&&r->x<800U&&r->y<480U&&r->w<=800U-r->x&&r->h<=480U-r->y;}
static void init(void){regs[0]=TRACEGPU_GPU_BATCH_VERSION;regs[0x3c/4]=TRACEGPU_GPU_BATCH_DMA_CAPS;}
static void draw_reference(const tracegpu_render_rect *r,const uint32_t *colors,unsigned count){
 for(unsigned i=0;i<count;i++)for(unsigned y=r[i].y;y<(unsigned)r[i].y+r[i].h;y++)for(unsigned x=r[i].x;x<(unsigned)r[i].x+r[i].w;x++)reference[y*800U+x]=colors[i];
}
static void success(void){
 tracegpu_gpu_batch_job j={0};tracegpu_render_rect r[512];uint32_t colors[512];
 for(unsigned i=0;i<512;i++){r[i]=(tracegpu_render_rect){(uint16_t)(i%788U),(uint16_t)(i%474U),12,6};colors[i]=0xff000000U+i*23U;}
 CHECK(sizeof gpu_command_storage==8192U&&tracegpu_gpu_batch_dma_available());
 CHECK(!tracegpu_gpu_batch_begin_colored(&j,TRACEGPU_UI_CANVAS_1,0x87654321U,512));
 CHECK(j.dma&&!j.armed&&j.active&&!starts&&writes==6U);
 CHECK(tracegpu_gpu_batch_force_fifo(1)==-1&&tracegpu_gpu_batch_reset_frame_perf(&j)==-1);
 unsigned before_reads=reads;
 for(unsigned i=0;i<31;i++){CHECK(tracegpu_gpu_batch_submit_colored(&j,r,colors,512,16)==16);CHECK(!starts&&!flushes);CHECK(tracegpu_gpu_batch_poll(&j)==0);}
 CHECK(reads==before_reads);CHECK(tracegpu_gpu_batch_submit_colored(&j,r,colors,512,16)==16);
 CHECK(j.sealed&&j.armed&&starts==1&&flushes==1&&!seals&&!pushes&&writes==8U);
 for(unsigned i=0;i<512;i++)CHECK(device_list[i].xy==((uint32_t)r[i].x|((uint32_t)r[i].y<<16))&&device_list[i].wh==(12U|(6U<<16))&&device_list[i].color==colors[i]&&device_list[i].frame_id==j.frame_id);
 CHECK(!tracegpu_gpu_batch_poll(&j)&&!j.completed);draw_reference(r,colors,512);complete();
 CHECK(tracegpu_gpu_batch_poll(&j)==1&&j.completed&&!j.active&&!memcmp(pixels,reference,sizeof pixels));
 CHECK(j.frame_perf.jobs==1&&j.frame_perf.dma_jobs==1&&!j.frame_perf.invalid_jobs);
 CHECK(j.frame_perf.busy_cycles==900&&j.frame_perf.descriptor_bytes==8192&&j.frame_perf.read_bursts==32);
 CHECK(j.frame_perf.submit_ticks&&j.frame_perf.descriptor_flush_ticks);
 CHECK(!tracegpu_gpu_batch_begin_colored(&j,TRACEGPU_UI_CANVAS_1,0x9999U,1));
 CHECK(tracegpu_gpu_batch_submit_colored(&j,r,colors,1,16)==1);complete();CHECK(tracegpu_gpu_batch_poll(&j)==1);
 CHECK(j.frame_perf.jobs==2&&j.frame_perf.dma_jobs==2&&j.frame_perf.busy_cycles==1800&&j.frame_perf.descriptor_bytes==8208&&j.frame_perf.read_bursts==33);
 CHECK(samples==2&&!faults&&begins==2&&!tracegpu_gpu_batch_reset_frame_perf(&j)&&!j.frame_perf.jobs);
 CHECK(!tracegpu_gpu_batch_force_fifo(1));
 CHECK(!tracegpu_gpu_batch_begin_colored(&j,TRACEGPU_UI_CANVAS_1,10,2)&&!j.dma&&j.armed&&regs[0x44/4]==1U);
 CHECK(tracegpu_gpu_batch_submit_colored(&j,r,colors,2,16)==2&&pushes==2&&seals==1);
 puts("PASS dma success: 512 ordered noncoherent descriptors, 16 budget, 8 configuration/doorbell writes, delayed DONE, pixel oracle, aggregate all jobs, explicit FIFO");
}
static void oversized(void){
 tracegpu_gpu_batch_job j={0};CHECK(!tracegpu_gpu_batch_begin_colored(&j,TRACEGPU_UI_CANVAS_1,1,513));
 CHECK(!j.dma&&j.armed&&starts==1&&regs[0x44/4]==1U&&!flushes);
 puts("PASS dma oversize: 513..2187 retain FIFO without list overflow");
}
static void failure(const char *mode){
 tracegpu_gpu_batch_job j={0};tracegpu_render_rect r={3,4,5,6};uint32_t c=0xdeadbeefU;
 CHECK(!tracegpu_gpu_batch_begin_colored(&j,TRACEGPU_UI_CANVAS_1,55,1));
 if(!strcmp(mode,"invalid")){
  r.w=0;CHECK(tracegpu_gpu_batch_submit_colored(&j,&r,&c,1,16)==-3&&!starts&&!flushes);
 }else if(!strcmp(mode,"staged_timeout")){
  now=j.deadline;CHECK(tracegpu_gpu_batch_poll(&j)==-5&&!starts);
 }else{
  CHECK(tracegpu_gpu_batch_submit_colored(&j,&r,&c,1,16)==1);
  if(!strcmp(mode,"timeout")){now=j.deadline;CHECK(tracegpu_gpu_batch_poll(&j)==-5);}
  else if(!strcmp(mode,"read_error")){status=4;CHECK(tracegpu_gpu_batch_poll(&j)==-2);}
  else if(!strcmp(mode,"wrong_frame")){complete();regs[0x24/4]++;CHECK(tracegpu_gpu_batch_poll(&j)==-3);}
  else if(!strcmp(mode,"wrong_descriptor_count")){complete();regs[0x74/4]-=4;CHECK(tracegpu_gpu_batch_poll(&j)==-6);}
  else if(!strcmp(mode,"overflow")){complete();regs[0x48/4]=2;CHECK(tracegpu_gpu_batch_poll(&j)==1&&j.frame_perf.invalid_jobs==1&&j.completed);puts("PASS DMA saturated measurement invalidated without rejecting verified DONE");return;}
  else CHECK(!"invalid test selector");
 }
 unsigned w=writes,f=flushes;CHECK(tracegpu_gpu_batch_force_fifo(1)==-1);
 tracegpu_gpu_batch_job next={0};CHECK(tracegpu_gpu_batch_begin_colored(&next,TRACEGPU_UI_CANVAS_0,56,1)==-1);
 CHECK(writes==w&&flushes==f&&!j.completed&&!j.frame_perf.jobs);
 printf("PASS dma quarantine mode=%s no reset, no cancel, no descriptor reuse, no completion publication\n",mode);
}
int main(int argc,char **argv){
 CHECK(argc==2);init();
 if(!strcmp(argv[1],"success"))success();
 else if(!strcmp(argv[1],"oversized"))oversized();
 else if(!strcmp(argv[1],"immediate")){
  tracegpu_gpu_batch_job j={0};tracegpu_render_rect r={0,0,1,1};uint32_t c=7;immediate_done=1;
  CHECK(!tracegpu_gpu_batch_begin_colored(&j,TRACEGPU_UI_CANVAS_1,2,1));CHECK(tracegpu_gpu_batch_submit_colored(&j,&r,&c,1,16)==1);CHECK(tracegpu_gpu_batch_poll(&j)==1);puts("PASS dma immediate DONE accepted without transient BUSY assumption");
 }else failure(argv[1]);
 printf("RESULT assertions=%u reads=%u writes=%u programmed=0 board_accepted=0\n",checks,reads,writes);return 0;
}
