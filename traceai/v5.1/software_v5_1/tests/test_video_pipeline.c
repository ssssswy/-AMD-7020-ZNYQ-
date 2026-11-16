#include "video_pipeline.h"
#include "board_config.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
static uint32_t regs[128],version=TRACEGPU_DISPLAY_VERSION,flushes;
uint32_t Xil_In32(uintptr_t a){
 if(a==TRACEGPU_DISPLAY_BASE)return version;
 if(a==TRACEGPU_DISPLAY_BASE+4)return 45;
 if(a==TRACEGPU_DISPLAY_BASE+12)return 0;
 assert(a>=TRACEGPU_VDMA_BASE&&a<TRACEGPU_VDMA_BASE+sizeof regs);
 return regs[(a-TRACEGPU_VDMA_BASE)/4];
}
void Xil_Out32(uintptr_t a,uint32_t v){assert(a>=TRACEGPU_VDMA_BASE&&a<TRACEGPU_VDMA_BASE+sizeof regs);regs[(a-TRACEGPU_VDMA_BASE)/4]=(a==TRACEGPU_VDMA_BASE&&v==4)?0:v;}
void Xil_DCacheFlushRange(uintptr_t a,unsigned n){(void)a;(void)n;flushes++;}
int main(void){
 assert(!tracegpu_video_init());
 assert(regs[0x5c/4]==TRACEGPU_UI_CANVAS_0&&regs[0x60/4]==TRACEGPU_UI_CANVAS_1);
 assert(regs[0x54/4]==3200&&regs[0x58/4]==3200&&regs[0x50/4]==480);
 assert(regs[0]==1&&flushes==0);
 regs[0x28/4]=0x00010000;tracegpu_video_select(0);
 assert(regs[0x28/4]==0x00010000&&tracegpu_video_current_buffer()==1);
 tracegpu_video_select(1);assert(regs[0x28/4]==0x00010001);
 assert(tracegpu_video_stream_frames()==45&&tracegpu_video_bad_pixels()==0);
 version=0;assert(tracegpu_video_init()==-3);
 puts("PASS video pipeline: ARGB addresses, 3200-byte stride, park preserves reader index, version mismatch and stream counters");
 return 0;
}
