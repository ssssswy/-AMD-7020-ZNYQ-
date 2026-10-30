#include "gpu_display.h"
/* Legacy startup-only helper. v1.7 runtime uses render_scheduler/gpu_render. */
#include "board_config.h"
#include "xil_cache.h"
#include "xil_io.h"
#include "tracegpu_time.h"

#define GPU_PARAM_OFFSET(n) (4U*((n)+1U))
#define GPU_CONTROLLER_STATE 0x44U
#define GPU_COLOR_STATE 0x58U
#define GPU_COLOR_DONE 8U

static int gpu_reset(void)
{
 unsigned timeout=TRACEGPU_GPU_TIMEOUT;
 Xil_Out32(TRACEGPU_GPU_BASE,0U);
 while((Xil_In32(TRACEGPU_GPU_BASE+GPU_CONTROLLER_STATE)&0xFFU)&&timeout)--timeout;
 return timeout?0:-1;
}

int tracegpu_gpu_display_present(uintptr_t canvas,uintptr_t framebuffer,tracegpu_gpu_display_stats*stats)
{
 unsigned timeout=TRACEGPU_GPU_TIMEOUT;uint32_t state=0U;XTime begin,end;
 if(!stats)return-1;
 if(gpu_reset()){stats->frames_failed++;stats->active=0U;return-2;}
 Xil_DCacheFlushRange(canvas,TRACEGPU_UI_CANVAS_BYTES);
 Xil_DCacheInvalidateRange(framebuffer,TRACEGPU_FRAME_BYTES);
 Xil_Out32(TRACEGPU_GPU_BASE+GPU_PARAM_OFFSET(0),(uint32_t)canvas);
 Xil_Out32(TRACEGPU_GPU_BASE+GPU_PARAM_OFFSET(1),(uint32_t)framebuffer);
 Xil_Out32(TRACEGPU_GPU_BASE+GPU_PARAM_OFFSET(2),TRACEGPU_LCD_WIDTH*TRACEGPU_LCD_HEIGHT);
 Xil_Out32(TRACEGPU_GPU_BASE+GPU_PARAM_OFFSET(3),TRACEGPU_GPU_DISPLAY_FORMAT);
 XTime_GetTime(&begin);Xil_Out32(TRACEGPU_GPU_BASE,4U);
 do{
  state=Xil_In32(TRACEGPU_GPU_BASE+GPU_COLOR_STATE)&0xFFU;
  if(state==GPU_COLOR_DONE)break;
  if(state>=9U){stats->last_state=state;stats->frames_failed++;stats->active=0U;return-3;}
 }while(--timeout);
 XTime_GetTime(&end);stats->last_state=state;stats->last_ticks=(uint32_t)(end-begin);
 if(!timeout){stats->frames_failed++;stats->active=0U;return-4;}
 stats->frames_ok++;stats->active=1U;return 0;
}
